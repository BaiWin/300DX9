// 写文件用 std::fopen 就够了，不想为了 MSVC 的安全检查去引 <filesystem>。
// 引擎（SignatureScan.cpp）也是这么处理的，保持一致。
#define _CRT_SECURE_NO_WARNINGS

#include "SignatureOutputWriter.h"

#include <cstdio>
#include <ctime>

namespace signature_scan {

namespace {

// UTF-8 BOM。有了它，不管编译器开不开 /utf-8 都能把中文注释读对。
const char kUtf8ByteOrderMark[] = "\xEF\xBB\xBF";

std::string FormatCurrentLocalTime()
{
    const std::time_t currentTime = std::time(nullptr);
    std::tm localTime = std::tm{};
#if defined(_WIN32)
    localtime_s(&localTime, &currentTime);
#else
    localTime = *std::localtime(&currentTime);
#endif

    char textBuffer[64];
    std::snprintf(textBuffer, sizeof(textBuffer), "%04d-%02d-%02d %02d:%02d:%02d",
                  localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
                  localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    return textBuffer;
}

// 常量后面跟的那句说明。看一眼生成的文件就知道每个地址是什么性质。
const char* GetConstantTrailingComment(SignatureKind kind)
{
    switch (kind)
    {
    case SignatureKind::Function:   return "函数入口";
    case SignatureKind::Trampoline: return "Hook 点";
    case SignatureKind::Dword:      return "全局变量";
    }
    return "";
}

} // namespace

bool WriteOffsetUpdateHeader(const char* outputDirectoryName,
                             const char* fileName,
                             const GameVersionInformation& version,
                             const char* sourceImagePath,
                             uintptr_t imageBaseAddress,
                             const std::vector<ResolvedSignature>& resolvedList,
                             OffsetUpdateWriteResult& outResult)
{
    outResult = OffsetUpdateWriteResult{};

    if (outputDirectoryName == nullptr || fileName == nullptr)
    {
        outResult.errorMessage = "输出路径是空的";
        return false;
    }

    outResult.filePath = std::string(outputDirectoryName) + "\\" + fileName;

    std::FILE* fileHandle = std::fopen(outResult.filePath.c_str(), "wb");
    if (fileHandle == nullptr)
    {
        outResult.errorMessage = "写不开文件：" + outResult.filePath;
        return false;
    }

    std::string text;
    text.reserve(4096);

    text += kUtf8ByteOrderMark;
    text += "#pragma once\n";
    text += "// =============================================================================\n";
    text += "//  offset_update.h  —— 由 SignatureTool 自动生成，请勿手工修改\n";
    text += "// -----------------------------------------------------------------------------\n";
    text += "//  重新生成：双击 build_tool.bat（它会先编译再运行）\n";
    text += "//  特征码表：offsets_AI.h（要加英雄 / 加 hook，改那张表，不改这个文件）\n";
    text += "// -----------------------------------------------------------------------------\n";

    text += "//  GameVersion=" + (version.clientVersion.empty() ? std::string("UNKNOWN")
                                                          : version.clientVersion) + "\n";
    text += "//  InnerVersion=" + (version.innerVersion.empty() ? std::string("UNKNOWN")
                                                            : version.innerVersion) + "\n";
    text += "//  SourceImage=" + std::string(sourceImagePath != nullptr ? sourceImagePath : "(空)") + "\n";

    char numberBuffer[64];
    std::snprintf(numberBuffer, sizeof(numberBuffer), "//  ImageBase=0x%08llX\n",
                  (unsigned long long)imageBaseAddress);
    text += numberBuffer;

    text += "//  GeneratedAt=" + FormatCurrentLocalTime() + "\n";
    text += "//\n";
    text += "//  ★ 下面全部是【相对地址】。用的时候一律：\n";
    text += "//        实际地址 = 模块基址 + 这里的值\n";
    text += "//    模块基址必须现取：GetModuleHandleW(nullptr)\n";
    text += "//    游戏实际加载的基址【不是】PE 头里写的那个（dump 里是 0xA00000），\n";
    text += "//    写死 0x400000 一定错。\n";
    text += "//\n";
    text += "//  生成方式：扫 300.exe 的 .text，每条特征码都要求命中且【唯一】。\n";
    text += "// =============================================================================\n";
    text += "\n";
    text += "#include <cstdint>\n";
    text += "\n";

    // 失败的条目在顶上大声列出来。少常量是编译错误，很吵 —— 这是故意的。
    if (!resolvedList.empty())
    {
        std::string failureBlock;
        int failureCount = 0;
        for (size_t index = 0; index < resolvedList.size(); ++index)
        {
            const ResolvedSignature& resolved = resolvedList[index];
            if (resolved.IsOk() || resolved.definition == nullptr)
                continue;

            ++failureCount;
            if (failureCount == 1)
            {
                failureBlock += "// ###########################################################\n";
                failureBlock += "// ##  下面这些特征码这次没解析出来，对应的常量【没有生成】      ##\n";
                failureBlock += "// ##  去 offsets_AI.h 修那条特征码，然后重跑 build_tool.bat    ##\n";
                failureBlock += "// ###########################################################\n";
            }

            char failureBuffer[512];
            std::snprintf(failureBuffer, sizeof(failureBuffer),
                          "//  [%s] %s：%s\n",
                          GetResolveStatusText(resolved.status),
                          resolved.definition->signatureName != nullptr
                              ? resolved.definition->signatureName : "(无名)",
                          resolved.errorMessage.c_str());
            failureBlock += failureBuffer;
        }

        if (failureCount > 0)
        {
            text += failureBlock;
            text += "//\n";
        }
    }

    for (size_t index = 0; index < resolvedList.size(); ++index)
    {
        const ResolvedSignature& resolved = resolvedList[index];
        if (!resolved.IsOk() || resolved.definition == nullptr)
            continue;

        const SignatureDefinition& definition = *resolved.definition;
        const char* signatureName = definition.signatureName != nullptr
                                        ? definition.signatureName : "(无名)";

        char lineBuffer[512];

        if (resolved.isHookSpan)
        {
            std::snprintf(lineBuffer, sizeof(lineBuffer),
                          "constexpr uintptr_t %s_Start = 0x%08llX;  // %s 起点（%d 字节）\n",
                          signatureName,
                          (unsigned long long)resolved.relativeAddress,
                          GetConstantTrailingComment(definition.signatureKind),
                          definition.boundaryLength);
            text += lineBuffer;
            ++outResult.writtenConstantCount;

            std::snprintf(lineBuffer, sizeof(lineBuffer),
                          "constexpr uintptr_t %s_End = 0x%08llX;    // %s 终点（起点 + %d）\n",
                          signatureName,
                          (unsigned long long)resolved.boundaryEndRelativeAddress,
                          GetConstantTrailingComment(definition.signatureKind),
                          definition.boundaryLength);
            text += lineBuffer;
            ++outResult.writtenConstantCount;
        }
        else
        {
            std::snprintf(lineBuffer, sizeof(lineBuffer),
                          "constexpr uintptr_t %s = 0x%08llX;  // %s\n",
                          signatureName,
                          (unsigned long long)resolved.relativeAddress,
                          GetConstantTrailingComment(definition.signatureKind));
            text += lineBuffer;
            ++outResult.writtenConstantCount;
        }
    }

    const size_t writtenByteCount = std::fwrite(text.data(), 1, text.size(), fileHandle);
    const bool isWriteComplete = (writtenByteCount == text.size());
    std::fclose(fileHandle);

    if (!isWriteComplete)
    {
        outResult.errorMessage = "写文件写到一半失败了：" + outResult.filePath;
        return false;
    }

    for (size_t index = 0; index < resolvedList.size(); ++index)
    {
        const ResolvedSignature& resolved = resolvedList[index];
        if (resolved.IsOk() || resolved.definition == nullptr)
            continue;
        outResult.failedSignatureNames.push_back(
            resolved.definition->signatureName != nullptr ? resolved.definition->signatureName
                                                          : "(无名)");
    }

    outResult.isWritten = true;
    return true;
}

} // namespace signature_scan
