#pragma once
// =============================================================================
//  SignatureScan.h  —— 特征码扫描引擎
// -----------------------------------------------------------------------------
//  命名约定：本工程一律用英文全称，不用缩写。
//    例如 RelativeAddress 而不是 Rva、InstructionOffset 而不是 InsOff、
//    Image 而不是 img。名字长一点，但一眼就知道是什么。
//
//  设计要点：
//    * 本文件【不依赖 Windows、不依赖游戏】。它只认「一块内存 + 长度」，
//      所以同一套代码既能扫磁盘上的 300.exe，也能扫一个内存 dump ——
//      两种输入的字节是同一种，差别只在「段数据怎么换算成指针」，
//      由 PortableExecutableImage 的布局标志区分（见下面 isMemoryLayout）。
//
//    * 【目前唯一的调用方是独立工具】，扫的是磁盘上的 300.exe。
//      游戏里用的是工具生成出来的 offset_update.h，不在运行时扫。
//      但引擎本身是通用的 —— 以后多一个输入源，不用改这个文件。
//
//    * 本文件【只管扫】。特征码是「命中之后怎么算出真地址」那部分语义
//      （Function / Trampoline / Dword、偏移、边界、修正、二级）在另外两层：
//          SignaturePattern   = 字符串 "E8 ?? ?? ?? ?? 80 38 00 74 16"
//                                -> 期望字节 + 比较掩码
//          ScanMemoryBuffer   = 在缓冲区里找所有匹配（本文件到此为止）
//          offsets_AI.h       = 表本身，唯一手工维护的地方
//          SignatureDefinitionResolve.*
//                             = 按表解析出最终地址 + 诊断信息
//
//  为什么「命中之后怎么算」要单独一层：
//      表里的特征码分三类，混在一起是维护灾难的根源——
//        (a) Function    偏移处是一条 call/jmp，目标是它的跳转目标
//        (b) Trampoline  有效地址就是 Hook 点，结果是一对 (起点, 起点+边界)
//        (c) Dword       有效地址处是一条引用指令，取它立即数里的值；
//                        有的还要先进函数（二级码）才找得到那条指令
//      详见 offsets_AI.h 里的表结构说明。
// =============================================================================

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace signature_scan {

// =============================== 特征码 ===============================

struct SignaturePattern
{
    std::string          patternText;         // 原始字符串，报告里原样输出
    std::vector<uint8_t> expectedBytes;       // 期望字节（通配位填 0）
    std::vector<uint8_t> comparisonMask;      // 1 = 需要比较，0 = 通配
    size_t               exactByteCount  = 0; // 非通配字节个数
    int                  firstExactByteIndex = -1; // 第一个非通配位（用于锚点加速）
    bool                 isValid         = false;
    std::string          errorMessage;
};

// 解析 "E8 ?? ?? ?? ?? 80 38 00 74 16" 形式的特征码
// 支持 '?'、'??'、'**' 作通配；分隔符任意空白
SignaturePattern ParseSignaturePattern(const char* patternText);

// =============================== 扫描 ===============================

struct SignatureHit
{
    uintptr_t relativeAddress = 0;  // 命中处的相对地址（= 段相对地址 + 段内偏移）
    size_t    bufferOffset    = 0;  // 在传入缓冲区内的字节偏移
};

// 在 [memoryData, memoryData + memorySize) 里查找 pattern 的【所有】匹配。
// bufferStartAddress = 该缓冲区起始地址对应的相对地址（用于把偏移换算成相对地址）。
// 返回命中总数（可能大于 hits.size()，如果超过 maximumHitCount 会提前停止）。
size_t ScanMemoryBuffer(const uint8_t* memoryData, size_t memorySize,
                        uintptr_t bufferStartAddress,
                        const SignaturePattern& pattern,
                        std::vector<SignatureHit>& hits,
                        size_t maximumHitCount = 64);

// 一次遍历同时扫多条特征码。比逐条调用 ScanMemoryBuffer 快得多，
// 因为它对整块内存只走一遍（按首字节分桶）。
// hits[第几条特征码] = 那一条的所有命中。
void ScanMemoryBufferForMultiplePatterns(const uint8_t* memoryData, size_t memorySize,
                                         uintptr_t bufferStartAddress,
                                         const SignaturePattern* patterns,
                                         size_t patternCount,
                                         std::vector<std::vector<SignatureHit>>& hits,
                                         size_t maximumHitCountPerPattern = 64);

// =============================== 可执行文件镜像 ===============================

struct PortableExecutableSection
{
    std::string sectionName;
    uint32_t    relativeAddress   = 0;  // 段在内存里的相对地址
    uint32_t    virtualSize       = 0;  // 段在内存里的大小
    uint32_t    rawDataFileOffset = 0;  // 段数据在【文件里】的偏移（注意：不是相对地址！）
    uint32_t    rawDataSize       = 0;  // 段数据在【文件里】的大小
    bool        isExecutable      = false;
};

struct PortableExecutableImage
{
    const uint8_t* imageData      = nullptr;  // 不持有内存（文件/DLL 各自管自己的缓冲）
    size_t         imageSize      = 0;
    uint32_t       imageBaseAddress = 0;

    // 裸内存镜像：文件偏移 == 相对地址（某些 dumper 产出这种布局）
    bool isFlatFileLayout = false;

    // 布局方式。这是本结构体最容易搞错的地方：
    //   isMemoryLayout = false —— 磁盘/dump 文件布局。
    //                            段数据在文件里的位置由 rawDataFileOffset 决定，
    //                            所以「相对地址 -> 指针」要走它换算。
    //   isMemoryLayout = true  —— 已加载进内存的布局（DLL 运行时扫自己所在的进程）。
    //                            此时段数据就在 imageBase + 相对地址 处，
    //                            rawDataFileOffset 完全没有意义
    //                            （它是文件偏移，在活进程里指向垃圾）。
    // 用错了不会崩，只会静默扫错地方 —— 所以两个 Parse 入口要分清楚。
    bool isMemoryLayout = false;

    std::vector<PortableExecutableSection> sections;

    // 按【文件布局】解析 PE 头（不复制数据）—— 磁盘文件 / dump 文件
    // 参数名特意不叫 imageData/imageSize，否则会和同名成员互相遮蔽。
    bool ParseAsFileLayout(const uint8_t* sourceImageData, size_t sourceImageSize);

    // 按【已加载进内存】的布局解析（活进程 / 内存 dump）。
    // 目前【没有调用方】—— 工具走 ParseAsFileLayout 扫磁盘上的 300.exe。
    // 留着是因为它不长，而且「扫一个已经加载的映像」迟早会用到
    // （例如想在 DLL 里自检 offset_update.h 的值和进程里的实际位置对不对）。
    // 真用不上就删掉 —— 连同下面的 isMemoryLayout 和 GetExecutableCodeSection
    // 里那两个分支一起删，别只删一半。
    bool ParseAsMemoryLayout(const uint8_t* sourceImageData, size_t sourceImageSize);

    const PortableExecutableSection* FindSectionContainingAddress(uintptr_t relativeAddress) const;

    bool     ConvertRelativeAddressToPointer(uintptr_t relativeAddress,
                                             const uint8_t** outPointer) const;
    bool     ConvertRelativeAddressToFileOffset(uintptr_t relativeAddress,
                                                size_t& outFileOffset) const;
    uint32_t ConvertFileOffsetToRelativeAddress(size_t fileOffset) const;

    // 便捷：可执行代码段（通常叫 .text）的指针、大小、相对地址。
    // 找不到 .text 时会退化为第一个可执行段。
    bool GetExecutableCodeSection(const uint8_t** outData, size_t& outSize,
                                  uintptr_t& outRelativeAddress) const;

    // 该相对地址是否落在任意已映射段里
    //（用于判断一个立即数像不像「镜像内地址」）
    bool IsAddressInsideImage(uintptr_t relativeAddress) const;

    bool LooksLikePortableExecutable() const
    {
        return !sections.empty() || isFlatFileLayout;
    }
};

// 把整个文件读进内存（独立工具用）
bool ReadEntireFileIntoBuffer(const char* filePath, std::vector<uint8_t>& outBuffer);

} // namespace signature_scan
