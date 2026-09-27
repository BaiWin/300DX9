#include "SignatureDefinitionResolve.h"

#include <cstdio>
#include <cstring>

namespace signature_scan {

// =============================== 状态文本 ===============================

const char* GetResolveStatusText(SignatureResolveStatus status)
{
    switch (status)
    {
    case SignatureResolveStatus::Ok:                       return "OK";
    case SignatureResolveStatus::InvalidPattern:           return "BAD-PATTERN";
    case SignatureResolveStatus::ImageHasNoCodeSection:    return "NO-CODE-SECTION";
    case SignatureResolveStatus::NotFound:                 return "MISS";
    case SignatureResolveStatus::NotUnique:                return "NOT-UNIQUE";
    case SignatureResolveStatus::NotACallInstruction:      return "NOT-A-CALL";
    case SignatureResolveStatus::NotAReferenceInstruction: return "NOT-A-REFERENCE";
    case SignatureResolveStatus::BadBoundaryLength:        return "BAD-BOUNDARY";
    case SignatureResolveStatus::SecondaryNotFound:        return "SECONDARY-MISS";
    case SignatureResolveStatus::SecondaryNotUnique:       return "SECONDARY-NOT-UNIQUE";
    case SignatureResolveStatus::AddressOutOfCodeSection:  return "OUT-OF-CODE";
    }
    return "UNKNOWN";
}

const char* GetSignatureKindText(SignatureKind kind)
{
    switch (kind)
    {
    case SignatureKind::Function:   return "Function";
    case SignatureKind::Trampoline: return "Trampoline";
    case SignatureKind::Dword:      return "Dword";
    }
    return "UNKNOWN";
}

namespace {

// 二级扫描最多从函数入口往下找这么多字节。
// 函数都很小，找不到就是这条二级码废了 —— 不要为了一个废码把整个 .text 扫一遍。
constexpr int64_t kSecondaryScanMaximumByteCount = 0x10000;

// 诊断用：二级扫描前几个命中都记下来。第二个命中离第一个太近就是个警告信号：
// 说明二级码太短，游戏版本一动它就可能前移到别的地方去。
constexpr size_t kSecondaryHitReportCount = 4;

// 操作码推长度、读 4 字节，最多需要看到 8 个字节。
constexpr size_t kMinimumInstructionByteCount = 8;

uint32_t ReadUInt32At(const uint8_t* pointer)
{
    uint32_t value = 0;
    std::memcpy(&value, pointer, sizeof(value));
    return value;
}

int32_t ReadInt32At(const uint8_t* pointer)
{
    int32_t value = 0;
    std::memcpy(&value, pointer, sizeof(value));
    return value;
}

bool IsCallOpcode(uint8_t opcodeByte)
{
    return opcodeByte == 0xE8 || opcodeByte == 0xE9;
}

// -----------------------------------------------------------------------------
//  哪条指令里装了我们要找的那个 4 字节
// -----------------------------------------------------------------------------
//  不做完整的 x86 反汇编，只认下面这些形状 —— 表里用到的全在里头：
//
//      A1 / A3    +imm32                     绝对地址，4 字节在 +1
//      66 A1 / 66 A3 +imm32                  66 只把 eax 换成 ax，【地址宽度不变】，
//                                            所以 4 字节还在 +2，不是 +1
//      <op> <modrm mod=00 rm=101> +disp32     没有基址寄存器 -> 4 字节就是绝对地址
//      <op> <modrm mod=10>        +disp32     有基址寄存器 -> 4 字节是【结构体偏移】
//      F3/F2 0F 10/11 <modrm>     +disp32     movss / movsd
// -----------------------------------------------------------------------------

struct ImmediateValueSite
{
    int  immediateOffset   = 0;     // 4 字节相对有效地址的位置
    bool isAbsoluteAddress = false; // true = 里面是绝对地址，要减镜像基址
    int  instructionLength = 0;     // 整条指令多长，用来和表里的「边界」对账
};

bool IsSupportedReferenceOpcode(uint8_t opcodeByte)
{
    switch (opcodeByte)
    {
    case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D:
    case 0xC6: case 0xC7:
    case 0xF7: case 0xFF:
    case 0x10: case 0x11:   // 只在 F3/F2 0F 后面才有意义，靠前面的前缀拦住
        return true;
    default:
        return false;
    }
}

bool ClassifyImmediateValueSite(const uint8_t* instructionBytes,
                                size_t availableByteCount,
                                ImmediateValueSite& outSite)
{
    outSite = ImmediateValueSite{};
    if (availableByteCount < kMinimumInstructionByteCount)
        return false;

    // A1 / A3：moffs32，直接就是绝对地址
    if (instructionBytes[0] == 0xA1 || instructionBytes[0] == 0xA3)
    {
        outSite.immediateOffset   = 1;
        outSite.isAbsoluteAddress = true;
        outSite.instructionLength = 5;
        return true;
    }

    // 66 A1 / 66 A3
    if (instructionBytes[0] == 0x66 &&
        (instructionBytes[1] == 0xA1 || instructionBytes[1] == 0xA3))
    {
        outSite.immediateOffset   = 2;
        outSite.isAbsoluteAddress = true;
        outSite.instructionLength = 6;
        return true;
    }

    size_t opcodeIndex = 0;
    bool isSinglePrecisionFloatingPoint = false;
    if (instructionBytes[0] == 0x66)
    {
        opcodeIndex = 1;                        // 操作数尺寸前缀
    }
    else if ((instructionBytes[0] == 0xF3 || instructionBytes[0] == 0xF2) &&
             instructionBytes[1] == 0x0F)
    {
        opcodeIndex = 2;                        // F3 0F 10 / F3 0F 11
        isSinglePrecisionFloatingPoint = true;
    }

    const uint8_t opcodeByte = instructionBytes[opcodeIndex];
    if (!IsSupportedReferenceOpcode(opcodeByte))
        return false;
    // 0x10 / 0x11 单摆着不是 movss，必须有 F3/F2 0F 前缀才算
    if ((opcodeByte == 0x10 || opcodeByte == 0x11) && !isSinglePrecisionFloatingPoint)
        return false;

    const size_t  modrmIndex = opcodeIndex + 1;
    const uint8_t modrmByte  = instructionBytes[modrmIndex];
    const uint8_t modBits    = (uint8_t)(modrmByte >> 6);
    const uint8_t registerMemoryBits = (uint8_t)(modrmByte & 0x07);

    // mod=00 rm=101：没有基址寄存器，disp32 就是绝对地址
    if (modBits == 0 && registerMemoryBits == 5)
    {
        outSite.immediateOffset   = (int)(modrmIndex + 1);
        outSite.isAbsoluteAddress = true;
        outSite.instructionLength = (int)(modrmIndex + 5);
        return true;
    }
    // mod=10 且不是 SIB：disp32 是 [基址寄存器 + 位移]，是纯偏移，不是地址
    if (modBits == 2 && registerMemoryBits != 4)
    {
        outSite.immediateOffset   = (int)(modrmIndex + 1);
        outSite.isAbsoluteAddress = false;
        outSite.instructionLength = (int)(modrmIndex + 5);
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
//  函数体末尾 = 第一条 ret 之后
// -----------------------------------------------------------------------------
//  为什么需要它：二级码（例如 A1 ?? ?? ?? ?? C3，mov eax,[全局] ; ret）通常
//  【不是】唯一的，靠「从函数入口往下取第一个」来定位。可是编译器常把几个
//  一模一样的小 getter 排在一起：
//
//      001C11B0  A1 40 0C E7 01  C3    mov eax, dword_1A70C40 ; ret   <- 要的
//      001C11C0  A1 44 0C E7 01  C3    mov eax, dword_1A70C44 ; ret   <- 隔壁的
//
//  两个函数只差 16 字节，后面都是 CC 填充，靠加长特征码【分不开】。
//  但只要把扫描范围收到函数体内，第二个就自然被排除了 —— 它在上一条 ret 之后。
//
//  ret 的两种编码都认：C3（ret）、C2 iw（ret imm16）。
//  这里只是「划一条边界」，不是完整反汇编：万一 C3 出现在某条指令的立即数里，
//  边界会画早。画早的后果是 SECONDARY-MISS（吵，一眼能看见），
//  而不是选错一个地址（静默，极难查）—— 失败方向是安全的。
// -----------------------------------------------------------------------------
uintptr_t FindFunctionEndRelativeAddress(const uint8_t* codeSectionData,
                                         int64_t codeSectionStart,
                                         int64_t codeSectionEnd,
                                         int64_t functionEntryRelativeAddress,
                                         int64_t maximumScanByteCount)
{
    int64_t scanByteCount = codeSectionEnd - functionEntryRelativeAddress;
    if (scanByteCount > maximumScanByteCount)
        scanByteCount = maximumScanByteCount;
    if (scanByteCount <= 0)
        return 0;

    const uint8_t* functionBytes = codeSectionData +
                                   (functionEntryRelativeAddress - codeSectionStart);

    for (int64_t offset = 0; offset < scanByteCount; ++offset)
    {
        if (functionBytes[offset] == 0xC3)
            return (uintptr_t)(functionEntryRelativeAddress + offset + 1);
        if (functionBytes[offset] == 0xC2 && offset + 2 < scanByteCount)
            return (uintptr_t)(functionEntryRelativeAddress + offset + 3);
    }
    return 0;
}

// -----------------------------------------------------------------------------
//  三种类型各自的解析
// -----------------------------------------------------------------------------

// 有效地址处是一条 E8/E9，目标是 有效地址 + 5 + rel32
void ResolveFunctionEntry(int64_t effectiveAddress,
                          const uint8_t* effectivePointer,
                          const SignatureDefinition& definition,
                          ResolvedSignature& result)
{
    if (!IsCallOpcode(effectivePointer[0]))
    {
        char messageBuffer[160];
        std::snprintf(messageBuffer, sizeof(messageBuffer),
                      "偏移指到的地方是 %02X，不是 E8/E9，偏移是不是填错一位",
                      effectivePointer[0]);
        result.status = SignatureResolveStatus::NotACallInstruction;
        result.errorMessage = messageBuffer;
        return;
    }
    if (definition.boundaryLength != 5)
    {
        char messageBuffer[160];
        std::snprintf(messageBuffer, sizeof(messageBuffer),
                      "边界填的是 %d，但 E8/E9 这条 call 是 5 字节",
                      definition.boundaryLength);
        result.status = SignatureResolveStatus::BadBoundaryLength;
        result.errorMessage = messageBuffer;
        return;
    }

    result.relativeAddress = (uintptr_t)(effectiveAddress + 5 + ReadInt32At(effectivePointer + 1));
    result.hasRelativeAddress = true;
}

// 有效地址就是 Hook 点，给出一对 (起点, 起点 + 边界)
void ResolveHookSpan(int64_t effectiveAddress,
                     const SignatureDefinition& definition,
                     ResolvedSignature& result)
{
    if (definition.boundaryLength <= 0)
    {
        result.status = SignatureResolveStatus::BadBoundaryLength;
        result.errorMessage = "Trampoline 的边界必须大于 0（它是被搬走的那几条指令的总长）";
        return;
    }

    result.isHookSpan = true;
    result.relativeAddress = (uintptr_t)effectiveAddress;
    result.boundaryEndRelativeAddress = (uintptr_t)(effectiveAddress + definition.boundaryLength);
    result.hasRelativeAddress = true;
}

// 有效地址处是一条引用指令，读它里面的 4 字节。
// 有二级码时，先按主码反解出函数入口，再从函数入口向下扫二级码，取第一个命中。
void ResolveDwordValue(const PortableExecutableImage& image,
                       const uint8_t* codeSectionData,
                       int64_t codeSectionStart,
                       int64_t codeSectionEnd,
                       int64_t effectiveAddress,
                       const uint8_t* effectivePointer,
                       size_t availableByteCount,
                       const SignatureDefinition& definition,
                       ResolvedSignature& result)
{
    int64_t valueSiteAddress = effectiveAddress;
    const uint8_t* valueSitePointer = effectivePointer;
    size_t valueSiteAvailableByteCount = availableByteCount;

    const bool hasSecondaryPattern = definition.secondarySignature.patternText != nullptr;

    if (hasSecondaryPattern)
    {
        // ---- 第一步：主码这条 call 反解出函数入口 ----
        if (!IsCallOpcode(effectivePointer[0]))
        {
            char messageBuffer[160];
            std::snprintf(messageBuffer, sizeof(messageBuffer),
                          "这条是二级查找，主码偏移处应该是 call（E8/E9），实际是 %02X",
                          effectivePointer[0]);
            result.status = SignatureResolveStatus::NotACallInstruction;
            result.errorMessage = messageBuffer;
            return;
        }

        const int64_t functionEntryAddress =
            effectiveAddress + 5 + ReadInt32At(effectivePointer + 1);
        result.secondaryFunctionEntryRelativeAddress = (uintptr_t)functionEntryAddress;

        if (functionEntryAddress < codeSectionStart || functionEntryAddress >= codeSectionEnd)
        {
            result.status = SignatureResolveStatus::AddressOutOfCodeSection;
            result.errorMessage = "按 call 反解出来的函数入口跑到代码段外面了";
            return;
        }

        // ---- 第二步：在【函数体内】扫二级码，取第一个命中 ----
        // 二级码通常【不是】唯一的，所以命中多处是正常的，不算错。
        // 但范围必须收在函数体内（入口 -> 第一条 ret）：编译器爱把几个一模一样的
        // 小 getter 排在一起，不收范围就会选到隔壁那个，而且静默无声。
        const SignaturePattern secondaryPattern =
            ParseSignaturePattern(definition.secondarySignature.patternText);
        if (!secondaryPattern.isValid)
        {
            result.status = SignatureResolveStatus::InvalidPattern;
            result.errorMessage = std::string("二级特征码有问题：") + secondaryPattern.errorMessage;
            return;
        }

        // 函数体末尾。找不到 ret 就退回 64KB 上限。
        const uintptr_t functionEndAddress =
            FindFunctionEndRelativeAddress(codeSectionData, codeSectionStart, codeSectionEnd,
                                           functionEntryAddress, kSecondaryScanMaximumByteCount);
        result.secondaryFunctionEndRelativeAddress = functionEndAddress;

        int64_t scanEndAddress = (functionEndAddress != 0) ? (int64_t)functionEndAddress
                                                          : codeSectionEnd;
        if (scanEndAddress > functionEntryAddress + kSecondaryScanMaximumByteCount)
            scanEndAddress = functionEntryAddress + kSecondaryScanMaximumByteCount;
        if (scanEndAddress > codeSectionEnd)
            scanEndAddress = codeSectionEnd;

        const int64_t scanByteCount = scanEndAddress - functionEntryAddress;

        std::vector<SignatureHit> secondaryHits;
        ScanMemoryBuffer(codeSectionData + (functionEntryAddress - codeSectionStart),
                         (size_t)scanByteCount,
                         (uintptr_t)functionEntryAddress,
                         secondaryPattern,
                         secondaryHits,
                         kSecondaryHitReportCount);

        if (secondaryHits.empty())
        {
            char messageBuffer[256];
            std::snprintf(messageBuffer, sizeof(messageBuffer),
                          "函数体 %08llX..%08llX（%lld 字节，按第一条 ret 划的）里没有二级码",
                          (unsigned long long)functionEntryAddress,
                          (unsigned long long)scanEndAddress,
                          (long long)scanByteCount);
            result.status = SignatureResolveStatus::SecondaryNotFound;
            result.errorMessage = messageBuffer;
            return;
        }

        for (size_t hitIndex = 0; hitIndex < secondaryHits.size(); ++hitIndex)
            result.secondaryHitRelativeAddresses.push_back(secondaryHits[hitIndex].relativeAddress);

        // 诊断：函数体【外面】还有多少同名命中。
        // 有的话说明这条二级码本身不唯一，只是靠 ret 边界兜住了 —— 要让你看见。
        if (scanEndAddress < codeSectionEnd)
        {
            int64_t beyondByteCount = codeSectionEnd - scanEndAddress;
            if (beyondByteCount > kSecondaryScanMaximumByteCount)
                beyondByteCount = kSecondaryScanMaximumByteCount;

            std::vector<SignatureHit> hitsBeyondFunctionEnd;
            ScanMemoryBuffer(codeSectionData + (scanEndAddress - codeSectionStart),
                             (size_t)beyondByteCount,
                             (uintptr_t)scanEndAddress,
                             secondaryPattern,
                             hitsBeyondFunctionEnd,
                             kSecondaryHitReportCount);
            result.secondaryHitCountBeyondFunctionEnd = hitsBeyondFunctionEnd.size();
        }

        valueSiteAddress = (int64_t)secondaryHits[0].relativeAddress +
                           (int64_t)definition.secondarySignature.patternOffset;

        if (valueSiteAddress < codeSectionStart || valueSiteAddress >= codeSectionEnd)
        {
            result.status = SignatureResolveStatus::AddressOutOfCodeSection;
            result.errorMessage = "二级命中处 + 二级偏移 跑到代码段外面了";
            return;
        }

        valueSitePointer = codeSectionData + (valueSiteAddress - codeSectionStart);
        valueSiteAvailableByteCount = (size_t)(codeSectionEnd - valueSiteAddress);
    }

    // ---- 第三步：看操作码，决定 4 字节在哪儿、要不要减镜像基址 ----
    ImmediateValueSite valueSite;
    if (!ClassifyImmediateValueSite(valueSitePointer, valueSiteAvailableByteCount, valueSite))
    {
        char messageBuffer[192];
        std::snprintf(messageBuffer, sizeof(messageBuffer),
                      "地址 %08llX 处的字节是 %02X %02X %02X %02X，不是认得的引用指令形状",
                      (unsigned long long)valueSiteAddress,
                      valueSitePointer[0], valueSitePointer[1],
                      valueSitePointer[2], valueSitePointer[3]);
        result.status = SignatureResolveStatus::NotAReferenceInstruction;
        result.errorMessage = messageBuffer;
        return;
    }

    // 用「边界」对账：操作码推出来的长度必须和表里填的一致。
    // 这一条免费抓「偏移数错一位」—— 数错了那边就不是一条完整指令。
    if (valueSite.instructionLength != definition.boundaryLength)
    {
        char messageBuffer[192];
        std::snprintf(messageBuffer, sizeof(messageBuffer),
                      "边界填的是 %d，但 %08llX 处那条指令按操作码推出来是 %d 字节",
                      definition.boundaryLength,
                      (unsigned long long)valueSiteAddress,
                      valueSite.instructionLength);
        result.status = SignatureResolveStatus::BadBoundaryLength;
        result.errorMessage = messageBuffer;
        return;
    }

    int64_t value = (int64_t)ReadUInt32At(valueSitePointer + valueSite.immediateOffset);
    if (valueSite.isAbsoluteAddress)
        value -= (int64_t)image.imageBaseAddress;   // 绝对地址 -> 换成相对地址
    value += (int64_t)definition.valueAdjustment;  // 修正

    result.relativeAddress = (uintptr_t)value;
    result.hasRelativeAddress = true;
}

} // namespace

// =============================== 主流程 ===============================

ResolvedSignature ResolveSignatureDefinition(const PortableExecutableImage& image,
                                             const SignatureDefinition& definition)
{
    ResolvedSignature result;
    result.definition = &definition;

    const SignaturePattern pattern = ParseSignaturePattern(definition.patternText);
    if (!pattern.isValid)
    {
        result.status = SignatureResolveStatus::InvalidPattern;
        result.errorMessage = pattern.errorMessage;
        return result;
    }

    const uint8_t* codeSectionData = nullptr;
    size_t         codeSectionByteCount = 0;
    uintptr_t      codeSectionRelativeAddress = 0;
    if (!image.GetExecutableCodeSection(&codeSectionData, codeSectionByteCount,
                                        codeSectionRelativeAddress))
    {
        result.status = SignatureResolveStatus::ImageHasNoCodeSection;
        result.errorMessage = "镜像里找不到可执行的代码段";
        return result;
    }

    // 只要唯一，所以最多找 2 处就够了 —— 找到第 2 处就已经知道不能用。
    std::vector<SignatureHit> hits;
    const size_t totalHitCount = ScanMemoryBuffer(codeSectionData, codeSectionByteCount,
                                                  codeSectionRelativeAddress,
                                                  pattern, hits, 2);
    result.hitCount = totalHitCount > hits.size() ? totalHitCount : hits.size();

    if (result.hitCount == 0)
    {
        result.status = SignatureResolveStatus::NotFound;
        result.errorMessage = "整个代码段一处都没命中";
        return result;
    }
    if (result.hitCount > 1)
    {
        char messageBuffer[128];
        std::snprintf(messageBuffer, sizeof(messageBuffer),
                      "命中 %zu 处，必须唯一才敢用", result.hitCount);
        result.status = SignatureResolveStatus::NotUnique;
        result.errorMessage = messageBuffer;
        return result;
    }

    result.hitRelativeAddress = hits[0].relativeAddress;

    const int64_t codeSectionStart = (int64_t)codeSectionRelativeAddress;
    const int64_t codeSectionEnd   = codeSectionStart + (int64_t)codeSectionByteCount;
    const int64_t effectiveAddress = (int64_t)result.hitRelativeAddress +
                                     (int64_t)definition.patternOffset;

    if (effectiveAddress < codeSectionStart || effectiveAddress >= codeSectionEnd)
    {
        result.status = SignatureResolveStatus::AddressOutOfCodeSection;
        result.errorMessage = "命中处 + 偏移 跑到代码段外面了，偏移是不是填错了";
        return result;
    }
    result.effectiveRelativeAddress = (uintptr_t)effectiveAddress;

    const uint8_t* effectivePointer = codeSectionData + (effectiveAddress - codeSectionStart);
    const size_t   availableByteCount = (size_t)(codeSectionEnd - effectiveAddress);

    switch (definition.signatureKind)
    {
    case SignatureKind::Function:
        ResolveFunctionEntry(effectiveAddress, effectivePointer, definition, result);
        break;

    case SignatureKind::Trampoline:
        ResolveHookSpan(effectiveAddress, definition, result);
        break;

    case SignatureKind::Dword:
        ResolveDwordValue(image, codeSectionData, codeSectionStart, codeSectionEnd,
                          effectiveAddress, effectivePointer, availableByteCount,
                          definition, result);
        break;

    default:
        result.status = SignatureResolveStatus::InvalidPattern;
        result.errorMessage = "类型字段是个不认识的值";
        break;
    }

    return result;
}

const ResolvedSignature* FindResolvedSignature(const std::vector<ResolvedSignature>& resolvedList,
                                               const char* signatureName)
{
    if (signatureName == nullptr)
        return nullptr;

    for (size_t index = 0; index < resolvedList.size(); ++index)
    {
        const SignatureDefinition* definition = resolvedList[index].definition;
        if (definition != nullptr && definition->signatureName != nullptr &&
            std::strcmp(definition->signatureName, signatureName) == 0)
        {
            return &resolvedList[index];
        }
    }
    return nullptr;
}

} // namespace signature_scan
