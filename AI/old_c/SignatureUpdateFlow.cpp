#include "SignatureUpdateFlow.h"

#include "SignatureConfig.h"

#include <cstdio>

namespace signature_scan {

namespace {

constexpr int kExitCodeSuccess = 0;
constexpr int kExitCodeSomeSignatureFailed = 1;
constexpr int kExitCodeHardError = 2;

std::string JoinPath(const char* directoryName, const char* fileName)
{
    return std::string(directoryName) + "\\" + fileName;
}

void PrintSeparator()
{
    std::printf("================================================================================\n");
}

} // namespace

// =============================== 载入镜像 ===============================

bool LoadGameImage(const char* filePath,
                   std::vector<uint8_t>& imageBuffer,
                   PortableExecutableImage& outImage,
                   std::string& outErrorMessage)
{
    outImage = PortableExecutableImage{};

    if (!ReadEntireFileIntoBuffer(filePath, imageBuffer))
    {
        outErrorMessage = std::string("读不进这个文件：") + (filePath ? filePath : "(空路径)");
        return false;
    }

    if (!outImage.ParseAsFileLayout(imageBuffer.data(), imageBuffer.size()))
    {
        outErrorMessage = std::string("这不是一个能解析的 PE 文件：") + (filePath ? filePath : "");
        return false;
    }

    const uint8_t* codeSectionData = nullptr;
    size_t codeSectionByteCount = 0;
    uintptr_t codeSectionRelativeAddress = 0;
    if (!outImage.GetExecutableCodeSection(&codeSectionData, codeSectionByteCount,
                                           codeSectionRelativeAddress))
    {
        outErrorMessage = "这个镜像里没有可执行的代码段";
        return false;
    }

    return true;
}

// =============================== 整表解析 ===============================

void ResolveEntireSignatureTable(const PortableExecutableImage& image,
                                 std::vector<ResolvedSignature>& outResolvedList)
{
    outResolvedList.clear();
    outResolvedList.reserve(::kSignatureTableCount);

    for (unsigned index = 0; index < ::kSignatureTableCount; ++index)
        outResolvedList.push_back(ResolveSignatureDefinition(image, ::kSignatureTable[index]));
}

// =============================== 打印报告 ===============================

void PrintResolveTable(const std::vector<ResolvedSignature>& resolvedList)
{
    std::printf("%-40s %-11s %-16s %s\n", "函数名", "类型", "状态", "结果");
    std::printf("--------------------------------------------------------------------------------\n");

    int okCount = 0;
    int failedCount = 0;

    for (size_t index = 0; index < resolvedList.size(); ++index)
    {
        const ResolvedSignature& resolved = resolvedList[index];
        const SignatureDefinition* definition = resolved.definition;
        if (definition == nullptr)
            continue;

        const char* signatureName = definition->signatureName != nullptr
                                        ? definition->signatureName : "(无名)";

        if (resolved.IsOk())
        {
            ++okCount;

            if (resolved.isHookSpan)
            {
                std::printf("%-40s %-11s %-16s 0x%08llX ~ 0x%08llX  (%d 字节)\n",
                            signatureName,
                            GetSignatureKindText(definition->signatureKind),
                            GetResolveStatusText(resolved.status),
                            (unsigned long long)resolved.relativeAddress,
                            (unsigned long long)resolved.boundaryEndRelativeAddress,
                            definition->boundaryLength);
            }
            else
            {
                std::printf("%-40s %-11s %-16s 0x%08llX\n",
                            signatureName,
                            GetSignatureKindText(definition->signatureKind),
                            GetResolveStatusText(resolved.status),
                            (unsigned long long)resolved.relativeAddress);
            }
        }
        else
        {
            ++failedCount;
            std::printf("%-40s %-11s %-16s %s\n",
                        signatureName,
                        GetSignatureKindText(definition->signatureKind),
                        GetResolveStatusText(resolved.status),
                        resolved.errorMessage.c_str());
        }

        // 二级扫描的诊断。两件事要盯：
        //   * 函数体外的同名命中数 —— 说明这条二级码本身不唯一，只是靠 ret 兜住
        //   * 函数体【内】第二处离第一处太近 —— 那说明 ret 都救不了它
        if (!resolved.secondaryHitRelativeAddresses.empty())
        {
            std::printf("%-40s   二级: 函数体 %08llX..%08llX  命中 %zu 处",
                        "",
                        (unsigned long long)resolved.secondaryFunctionEntryRelativeAddress,
                        (unsigned long long)resolved.secondaryFunctionEndRelativeAddress,
                        resolved.secondaryHitRelativeAddresses.size());

            for (size_t hitIndex = 0;
                 hitIndex < resolved.secondaryHitRelativeAddresses.size(); ++hitIndex)
            {
                std::printf("  0x%08llX",
                            (unsigned long long)resolved.secondaryHitRelativeAddresses[hitIndex]);
            }

            if (resolved.secondaryHitCountBeyondFunctionEnd > 0)
            {
                std::printf("   | 函数体外还有 %zu 处同名，已按 ret 排除",
                            resolved.secondaryHitCountBeyondFunctionEnd);
            }

            if (resolved.secondaryHitRelativeAddresses.size() >= 2)
            {
                const uintptr_t distance = resolved.secondaryHitRelativeAddresses[1] -
                                           resolved.secondaryHitRelativeAddresses[0];
                if (distance < 64)
                {
                    std::printf("   ★ 函数体内第二处只差 %llu 字节，ret 兜不住，"
                                "换个更长的二级码",
                                (unsigned long long)distance);
                }
            }
            std::printf("\n");
        }
    }

    std::printf("--------------------------------------------------------------------------------\n");
    std::printf(" 共 %zu 条：成功 %d，失败 %d\n",
                resolvedList.size(), okCount, failedCount);
    PrintSeparator();
}

// =============================== 更新流程 ===============================

int RunOffsetUpdateFlow(bool forceRescan)
{
    PrintSeparator();
    std::printf(" offset_update.h 更新流程%s\n", forceRescan ? "（强制重扫）" : "");
    PrintSeparator();

    // ---- 1. 游戏当前版本 ----
    GameVersionInformation currentVersion;
    const bool hasCurrentVersion =
        ReadGameVersionFromLauncherConfig(kGameVersionFilePath, currentVersion);

    if (hasCurrentVersion)
    {
        std::printf(" 游戏版本   : %s   (InnerVersion %s)\n",
                    currentVersion.clientVersion.c_str(),
                    currentVersion.innerVersion.empty() ? "-" : currentVersion.innerVersion.c_str());
    }
    else
    {
        std::printf(" 游戏版本   : 读不到 —— %s\n", currentVersion.errorMessage.c_str());
        std::printf("              当作版本变了，继续扫描\n");
    }

    // ---- 2. 上次生成的版本 ----
    const std::string outputFilePath =
        JoinPath(kOffsetUpdateOutputDirectory, kOffsetUpdateFileName);

    GameVersionInformation previousVersion;
    const bool hasPreviousVersion =
        ReadGameVersionFromGeneratedHeader(outputFilePath.c_str(), previousVersion);

    if (hasPreviousVersion)
    {
        std::printf(" 上次生成   : %s   (InnerVersion %s)\n",
                    previousVersion.clientVersion.c_str(),
                    previousVersion.innerVersion.empty() ? "-" : previousVersion.innerVersion.c_str());
    }
    else
    {
        std::printf(" 上次生成   : 没有 —— %s\n", previousVersion.errorMessage.c_str());
    }

    // ---- 3. 比版本 ----
    if (!forceRescan && hasCurrentVersion && hasPreviousVersion &&
        IsSameGameVersion(currentVersion, previousVersion))
    {
        std::printf("--------------------------------------------------------------------------------\n");
        std::printf(" 版本没变，跳过扫描。%s\n", outputFilePath.c_str());
        PrintSeparator();
        return kExitCodeSuccess;
    }

    std::printf("--------------------------------------------------------------------------------\n");

    // ---- 4. 读游戏 exe ----
    std::printf(" 正在读取 %s ...\n", kGameExecutablePath);

    std::vector<uint8_t> imageBuffer;
    PortableExecutableImage image;
    std::string loadErrorMessage;
    if (!LoadGameImage(kGameExecutablePath, imageBuffer, image, loadErrorMessage))
    {
        std::printf(" 失败：%s\n", loadErrorMessage.c_str());
        PrintSeparator();
        return kExitCodeHardError;
    }

    std::printf(" 镜像大小   : %zu 字节，ImageBase 0x%08lX\n",
                image.imageSize, (unsigned long)image.imageBaseAddress);
    std::printf("--------------------------------------------------------------------------------\n");

    // ---- 5. 整表解析 ----
    std::vector<ResolvedSignature> resolvedList;
    ResolveEntireSignatureTable(image, resolvedList);

    std::printf(" 特征码解析   [%s]\n", kGameExecutablePath);
    PrintResolveTable(resolvedList);

    // ---- 6. 写文件 ----
    OffsetUpdateWriteResult writeResult;
    if (!WriteOffsetUpdateHeader(kOffsetUpdateOutputDirectory, kOffsetUpdateFileName,
                                 currentVersion, kGameExecutablePath,
                                 image.imageBaseAddress, resolvedList, writeResult))
    {
        std::printf(" 写文件失败：%s\n", writeResult.errorMessage.c_str());
        PrintSeparator();
        return kExitCodeHardError;
    }

    std::printf(" 已写出 %s\n", writeResult.filePath.c_str());
    std::printf("   常量 %d 个（Trampoline 各算两个）\n", writeResult.writtenConstantCount);

    if (!writeResult.failedSignatureNames.empty())
    {
        std::printf("   ★ 有 %zu 条没解析出来，对应的常量【没有生成】：\n",
                    writeResult.failedSignatureNames.size());
        for (size_t index = 0; index < writeResult.failedSignatureNames.size(); ++index)
            std::printf("       %s\n", writeResult.failedSignatureNames[index].c_str());
        std::printf("     去 offsets_AI.h 修那几条特征码，然后重跑 build_tool.bat\n");
        PrintSeparator();
        return kExitCodeSomeSignatureFailed;
    }

    std::printf(" 全部成功。\n");
    PrintSeparator();
    return kExitCodeSuccess;
}

} // namespace signature_scan
