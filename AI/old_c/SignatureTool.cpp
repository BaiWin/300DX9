// =============================================================================
//  SignatureTool.cpp  —— 独立扫描工具（控制台）
// -----------------------------------------------------------------------------
//  用法：
//    build_tool.bat                        双击即可：编译 -> 读游戏版本 ->
//                                          版本变了就重扫 -> 覆盖 offset_update.h
//    signature_tool --force                无视版本号，强制重扫一次
//    signature_tool --matrix <镜像> ...    跨镜像对比整张表（查哪条特征码最稳）
//    signature_tool --gen <镜像> <相对地址> [最大长度]
//                                          目标在【代码】里：从该地址生成特征码
//    signature_tool --gen-global <镜像> <相对地址> [最大长度]
//                                          目标是【全局变量】：找引用它的指令，
//                                          把 imm32 通配掉
//
//  路径都在 SignatureConfig.h 里配；特征码表是 offsets_AI.h。
//
//  为什么要有独立工具（而不是只在 DLL 里扫）：
//    * 迭代速度：改一次特征码，这里是 200 毫秒一轮；
//      在 DLL 里是「改代码 -> 编译 -> 注入 -> 启游戏 -> 进对局」，几分钟一轮。
//    * 工具和 DLL 跑的是【同一份引擎代码】—— SignatureScan.cpp 只知道
//      「一块内存 + 长度」，谁喂给它都一样。所以工具里验证通过的特征码，
//      在 DLL 里必然成立。
//    * 可以一次对比多个历史版本，一眼看出哪条特征码跨版本稳定。
//      这件事在运行时做不到（你不可能同时跑 8 个版本的游戏）。
//
//  ★ 镜像用【磁盘上的 300.exe】就行，不需要 dump。
//    实测同一个 build 的磁盘 exe 和内存 dump：PE 时间戳相同、入口点相同，
//    .text 逐字节比过，差异全是镜像基址重定位（0x400000 vs 0xA00000）。
//    而且磁盘 exe 更干净 —— dump 里带着注入留下的补丁
//    （.text 里有一处 5 字节的 inline jmp，跳到模块外去了）。


#include "SignatureScan.h"
#include "SignatureConfig.h"
#include "SignatureDefinitionResolve.h"
#include "SignatureUpdateFlow.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

// 只为了 SetConsoleOutputCP —— 中文报告在 GBK 控制台里会变乱码。
// 引擎本身（SignatureScan.*）刻意不依赖 Windows，这里只在工具入口用一下。
// NOMINMAX 是为了不让 windows.h 的 min/max 宏污染标准库。
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace signature_scan;

// -----------------------------------------------------------------------------
static void PrintUsageInstructions()
{
    std::printf(
        "signature_tool —— 300 特征码扫描工具\n"
        "\n"
        "  （不带参数）                            更新流程：读版本 -> 变了就重扫 ->\n"
        "                                          覆盖 offset_update.h\n"
        "  --force                                无视版本号，强制重扫一次\n"
        "  --matrix <镜像1> <镜像2> ...            跨镜像对比整张表\n"
        "  --gen <镜像> <相对地址> [最大长度]       目标在代码里：从该地址取码\n"
        "  --gen-global <镜像> <相对地址> [最大长度]\n"
        "                                         目标是全局变量：找引用它的指令\n"
        "\n"
        "  镜像 = 磁盘上的 300.exe 就行，不需要 dump。\n"
        "\n"
        "  --gen 和 --gen-global 的区别：\n"
        "    目标住在 .text（代码）里       -> --gen\n"
        "    目标住在 .data/.rdata（数据）里 -> --gen-global（全局变量都是这种）\n"
        "\n"
        "  路径在 SignatureConfig.h 里配，改完重跑 build_tool.bat。\n"
        "  特征码表是 offsets_AI.h。\n");
}

// -----------------------------------------------------------------------------
static bool LoadImageFromFile(const char* filePath,
                              std::vector<uint8_t>& fileBuffer,
                              PortableExecutableImage& image)
{
    if (!ReadEntireFileIntoBuffer(filePath, fileBuffer))
    {
        std::printf("[错误] 读不到文件: %s\n", filePath);
        return false;
    }
    if (!image.ParseAsFileLayout(fileBuffer.data(), fileBuffer.size()))
    {
        std::printf("[错误] PE 解析失败: %s\n", filePath);
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  两种 --gen 共用的零件
// -----------------------------------------------------------------------------
//  至少要几个实字节才算「够结实」。全 ?? 的码等于没写。
static const int kMinimumExactByteCount = 8;

// 稳定化时允许「往外多读」几个字节。
//
// 为什么需要这个：窗口的最后一两条指令可能正好跨在窗口边界上。只按 windowLength
// 去读，就会读到一个残缺的 4 字节操作数，算出来的目标地址是垃圾，于是那 4 个字节
// 被当成实字节留在特征码里 —— 可它们其实是随版本变的。
// 典型症状：生成的码末尾挂着 "E8 48 CC" 这样的半截 call 位移，本版本能命中，
// 换个版本立刻 MISS（因为 call 点和目标之间的距离变了）。
//
// 所以：按 windowLength + 3 去【读】，但只往窗口内的位置【写】通配。
// 多出来的 3 字节只用来把跨界的那条指令看完整。
static const int kStabilizationReadAheadByteCount = 3;

// 把窗口里「看起来会随版本变」的 4 字节通配掉：
//   (a) 落在镜像内的绝对地址（ImageBase + 相对地址）
//   (b) 解出来落在镜像内的 rel32 相对偏移
// 这样生成的码天然跨版本可用。
//
// readableByteCount = 从 windowStartPointer 起还有多少字节可以安全读。
static void StabilizeWindow(const PortableExecutableImage& image,
                            const uint8_t* windowStartPointer,
                            size_t readableByteCount,
                            uint32_t windowStartRelativeAddress,
                            int windowLength,
                            std::vector<uint8_t>& comparisonMask)
{
    size_t readLength = (size_t)windowLength + kStabilizationReadAheadByteCount;
    if (readLength > readableByteCount)
        readLength = readableByteCount;

    for (size_t byteOffset = 0; byteOffset + 4 <= readLength; ++byteOffset)
    {
        uint32_t fourBytes;
        std::memcpy(&fourBytes, windowStartPointer + byteOffset, 4);

        bool looksLikeAddress = false;

        // (a) 绝对地址：ImageBase + 相对地址
        if (image.imageBaseAddress && fourBytes >= image.imageBaseAddress &&
            image.IsAddressInsideImage(fourBytes - image.imageBaseAddress))
        {
            looksLikeAddress = true;
        }

        // (b) rel32：从这条指令末尾算出来的目标落在镜像里
        if (!looksLikeAddress)
        {
            const int64_t targetAddress =
                (int64_t)(windowStartRelativeAddress + byteOffset + 4) +
                (int32_t)fourBytes;
            if (targetAddress > 0 &&
                image.IsAddressInsideImage((uintptr_t)targetAddress))
            {
                looksLikeAddress = true;
            }
        }

        if (looksLikeAddress)
        {
            for (int wildcardIndex = 0; wildcardIndex < 4; ++wildcardIndex)
            {
                const size_t position = byteOffset + (size_t)wildcardIndex;
                // 跨界的那条指令只有落在窗口内的字节才写进掩码。
                // 没写到的那些字节根本不在特征码里，等于天然通配。
                if (position < comparisonMask.size())
                    comparisonMask[position] = 0;
            }
        }
    }
}

// 把 expectedBytes + comparisonMask 组成 "E8 ?? ?? 8B 0D" 形式的字符串
static std::string BuildPatternText(const uint8_t* expectedBytes,
                                    const std::vector<uint8_t>& comparisonMask,
                                    int windowLength,
                                    int& outExactByteCount,
                                    int& outWildcardByteCount)
{
    std::string patternText;
    char        byteText[8];
    outExactByteCount    = 0;
    outWildcardByteCount = 0;

    for (int byteIndex = 0; byteIndex < windowLength; ++byteIndex)
    {
        if (comparisonMask[byteIndex])
        {
            std::snprintf(byteText, sizeof(byteText), "%02X", expectedBytes[byteIndex]);
            ++outExactByteCount;
        }
        else
        {
            std::snprintf(byteText, sizeof(byteText), "??");
            ++outWildcardByteCount;
        }
        if (!patternText.empty())
            patternText += ' ';
        patternText += byteText;
    }

    return patternText;
}

// 一个「生成出来的候选」。--gen 和 --gen-global 都产出它。
struct GeneratedSignatureCandidate
{
    bool        isValid              = false;
    uint32_t    startRelativeAddress = 0;  // 特征码起点
    int         windowLength         = 0;
    int         exactByteCount       = 0;
    int         wildcardByteCount    = 0;
    int32_t     addressAdjustment    = 0;  // 起点 -> 有效地址（--gen 用）
    int32_t     immediateValueOffset = 0;  // 仅 --gen-global：imm32 相对有效地址的偏移
    std::string patternText;
};

// 排序规则：通配位越少越好；一样少就取短一点、地址靠前的。
// 返回 true 表示 firstCandidate 优于 secondCandidate。
static bool IsBetterCandidate(const GeneratedSignatureCandidate& firstCandidate,
                              const GeneratedSignatureCandidate& secondCandidate)
{
    if (!secondCandidate.isValid) return true;
    if (!firstCandidate.isValid)  return false;
    if (firstCandidate.wildcardByteCount != secondCandidate.wildcardByteCount)
        return firstCandidate.wildcardByteCount < secondCandidate.wildcardByteCount;
    if (firstCandidate.windowLength != secondCandidate.windowLength)
        return firstCandidate.windowLength < secondCandidate.windowLength;
    return firstCandidate.startRelativeAddress < secondCandidate.startRelativeAddress;
}

// -----------------------------------------------------------------------------
//  模式一：单镜像详细报告
// -----------------------------------------------------------------------------
//  跨镜像对比：同一张表，N 个镜像各扫一遍，横向看哪条最稳
// -----------------------------------------------------------------------------
//  这是独立工具最有价值的功能 —— 运行时做不到（你不可能同时跑 8 个版本的游戏）。
//  一眼看出：哪条特征码在所有历史版本里都 OK，哪条已经在某个版本挂了。
static const char* GetShortResolveStatusText(SignatureResolveStatus status)
{
    switch (status)
    {
    case SignatureResolveStatus::Ok:                       return "";
    case SignatureResolveStatus::InvalidPattern:           return "PATTERN";
    case SignatureResolveStatus::ImageHasNoCodeSection:    return "NO-CODE";
    case SignatureResolveStatus::NotFound:                 return "MISS";
    case SignatureResolveStatus::NotUnique:                return "NOT-UNIQ";
    case SignatureResolveStatus::NotACallInstruction:      return "NO-CALL";
    case SignatureResolveStatus::NotAReferenceInstruction: return "NO-REF";
    case SignatureResolveStatus::BadBoundaryLength:        return "BAD-BND";
    case SignatureResolveStatus::SecondaryNotFound:        return "2ND-MISS";
    case SignatureResolveStatus::SecondaryNotUnique:       return "2ND-MULT";
    case SignatureResolveStatus::AddressOutOfCodeSection:  return "OUT-CODE";
    }
    return "UNKNOWN";
}

struct CrossImageResult
{
    std::string                    imageName;
    std::vector<ResolvedSignature> resolvedList;
};

static int RunCrossVersionMatrix(std::vector<const char*>& filePaths)
{
    std::vector<CrossImageResult> imageResults;
    imageResults.reserve(filePaths.size());

    for (size_t fileIndex = 0; fileIndex < filePaths.size(); ++fileIndex)
    {
        std::vector<uint8_t> imageBuffer;
        PortableExecutableImage image;
        std::string loadErrorMessage;
        if (!LoadGameImage(filePaths[fileIndex], imageBuffer, image, loadErrorMessage))
        {
            std::printf("[错误] %s：%s\n", filePaths[fileIndex], loadErrorMessage.c_str());
            return 2;
        }

        CrossImageResult imageResult;
        imageResult.imageName = filePaths[fileIndex];
        ResolveEntireSignatureTable(image, imageResult.resolvedList);
        imageResults.push_back(imageResult);
    }

    const int kImageColumnWidth = 10;

    std::printf("列号对照：\n");
    for (size_t fileIndex = 0; fileIndex < imageResults.size(); ++fileIndex)
        std::printf("  [%zu] %s\n", fileIndex, imageResults[fileIndex].imageName.c_str());
    std::printf("\n");

    // ---- 矩阵本体：一行一条特征码，一列一个镜像 ----
    std::printf("%-40s", "函数名");
    for (size_t fileIndex = 0; fileIndex < imageResults.size(); ++fileIndex)
        std::printf("  [%zu]    ", fileIndex);
    std::printf("\n");
    std::printf("--------------------------------------------------------------------------------\n");

    const size_t entryCount = ::kSignatureTableCount;
    for (size_t entryIndex = 0; entryIndex < entryCount; ++entryIndex)
    {
        const SignatureDefinition& definition = ::kSignatureTable[entryIndex];
        std::printf("%-40s",
                    definition.signatureName != nullptr ? definition.signatureName : "(无名)");

        for (size_t fileIndex = 0; fileIndex < imageResults.size(); ++fileIndex)
        {
            const std::vector<ResolvedSignature>& resolvedList =
                imageResults[fileIndex].resolvedList;

            if (entryIndex >= resolvedList.size())
            {
                std::printf("%-*s", kImageColumnWidth, "?");
                continue;
            }

            const ResolvedSignature& resolved = resolvedList[entryIndex];
            if (resolved.IsOk())
            {
                char addressBuffer[32];
                std::snprintf(addressBuffer, sizeof(addressBuffer), "%08llX",
                              (unsigned long long)resolved.relativeAddress);
                std::printf("%-*s", kImageColumnWidth, addressBuffer);
            }
            else
            {
                std::printf("%-*s", kImageColumnWidth,
                            GetShortResolveStatusText(resolved.status));
            }
        }
        std::printf("\n");
    }

    std::printf("--------------------------------------------------------------------------------\n");

    // ---- 稳定性小结：直接回答「哪条最稳」 ----
    std::printf(" 稳定性小结\n");
    for (size_t entryIndex = 0; entryIndex < entryCount; ++entryIndex)
    {
        const SignatureDefinition& definition = ::kSignatureTable[entryIndex];
        const char* signatureName = definition.signatureName != nullptr
                                        ? definition.signatureName : "(无名)";

        int okCount = 0;
        std::string brokenImageNames;
        for (size_t fileIndex = 0; fileIndex < imageResults.size(); ++fileIndex)
        {
            const std::vector<ResolvedSignature>& resolvedList =
                imageResults[fileIndex].resolvedList;
            if (entryIndex < resolvedList.size() && resolvedList[entryIndex].IsOk())
            {
                ++okCount;
            }
            else
            {
                if (!brokenImageNames.empty())
                    brokenImageNames += ", ";
                brokenImageNames += imageResults[fileIndex].imageName;
            }
        }

        std::printf("   %-40s %d/%zu 个镜像", signatureName, okCount, imageResults.size());
        if (brokenImageNames.empty())
            std::printf("   OK\n");
        else
            std::printf("   挂过：%s\n", brokenImageNames.c_str());
    }

    return 0;
}
// -----------------------------------------------------------------------------
static int GenerateSignatureForAddress(const char* filePath,
                                       uint32_t targetRelativeAddress,
                                       int maximumPatternLength)
{
    std::vector<uint8_t> fileBuffer;
    PortableExecutableImage image;
    if (!LoadImageFromFile(filePath, fileBuffer, image))
        return 1;

    const uint8_t* codeSectionData    = nullptr;
    size_t         codeSectionSize    = 0;
    uintptr_t      codeSectionAddress = 0;
    if (!image.GetExecutableCodeSection(&codeSectionData, codeSectionSize,
                                        codeSectionAddress))
    {
        std::printf("[错误] 找不到 .text\n");
        return 1;
    }

    if (targetRelativeAddress < codeSectionAddress ||
        targetRelativeAddress >= codeSectionAddress + codeSectionSize)
    {
        std::printf("[错误] 相对地址 0x%X 不在 .text (0x%X..0x%X) 里。\n",
                    targetRelativeAddress, (unsigned)codeSectionAddress,
                    (unsigned)(codeSectionAddress + codeSectionSize));
        std::printf("       如果这是全局变量（.data/.rdata 里的），"
                    "应该用 --gen-global。\n");
        return 1;
    }

    // 窗口起点最多往后滑这么多字节找更好的码
    const int kMaximumWindowSlide = 64;

    std::printf("从相对地址 0x%X 生成特征码（最长 %d 字节，起点可后滑 %d 字节）\n\n",
                targetRelativeAddress, maximumPatternLength, kMaximumWindowSlide);

    GeneratedSignatureCandidate bestCandidate;

    for (int windowSlide = 0; windowSlide <= kMaximumWindowSlide; ++windowSlide)
    {
        const uint32_t startRelativeAddress =
            targetRelativeAddress + (uint32_t)windowSlide;
        const size_t startOffset = (size_t)(startRelativeAddress - codeSectionAddress);
        if (startOffset >= codeSectionSize)
            break;

        const uint8_t* windowStartPointer = codeSectionData + startOffset;
        const size_t   availableByteCount = codeSectionSize - startOffset;

        for (int windowLength = 8; windowLength <= maximumPatternLength; ++windowLength)
        {
            if ((size_t)windowLength > availableByteCount)
                break;

            std::vector<uint8_t> comparisonMask(windowLength, 1);
            StabilizeWindow(image, windowStartPointer, availableByteCount,
                            startRelativeAddress,
                            windowLength, comparisonMask);

            // 开头就是通配位的码没有意义：命中地址会飘到任何满足后几字节的地方，
            // 后面的 addressAdjustment 也就无从谈起。直接跳过。
            if (comparisonMask[0] == 0)
                continue;

            int exactByteCount    = 0;
            int wildcardByteCount = 0;
            const std::string patternText =
                BuildPatternText(windowStartPointer, comparisonMask, windowLength,
                                 exactByteCount, wildcardByteCount);

            if (exactByteCount < kMinimumExactByteCount)
                continue;

            // ---- 唯一性检查 ----
            SignaturePattern pattern = ParseSignaturePattern(patternText.c_str());
            if (!pattern.isValid)
                continue;

            std::vector<SignatureHit> hits;
            ScanMemoryBuffer(codeSectionData, codeSectionSize, codeSectionAddress,
                             pattern, hits, 8);

            if (windowSlide == 0)
            {
                std::printf("  起点 +0  长度 %2d : 命中 %d 处%s\n",
                            windowLength, (int)hits.size(),
                            hits.size() == 1 ? "   <-- 唯一" : "");
            }

            if (hits.size() != 1)
                continue;

            GeneratedSignatureCandidate candidate;
            candidate.isValid              = true;
            candidate.startRelativeAddress = startRelativeAddress;
            candidate.windowLength         = windowLength;
            candidate.exactByteCount       = exactByteCount;
            candidate.wildcardByteCount    = wildcardByteCount;
            candidate.patternText          = patternText;
            // addressAdjustment 的方向是「命中点 -> 目标」，
            // 所以起点在目标之后时它是负的。
            candidate.addressAdjustment =
                -(int32_t)(startRelativeAddress - targetRelativeAddress);

            if (IsBetterCandidate(candidate, bestCandidate))
                bestCandidate = candidate;

            break;   // 这个起点已经找到最短的唯一码，换下一个起点
        }
    }

    if (!bestCandidate.isValid)
    {
        std::printf("\n[失败] 没找到可用特征码"
                    "（要求：开头是实字节、实字节 >= %d、全镜像唯一）。\n",
                    kMinimumExactByteCount);
        std::printf("       建议：把目标改成「调用它的那条 call 指令」，"
                    "用 FollowCallInstruction 反解；\n");
        std::printf("             或者手工去 x64dbg 抄函数序言。\n");
        return 2;
    }

    const int32_t windowSlide =
        (int32_t)bestCandidate.startRelativeAddress - (int32_t)targetRelativeAddress;

    std::printf("\n生成成功（长度 %d，实字节 %d，通配 %d，起点在目标 %+d 字节处）。\n\n",
                bestCandidate.windowLength, bestCandidate.exactByteCount,
                bestCandidate.wildcardByteCount, windowSlide);
    std::printf("可直接粘进 SignatureTable.cpp：\n\n");
    std::printf("    {\n");
    std::printf("        \"<起个语义名>\",\n");
    std::printf("        \"%s\",\n", bestCandidate.patternText.c_str());
    std::printf("        SignatureResolveKind::TargetIsHitAddress, 0, %d, 0x%X,\n",
                bestCandidate.addressAdjustment, targetRelativeAddress);
    std::printf("        \"<中文备注>\"\n");
    std::printf("    },\n\n");

    if (bestCandidate.addressAdjustment == 0)
    {
        std::printf("起点就是目标，addressAdjustment = 0，直接用。\n");
    }
    else
    {
        std::printf("注意：特征码起点在目标之后 %d 字节(0x%X) 处，"
                    "所以 addressAdjustment = %d 指回目标。\n"
                    "      窗口之所以要往后滑，是因为目标开头那几字节长得像镜像内地址，\n"
                    "      被稳定化处理吃掉了。这通常意味着「目标不是函数序言」——\n"
                    "      函数序言很少一上来就是绝对地址。建议去 x64dbg 核对一下。\n"
                    "      另外窗口落在函数体中间，游戏改了这段函数体就会失效，\n"
                    "      能用，但不如「直接抄函数序言」稳。\n",
                    windowSlide, windowSlide, bestCandidate.addressAdjustment);
    }

    std::printf("\n提示：如果目标其实是一条 call（想拿函数地址而不是调用点），\n");
    std::printf("      把 SignatureResolveKind::TargetIsHitAddress "
                "换成 SignatureResolveKind::FollowCallInstruction。\n");
    std::printf("      如果目标是全局变量（住在 .data 里），要用 --gen-global。\n");
    return 0;
}

// -----------------------------------------------------------------------------
//  模式四：目标在【数据】里（全局变量）—— 找引用它的那条指令
// -----------------------------------------------------------------------------
//  为什么不能直接用 --gen：
//    全局变量住在 .data/.rdata 里，代码段里根本没有它的字节，
//    --gen 会报「相对地址不在 .text 里」。这是正常的，不是 bug。
//
//  正确做法：
//    代码里访问全局变量是绝对寻址，机器码里直接写着那个地址：
//        mov eax, [0x246A4B8]   ->  A1 B8 A4 46 02
//        mov ecx, [0x246A4B8]   ->  8B 0D B8 A4 46 02
//        call dword ptr [xxx]   ->  FF 15 xx xx xx xx
//    所以拿【引用它的那条指令】做特征码，把 imm32 通配掉，
//    运行时读出来的就是本版本的地址 —— 版本无关。
//
//  这个模式做的事：
//    1) 把相对地址换算成绝对地址（ImageBase + 相对地址）
//    2) 在 .text 里搜这个 4 字节值的所有出现位置（每一处都是一条引用指令）
//    3) 对每处，向前试 1 字节和 2 字节两种「指令开头」，
//       向后开窗，把 imm32 通配掉，找一条唯一的
//    4) 输出最好的那条 + 其余备选引用点
//
//  为什么要输出备选：一条全局变量常有七八处引用，哪一处最稳只有跨版本
//  跑 --matrix 才知道。给几个备选，挂了好换。
// -----------------------------------------------------------------------------

// imm32 相对指令开头的偏移，可能是这几个值。
//   1 字节操作码：A1(mov eax,[m])、A3(mov [m],eax)
//   2 字节操作码：8B/89/8A/88/8D/C7/F7/FF + ModRM(mod=00,rm=101)
//                 —— 这一族就是「寄存器 <-> 绝对地址」的全部形式，
//                    含 FF 15(call [m])、FF 25(jmp [m])、FF 35(push [m])
//   3 字节：66 A1 / 66 A3（带操作数尺寸前缀）
//   4 字节：F3/F2 0F 10/11 (movss/movsd) + ModRM(mod=00,rm=101)
static const int kImmediateLeadByteChoices[] = { 1, 2, 3, 4 };

// 判断 imm32 前面那几个字节像不像真的是一条「绝对寻址」指令的开头。
//
// 为什么需要这道校验：在 21MB 的 .text 里裸搜一个 4 字节值，会撞上大量巧合
// （浮点常量的字节、长指令中间的位移、跳转表…）。实测搜 dword_HoverStruct
// 会命中 270 处，但真正是引用的只有十来处。不筛掉的话：
//   * 报出来的「引用数」是假的大数字
//   * 选中的「最优候选」可能是个巧合字节序列 —— 它碰巧也能读出正确地址，
//     但游戏一更新这段字节就变，跨版本稳定性没保障
// 筛掉之后剩下的，才是能长期用的。
static bool LooksLikeAbsoluteAddressingInstruction(const uint8_t* instructionBytes,
                                                   int immediateValueOffset)
{
    switch (immediateValueOffset)
    {
    case 1:
        // A1 = mov eax, [imm32]     A3 = mov [imm32], eax
        return instructionBytes[0] == 0xA1 || instructionBytes[0] == 0xA3;

    case 2:
    {
        // 66 A1 / 66 A3 —— 带操作数尺寸前缀的 moffs 绝对寻址。
        // 66 只把 eax 换成 ax，【地址宽度不变】，所以 imm32 仍然在 +2。
        if (instructionBytes[0] == 0x66 &&
            (instructionBytes[1] == 0xA1 || instructionBytes[1] == 0xA3))
            return true;

        // ModRM 的 mod=00、rm=101 表示「disp32 绝对寻址」，
        // 满足 (modrm & 0xC7) == 0x05 的字节恰好是 05/0D/15/1D/25/2D/35/3D
        if ((instructionBytes[1] & 0xC7) != 0x05)
            return false;

        switch (instructionBytes[0])
        {
        case 0x88:  // mov [imm32], r8
        case 0x89:  // mov [imm32], r32
        case 0x8A:  // mov r8, [imm32]
        case 0x8B:  // mov r32, [imm32]
        case 0x8D:  // lea r32, [imm32]
        case 0xC7:  // mov dword ptr [imm32], imm32
        case 0xF7:  // test dword ptr [imm32], imm32
        case 0xFF:  // inc/dec/call/jmp/push dword ptr [imm32]
            return true;
        default:
            return false;
        }
    }

    case 3:
    {
        // 66 前缀 + ModRM 绝对寻址：66 8B 0D <imm32> 这样的形式。
        // 比 case 2 多了一个前缀字节，所以 imm32 顺延到 +3。
        if (instructionBytes[0] != 0x66)
            return false;
        if ((instructionBytes[2] & 0xC7) != 0x05)
            return false;

        switch (instructionBytes[1])
        {
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8D:
        case 0xC7:
        case 0xF7:
        case 0xFF:
            return true;
        default:
            return false;
        }
    }

    case 4:
        // F3 0F 10 modrm = movss xmm, [imm32]
        // F3 0F 11 modrm = movss [imm32], xmm     （F2 同理，是 movsd）
        return (instructionBytes[0] == 0xF3 || instructionBytes[0] == 0xF2) &&
               instructionBytes[1] == 0x0F &&
               (instructionBytes[2] == 0x10 || instructionBytes[2] == 0x11) &&
               (instructionBytes[3] & 0xC7) == 0x05;

    default:
        return false;
    }
}

static int GenerateGlobalVariableSignature(const char* filePath,
                                           uint32_t targetRelativeAddress,
                                           int maximumPatternLength)
{
    std::vector<uint8_t> fileBuffer;
    PortableExecutableImage image;
    if (!LoadImageFromFile(filePath, fileBuffer, image))
        return 1;

    const uint8_t* codeSectionData    = nullptr;
    size_t         codeSectionSize    = 0;
    uintptr_t      codeSectionAddress = 0;
    if (!image.GetExecutableCodeSection(&codeSectionData, codeSectionSize,
                                        codeSectionAddress))
    {
        std::printf("[错误] 找不到 .text\n");
        return 1;
    }

    if (image.imageBaseAddress == 0)
    {
        std::printf("[错误] 这个镜像没有 ImageBase（裸内存镜像），"
                    "无法换算指令里的绝对地址。\n");
        return 1;
    }

    // 指令里编码的是【绝对地址】，不是相对地址。这一步不能漏。
    const uint32_t targetAbsoluteAddress =
        (uint32_t)image.imageBaseAddress + targetRelativeAddress;

    std::printf("从全局变量相对地址 0x%X 生成特征码\n", targetRelativeAddress);
    std::printf("  绝对地址 = ImageBase(0x%X) + 0x%X = 0x%X\n",
                image.imageBaseAddress, targetRelativeAddress, targetAbsoluteAddress);
    std::printf("  在 .text 里搜这个 4 字节值（最长 %d 字节）...\n\n",
                maximumPatternLength);

    // ---- 1) 找出所有引用点（imm32 所在的位置）----
    std::vector<size_t> immediateOffsets;   // 相对 codeSectionData 的偏移
    {
        const uint8_t needle[4] = {
            (uint8_t)(targetAbsoluteAddress & 0xFF),
            (uint8_t)((targetAbsoluteAddress >> 8) & 0xFF),
            (uint8_t)((targetAbsoluteAddress >> 16) & 0xFF),
            (uint8_t)((targetAbsoluteAddress >> 24) & 0xFF),
        };

        for (size_t offset = 0; offset + 4 <= codeSectionSize; ++offset)
        {
            if (codeSectionData[offset] == needle[0] &&
                std::memcmp(codeSectionData + offset, needle, 4) == 0)
            {
                immediateOffsets.push_back(offset);
            }
        }
    }

    if (immediateOffsets.empty())
    {
        std::printf("[失败] .text 里没有任何指令引用 0x%X。\n", targetAbsoluteAddress);
        std::printf("       可能原因：\n");
        std::printf("         * 这个相对地址已经过期了（游戏更新后挪走了）\n");
        std::printf("         * 它不是全局变量，而是某个结构体的成员偏移\n");
        std::printf("         * 它是通过寄存器算出来的地址，指令里不出现立即数\n");
        std::printf("           （这种只能找「算它的那个函数」，用 --gen 定位函数）\n");
        return 2;
    }

    // ---- 2) 用操作码校验把巧合的字节序列筛掉 ----
    //      每一处确认的引用 = (imm32 的位置, imm32 相对指令开头的偏移)
    struct ConfirmedReference
    {
        size_t immediateOffset;
        int    immediateValueOffset;
    };
    std::vector<ConfirmedReference> references;

    for (size_t matchIndex = 0; matchIndex < immediateOffsets.size(); ++matchIndex)
    {
        const size_t immediateOffset = immediateOffsets[matchIndex];

        for (size_t choiceIndex = 0;
             choiceIndex < sizeof(kImmediateLeadByteChoices) / sizeof(int);
             ++choiceIndex)
        {
            const int immediateValueOffset = kImmediateLeadByteChoices[choiceIndex];

            if (immediateOffset < (size_t)immediateValueOffset)
                continue;   // 指令开头跑到段外面去了

            if (!LooksLikeAbsoluteAddressingInstruction(
                    codeSectionData + immediateOffset - immediateValueOffset,
                    immediateValueOffset))
            {
                continue;   // 前面那几个字节不像指令开头，多半是巧合
            }

            ConfirmedReference reference;
            reference.immediateOffset      = immediateOffset;
            reference.immediateValueOffset = immediateValueOffset;
            references.push_back(reference);
        }
    }

    if (references.empty())
    {
        std::printf("[失败] 有 %zu 处字节匹配，但没有一处的操作码像「绝对寻址」指令。\n",
                    immediateOffsets.size());
        std::printf("       可能这个地址是被寄存器算出来的，指令里不出现立即数 ——\n");
        std::printf("       只能去找「计算它的那个函数」，用 --gen 定位那个函数。\n");
        return 2;
    }

    std::printf("字节匹配 %zu 处，其中 %zu 处经操作码确认是真正的引用指令。\n\n",
                immediateOffsets.size(), references.size());

    // ---- 3) 每处引用各试几种开窗，收集候选 ----
    std::vector<GeneratedSignatureCandidate> allCandidates;

    for (size_t referenceIndex = 0; referenceIndex < references.size();
         ++referenceIndex)
    {
        const size_t immediateOffset      = references[referenceIndex].immediateOffset;
        const int    immediateValueOffset = references[referenceIndex].immediateValueOffset;

        const size_t startOffset   = immediateOffset - (size_t)immediateValueOffset;
        const uint32_t startRelativeAddress =
            (uint32_t)(codeSectionAddress + startOffset);
        const uint8_t* windowStartPointer = codeSectionData + startOffset;
        const size_t   availableByteCount = codeSectionSize - startOffset;

        // 已经确认这个窗口的 imm32 位置是对的，所以先确定它本身能不能用
        if (windowStartPointer[0] == 0)
            continue;   // 理论上到不了这里，防御一下

        for (int windowLength = 8; windowLength <= maximumPatternLength;
             ++windowLength)
        {
            if ((size_t)windowLength > availableByteCount)
                break;
            // 窗口至少要包住整个 imm32（它就在 immediateValueOffset 处）
            if (windowLength < immediateValueOffset + 4)
                continue;

            std::vector<uint8_t> comparisonMask(windowLength, 1);
            StabilizeWindow(image, windowStartPointer, availableByteCount,
                            startRelativeAddress,
                            windowLength, comparisonMask);

            // 兜底：把 imm32 那 4 字节强制通配掉。
            // StabilizeWindow 通常已经处理了（因为它是个镜像内绝对地址），
            // 但万一这个值恰好也能被解释成 rel32，两种判断都不该让它留下实字节。
            for (int wildcardIndex = 0; wildcardIndex < 4; ++wildcardIndex)
                comparisonMask[immediateValueOffset + wildcardIndex] = 0;

            // 开头就是通配位的码没有意义
            if (comparisonMask[0] == 0)
                continue;

            int exactByteCount    = 0;
            int wildcardByteCount = 0;
            const std::string patternText =
                BuildPatternText(windowStartPointer, comparisonMask, windowLength,
                                 exactByteCount, wildcardByteCount);

            if (exactByteCount < kMinimumExactByteCount)
                continue;

            SignaturePattern pattern = ParseSignaturePattern(patternText.c_str());
            if (!pattern.isValid)
                continue;

            std::vector<SignatureHit> hits;
            ScanMemoryBuffer(codeSectionData, codeSectionSize, codeSectionAddress,
                             pattern, hits, 8);
            if (hits.size() != 1)
                continue;

            GeneratedSignatureCandidate candidate;
            candidate.isValid              = true;
            candidate.startRelativeAddress = startRelativeAddress;
            candidate.windowLength         = windowLength;
            candidate.exactByteCount       = exactByteCount;
            candidate.wildcardByteCount    = wildcardByteCount;
            candidate.addressAdjustment    = 0;   // 命中处就是有效地址
            candidate.immediateValueOffset = immediateValueOffset;
            candidate.patternText          = patternText;
            allCandidates.push_back(candidate);

            break;   // 这个起点已经找到最短的唯一码
        }
    }

    if (allCandidates.empty())
    {
        std::printf("[失败] %zu 处引用都没能生成唯一特征码。\n",
                    references.size());
        std::printf("       说明这些引用附近字节太大众化，"
                    "试试加大 [最大长度]，或换个引用点（在 x64dbg 里另找一处）。\n");
        return 2;
    }

    std::sort(allCandidates.begin(), allCandidates.end(),
              [](const GeneratedSignatureCandidate& first,
                 const GeneratedSignatureCandidate& second)
              { return IsBetterCandidate(first, second); });

    const GeneratedSignatureCandidate& bestCandidate = allCandidates.front();

    std::printf("生成成功（长度 %d，实字节 %d，通配 %d）。\n\n",
                bestCandidate.windowLength, bestCandidate.exactByteCount,
                bestCandidate.wildcardByteCount);
    std::printf("可直接粘进 SignatureTable.cpp：\n\n");
    std::printf("    {\n");
    std::printf("        \"<起个语义名>\",\n");
    std::printf("        \"%s\",\n", bestCandidate.patternText.c_str());
    std::printf("        SignatureResolveKind::ReadImmediateValue, %d, 0, 0x%X,\n",
                bestCandidate.immediateValueOffset, targetRelativeAddress);
    std::printf("        \"<中文备注>\"\n");
    std::printf("    },\n\n");

    std::printf("字段说明（这一条和 --gen 出来的不一样，注意别搞混）：\n");
    std::printf("  SignatureResolveKind::ReadImmediateValue"
                "  有效地址处是一条引用指令，目标是 imm32 里的值\n");
    std::printf("  immediateValueOffset = %d"
                "                   imm32 相对命中处的偏移\n",
                bestCandidate.immediateValueOffset);
    std::printf("  addressAdjustment    = 0"
                "                   命中处就是有效地址\n");
    std::printf("  expectedRelativeAddress = 0x%X"
                "       写【全局变量的相对地址】，不是绝对地址\n",
                targetRelativeAddress);
    std::printf("  （工具已经帮你把 imm32 的绝对地址 0x%X 换算成相对地址来校验）\n\n",
                targetAbsoluteAddress);

    // ---- 其余备选 ----
    if (allCandidates.size() > 1)
    {
        std::printf("另外还有 %zu 个可用候选。"
                    "如果上面这条哪天 MISS 了，换一条试试：\n\n",
                    allCandidates.size() - 1);
        const size_t kMaximumAlternativeCount = 5;
        size_t printedAlternatives = 0;
        for (size_t index = 1; index < allCandidates.size(); ++index)
        {
            if (printedAlternatives >= kMaximumAlternativeCount)
            {
                std::printf("    ...（还有 %zu 个，省略）\n",
                            allCandidates.size() - index);
                break;
            }
            const GeneratedSignatureCandidate& candidate = allCandidates[index];
            std::printf("    命中 0x%-8X imm32偏移=%d 通配=%d  \"%s\"\n",
                        candidate.startRelativeAddress, candidate.immediateValueOffset,
                        candidate.wildcardByteCount, candidate.patternText.c_str());
            ++printedAlternatives;
        }
        std::printf("\n    换之前先用 --matrix 看看哪个在历史上最稳。\n");
    }

    return 0;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    // 源码是 UTF-8（编译时加了 /utf-8），把控制台输出页也切到 UTF-8，
    // 否则中文报告在默认的 GBK 控制台里是乱码。
    SetConsoleOutputCP(CP_UTF8);

    // 不带参数 = 更新流程。这是双击 build_tool.bat 走的那条路。
    if (argc < 2)
        return RunOffsetUpdateFlow(false);

    const char* command = argv[1];

    if (std::strcmp(command, "--help") == 0 || std::strcmp(command, "-h") == 0)
    {
        PrintUsageInstructions();
        return 0;
    }

    if (std::strcmp(command, "--force") == 0)
        return RunOffsetUpdateFlow(true);

    if (std::strcmp(command, "--matrix") == 0)
    {
        if (argc < 3)
        {
            std::printf("[错误] --matrix 至少需要一个镜像\n\n");
            PrintUsageInstructions();
            return 1;
        }
        std::vector<const char*> filePaths;
        for (int argumentIndex = 2; argumentIndex < argc; ++argumentIndex)
            filePaths.push_back(argv[argumentIndex]);
        return RunCrossVersionMatrix(filePaths);
    }

    if (std::strcmp(command, "--gen") == 0)
    {
        if (argc < 4)
        {
            std::printf("[错误] --gen 需要：<镜像> <相对地址>\n\n");
            PrintUsageInstructions();
            return 1;
        }
        const uint32_t targetRelativeAddress =
            (uint32_t)std::strtoul(argv[3], nullptr, 16);
        const int maximumPatternLength = (argc >= 5) ? std::atoi(argv[4]) : 64;
        return GenerateSignatureForAddress(argv[2], targetRelativeAddress,
                                           maximumPatternLength);
    }

    if (std::strcmp(command, "--gen-global") == 0)
    {
        if (argc < 4)
        {
            std::printf("[错误] --gen-global 需要：<镜像> <全局变量相对地址>\n\n");
            PrintUsageInstructions();
            return 1;
        }
        const uint32_t targetRelativeAddress =
            (uint32_t)std::strtoul(argv[3], nullptr, 16);
        const int maximumPatternLength = (argc >= 5) ? std::atoi(argv[4]) : 64;
        return GenerateGlobalVariableSignature(argv[2], targetRelativeAddress,
                                               maximumPatternLength);
    }

    std::printf("[错误] 不认识的参数：%s\n\n", command);
    PrintUsageInstructions();
    return 1;
}