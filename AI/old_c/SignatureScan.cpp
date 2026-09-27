// 读文件用 std::fopen 就够了，不想为了 MSVC 的安全检查去引 <filesystem>。
// 这个宏必须在任何 <cstdio> 之前定义。
#define _CRT_SECURE_NO_WARNINGS

#include "SignatureScan.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>

namespace signature_scan {

// =============================================================================
//  特征码解析
// =============================================================================

// 把一个十六进制字符转成 0..15，非法字符返回 -1
static int HexDigitValue(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

// 把 "E8 ?? ?? ?? ?? 80 38" 拆成 expectedBytes / comparisonMask。
// 允许的分隔符：空格、制表、逗号。通配写法：? ?? **
SignaturePattern ParseSignaturePattern(const char* patternText)
{
    SignaturePattern pattern;
    pattern.patternText = patternText ? patternText : "";

    if (!patternText || !*patternText)
    {
        pattern.errorMessage = "特征码为空";
        return pattern;
    }

    const char* cursor = patternText;
    while (*cursor)
    {
        // 跳过/去除分隔符
        while (*cursor && (std::isspace((unsigned char)*cursor) || *cursor == ','))
            ++cursor;
        if (!*cursor)
            break;

        // 通配
        if (*cursor == '?')
        {
            while (*cursor == '?') ++cursor;    // 吃掉连续的 '?'
            pattern.expectedBytes.push_back(0);
            pattern.comparisonMask.push_back(0);
            continue;
        }
        if (*cursor == '*')
        {
            while (*cursor == '*') ++cursor;
            pattern.expectedBytes.push_back(0);
            pattern.comparisonMask.push_back(0);
            continue;
        }

        // 十六进制字节
        const int highNibble = HexDigitValue(*cursor);
        if (highNibble < 0)
        {
            pattern.errorMessage = std::string("非法字符 '") + *cursor + "'";
            return pattern;
        }
        ++cursor;

        const int lowNibble = HexDigitValue(*cursor);
        if (lowNibble < 0)
        {
            pattern.errorMessage = "十六进制字节不完整（少了一个半字节）";
            return pattern;
        }
        ++cursor;

        pattern.expectedBytes.push_back((uint8_t)((highNibble << 4) | lowNibble));
        pattern.comparisonMask.push_back(1);
    }

    if (pattern.expectedBytes.empty())
    {
        pattern.errorMessage = "特征码解析后为空";
        return pattern;
    }

    // 统计实字节个数，并记下第一个实(非?)字节的位置（扫描时用它做锚点）
    for (size_t byteIndex = 0; byteIndex < pattern.comparisonMask.size(); ++byteIndex)
    {
        if (pattern.comparisonMask[byteIndex])
        {
            ++pattern.exactByteCount;
            if (pattern.firstExactByteIndex < 0)
                pattern.firstExactByteIndex = (int)byteIndex;
        }
    }

    if (pattern.firstExactByteIndex < 0)
    {
        pattern.errorMessage = "特征码全是通配，无法定位";
        return pattern;
    }

    pattern.isValid = true;
    return pattern;
}

// =============================================================================
//  扫描
// =============================================================================

// 在 memoryData 的 [0, availableSize) 内，检查 [0, pattern.expectedBytes.size())
// 是否匹配
static inline bool DoesPatternMatchAt(const uint8_t* memoryData, size_t availableSize,
                                      const SignaturePattern& pattern)
{
    const size_t patternLength = pattern.expectedBytes.size();
    if (availableSize < patternLength)
        return false;

    const uint8_t* mask  = pattern.comparisonMask.data();  //返回指向容器内部存储首元素的指针
    const uint8_t* bytes = pattern.expectedBytes.data();

    for (size_t byteIndex = 0; byteIndex < patternLength; ++byteIndex)
    {
        if (mask[byteIndex] && memoryData[byteIndex] != bytes[byteIndex])
            return false;
    }
    return true;
}

size_t ScanMemoryBuffer(const uint8_t* memoryData, size_t memorySize,           // 要扫描的内存缓冲区 // 缓冲区大小
                        uintptr_t bufferStartAddress,                           // 缓冲区首字节对应的“绝对地址”（用于算真实地址）
                        const SignaturePattern& pattern,                        // 已解析好的特征码
                        std::vector<SignatureHit>& hits,                        // 输出：命中的位置列表
                        size_t maximumHitCount)                                 // 最多收集多少个命中
{
    hits.clear();
    if (!memoryData || !pattern.isValid)
        return 0;

    const size_t patternLength = pattern.expectedBytes.size();
    if (memorySize < patternLength)
        return 0;

    const size_t  anchorIndex     = (size_t)pattern.firstExactByteIndex;
    const uint8_t anchorByteValue = pattern.expectedBytes[anchorIndex];

    size_t       totalHitCount   = 0;
    const size_t lastPossibleStart = memorySize - patternLength;

    for (size_t searchStart = 0; searchStart <= lastPossibleStart; )
    {
        // 用锚点字节快速跳过（memchr 在 CRT 里是 SIMD 优化过的）
        const uint8_t* anchorPosition =
            (const uint8_t*)std::memchr(memoryData + searchStart + anchorIndex,
                                        anchorByteValue,
                                        (lastPossibleStart - searchStart) + 1);   // 比首字节
        if (!anchorPosition)
            break;

        // anchorPosition 指向锚点位置，candidateStart 相对于起始位置
        const size_t candidateStart = (size_t)(anchorPosition - memoryData) - anchorIndex;

        if (DoesPatternMatchAt(memoryData + candidateStart,
                               memorySize - candidateStart, pattern))   // 比所有整个特征码
        {
            ++totalHitCount;
            if (hits.size() < maximumHitCount)
            {
                SignatureHit hit;
                hit.bufferOffset    = candidateStart;
                hit.relativeAddress = bufferStartAddress + (uintptr_t)candidateStart;
                hits.push_back(hit);
            }
            if (hits.size() >= maximumHitCount)
                break;      // 已经收够了，再数下去没意义
        }

        searchStart = candidateStart + 1;
    }

    return totalHitCount;
}

void ScanMemoryBufferForMultiplePatterns(const uint8_t* memoryData, size_t memorySize,
                                         uintptr_t bufferStartAddress,
                                         const SignaturePattern* patterns,
                                         size_t patternCount,
                                         std::vector<std::vector<SignatureHit>>& hits,
                                         size_t maximumHitCountPerPattern)
{
    hits.assign(patternCount, std::vector<SignatureHit>());

    if (!memoryData || !patterns || patternCount == 0)
        return;

    // 按「锚点字节」分桶：每个字节值 -> 可能匹配的表项下标
    std::vector<uint32_t> patternsByAnchorByte[256];
    std::vector<uint32_t> patternsWithoutAnchor;   // 没有可用锚点的（正常不该出现）

    for (size_t patternIndex = 0; patternIndex < patternCount; ++patternIndex)
    {
        if (!patterns[patternIndex].isValid ||
            patterns[patternIndex].expectedBytes.size() > memorySize)
            continue;

        if (patterns[patternIndex].firstExactByteIndex < 0)
        {
            patternsWithoutAnchor.push_back((uint32_t)patternIndex);
        }
        else
        {
            const uint8_t anchorByteValue =
                patterns[patternIndex].expectedBytes[patterns[patternIndex].firstExactByteIndex];
            patternsByAnchorByte[anchorByteValue].push_back((uint32_t)patternIndex);
        }
    }

    // 单趟遍历整块内存
    for (size_t memoryIndex = 0; memoryIndex < memorySize; ++memoryIndex)
    {
        const std::vector<uint32_t>& candidates = patternsByAnchorByte[memoryData[memoryIndex]];
        if (!candidates.empty())
        {
            for (size_t candidateIndex = 0; candidateIndex < candidates.size(); ++candidateIndex)
            {
                const SignaturePattern& pattern = patterns[candidates[candidateIndex]];
                const size_t anchorIndex = (size_t)pattern.firstExactByteIndex;
                if (memoryIndex < anchorIndex)
                    continue;
                const size_t candidateStart = memoryIndex - anchorIndex;

                if (!DoesPatternMatchAt(memoryData + candidateStart,
                                        memorySize - candidateStart, pattern))
                    continue;

                std::vector<SignatureHit>& patternHits = hits[candidates[candidateIndex]];
                if (patternHits.size() < maximumHitCountPerPattern)
                {
                    SignatureHit hit;
                    hit.bufferOffset    = candidateStart;
                    hit.relativeAddress = bufferStartAddress + (uintptr_t)candidateStart;
                    patternHits.push_back(hit);
                }
            }
        }

        // 开头全是通配的特征码（性能兜底，正常用不到）
        for (size_t index = 0; index < patternsWithoutAnchor.size(); ++index)
        {
            const SignaturePattern& pattern = patterns[patternsWithoutAnchor[index]];
            if (DoesPatternMatchAt(memoryData + memoryIndex,
                                   memorySize - memoryIndex, pattern))
            {
                std::vector<SignatureHit>& patternHits = hits[patternsWithoutAnchor[index]];
                if (patternHits.size() < maximumHitCountPerPattern)
                {
                    SignatureHit hit;
                    hit.bufferOffset    = memoryIndex;
                    hit.relativeAddress = bufferStartAddress + (uintptr_t)memoryIndex;
                    patternHits.push_back(hit);
                }
            }
        }
    }
}

// =============================================================================
//  可执行文件镜像
// =============================================================================

static uint32_t ReadUInt32(const uint8_t* sourcePointer)
{
    uint32_t value;
    std::memcpy(&value, sourcePointer, 4);
    return value;
}

static uint16_t ReadUInt16(const uint8_t* sourcePointer)
{
    uint16_t value;
    std::memcpy(&value, sourcePointer, 2);
    return value;
}

// 解析 PE 头这件事，文件布局和内存布局是一样的 —— 头本身总是按相对地址摆的。
// 差别只在「段数据在哪」，那是 ConvertRelativeAddressToPointer /
// GetExecutableCodeSection 的事，由 isMemoryLayout 决定。
// 所以这里只负责把 isMemoryLayout 记下来，其余共用。
bool PortableExecutableImage::ParseAsMemoryLayout(const uint8_t* sourceImageData,
                                                  size_t sourceImageSize)
{
    if (!ParseAsFileLayout(sourceImageData, sourceImageSize))
        return false;
    isMemoryLayout = true;
    return true;
}

bool PortableExecutableImage::ParseAsFileLayout(const uint8_t* sourceImageData,
                                                size_t sourceImageSize)
{
    imageData        = sourceImageData;
    imageSize        = sourceImageSize;
    isMemoryLayout   = false;
    sections.clear();
    imageBaseAddress = 0;
    isFlatFileLayout = false;

    if (!imageData || imageSize < 0x40)
        return false;

    // ---- 不是 PE？当作裸内存镜像处理 ----
    // 某些 dumper 产出的是「按内存布局平铺」的文件，没有可用的 PE 头。
    // 这种情况下文件偏移 == 相对地址，仍然可以扫描，只是没有段信息。
    if (imageData[0] != 'M' || imageData[1] != 'Z')
    {
        isFlatFileLayout = true;

        PortableExecutableSection flatSection;
        flatSection.sectionName       = "{flat-image}";
        flatSection.relativeAddress   = 0;
        flatSection.virtualSize       = (uint32_t)imageSize;
        flatSection.rawDataFileOffset = 0;
        flatSection.rawDataSize       = (uint32_t)imageSize;
        flatSection.isExecutable      = true;
        sections.push_back(flatSection);
        return true;
    }

    const uint32_t peHeaderFileOffset = ReadUInt32(imageData + 0x3C);
    if (peHeaderFileOffset + 24 > imageSize)
        return false;
    if (std::memcmp(imageData + peHeaderFileOffset, "PE\0\0", 4) != 0)
        return false;

    const uint16_t sectionCount      = ReadUInt16(imageData + peHeaderFileOffset + 6);
    const uint16_t optionalHeaderSize = ReadUInt16(imageData + peHeaderFileOffset + 20);
    const uint8_t* optionalHeader     = imageData + peHeaderFileOffset + 24;

    if (peHeaderFileOffset + 24 + optionalHeaderSize > imageSize)
        return false;
    if (optionalHeaderSize < 96)
        return false;

    const uint16_t magicNumber = ReadUInt16(optionalHeader);
    if (magicNumber != 0x10B)               // 只要 PE32（本工程是 32 位）
        return false;

    imageBaseAddress = ReadUInt32(optionalHeader + 28);

    const uint8_t* sectionTable = optionalHeader + optionalHeaderSize;
    for (uint16_t sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex)
    {
        const uint8_t* sectionHeader = sectionTable + (size_t)sectionIndex * 40;
        if ((size_t)(sectionHeader - imageData) + 40 > imageSize)
            break;

        PortableExecutableSection section;
        char sectionNameBuffer[9] = { 0 };
        std::memcpy(sectionNameBuffer, sectionHeader, 8);
        section.sectionName       = sectionNameBuffer;
        section.virtualSize       = ReadUInt32(sectionHeader + 8);
        section.relativeAddress   = ReadUInt32(sectionHeader + 12);
        section.rawDataSize       = ReadUInt32(sectionHeader + 16);
        section.rawDataFileOffset = ReadUInt32(sectionHeader + 20);

        const uint32_t characteristics = ReadUInt32(sectionHeader + 36);
        section.isExecutable = (characteristics & 0x20000000u) != 0;

        sections.push_back(section);
    }

    return true;
}

const PortableExecutableSection*
PortableExecutableImage::FindSectionContainingAddress(uintptr_t relativeAddress) const
{
    for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
    {
        const PortableExecutableSection& section = sections[sectionIndex];
        if (relativeAddress >= section.relativeAddress &&
            relativeAddress < (uintptr_t)section.relativeAddress + section.virtualSize)
        {
            return &section;
        }
    }
    return nullptr;
}

bool PortableExecutableImage::ConvertRelativeAddressToPointer(
        uintptr_t relativeAddress, const uint8_t** outPointer) const
{
    const PortableExecutableSection* section = FindSectionContainingAddress(relativeAddress);
    if (!section)
        return false;

    const uintptr_t offsetInsideSection = relativeAddress - section->relativeAddress;

    if (isMemoryLayout)
    {
        // 活进程：段就在 imageBase + 相对地址 处，段长度按虚拟大小算
        //（内存里没有文件那种按分页对齐的填充）
        if (section->virtualSize != 0 && offsetInsideSection >= section->virtualSize)
            return false;
        if (relativeAddress >= imageSize)
            return false;

        *outPointer = imageData + relativeAddress;
        return true;
    }

    if (offsetInsideSection >= section->rawDataSize)   // 未初始化数据（.bss 之类）
        return false;

    const size_t fileOffset = (size_t)section->rawDataFileOffset + (size_t)offsetInsideSection;
    if (fileOffset >= imageSize)
        return false;

    *outPointer = imageData + fileOffset;
    return true;
}

bool PortableExecutableImage::ConvertRelativeAddressToFileOffset(
        uintptr_t relativeAddress, size_t& outFileOffset) const
{
    const uint8_t* pointer = nullptr;
    if (!ConvertRelativeAddressToPointer(relativeAddress, &pointer))
        return false;

    outFileOffset = (size_t)(pointer - imageData);
    return true;
}

uint32_t PortableExecutableImage::ConvertFileOffsetToRelativeAddress(size_t fileOffset) const
{
    for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
    {
        const PortableExecutableSection& section = sections[sectionIndex];
        if (fileOffset >= section.rawDataFileOffset &&
            fileOffset < (size_t)section.rawDataFileOffset + section.rawDataSize)
        {
            return section.relativeAddress +
                   (uint32_t)(fileOffset - section.rawDataFileOffset);
        }
    }
    return 0;
}

bool PortableExecutableImage::GetExecutableCodeSection(const uint8_t** outData,
                                                       size_t& outSize,
                                                       uintptr_t& outRelativeAddress) const
{
    // 活进程：段直接在 imageBase + 相对地址 处，长度按虚拟大小
    if (isMemoryLayout)
    {
        const PortableExecutableSection* codeSection = nullptr;
        for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
        {
            if (sections[sectionIndex].sectionName == ".text")
            {
                codeSection = &sections[sectionIndex];
                break;
            }
        }
        if (!codeSection)
        {
            for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
            {
                if (sections[sectionIndex].isExecutable)
                {
                    codeSection = &sections[sectionIndex];
                    break;
                }
            }
        }
        if (!codeSection)
            return false;

        size_t usableSize = codeSection->virtualSize ? codeSection->virtualSize
                                                     : codeSection->rawDataSize;
        if ((size_t)codeSection->relativeAddress >= imageSize)
            return false;
        if ((size_t)codeSection->relativeAddress + usableSize > imageSize)
            usableSize = imageSize - codeSection->relativeAddress;
        if (usableSize == 0)
            return false;

        *outData           = imageData + codeSection->relativeAddress;
        outSize            = usableSize;
        outRelativeAddress = codeSection->relativeAddress;
        return true;
    }

    // 文件布局：优先找名为 .text 的段；找不到就退化为第一个可执行段
    const PortableExecutableSection* codeSection = nullptr;
    for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
    {
        if (sections[sectionIndex].sectionName == ".text")
        {
            codeSection = &sections[sectionIndex];
            break;
        }
    }
    if (!codeSection)
    {
        for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
        {
            if (sections[sectionIndex].isExecutable)
            {
                codeSection = &sections[sectionIndex];
                break;
            }
        }
    }
    if (!codeSection)
        return false;

    // 只扫文件里真正有字节的部分
    size_t usableSize = codeSection->rawDataSize;
    if (usableSize > (size_t)codeSection->virtualSize && codeSection->virtualSize != 0)
        usableSize = codeSection->virtualSize;
    if ((size_t)codeSection->rawDataFileOffset + usableSize > imageSize)
        usableSize = imageSize - codeSection->rawDataFileOffset;

    if (usableSize == 0)
        return false;

    *outData           = imageData + codeSection->rawDataFileOffset;
    outSize            = usableSize;
    outRelativeAddress = codeSection->relativeAddress;
    return true;
}

bool PortableExecutableImage::IsAddressInsideImage(uintptr_t relativeAddress) const
{
    return FindSectionContainingAddress(relativeAddress) != nullptr;
}

// =============================================================================
//  文件读取
// =============================================================================

bool ReadEntireFileIntoBuffer(const char* filePath, std::vector<uint8_t>& outBuffer)
{
    std::FILE* file = std::fopen(filePath, "rb");
    if (!file)
        return false;

    std::fseek(file, 0, SEEK_END);
    const long fileLength = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (fileLength <= 0)
    {
        std::fclose(file);
        return false;
    }

    outBuffer.resize((size_t)fileLength);
    const size_t bytesRead = std::fread(outBuffer.data(), 1, (size_t)fileLength, file);
    std::fclose(file);

    if (bytesRead != (size_t)fileLength)
    {
        outBuffer.clear();
        return false;
    }
    return true;
}

} // namespace signature_scan
