#pragma once
// =============================================================================
//  SignatureOutputWriter.h  —— 把解析结果写成 offset_update.h
// -----------------------------------------------------------------------------
//  生成出来的东西就是一堆 constexpr uintptr_t，DLL 里 #include 就能用。
//  常量名直接沿用 offsets_AI.h 顶部原来的名字，所以 DLL 里那些
//      #include "offset_update.h"
//      WriteJmp(模块基址 + Trampoline_HookSendPack_Start, ...)
//  的代码一行都不用改。
//
//  Trampoline 类型生成【两个】常量：
//      <名字>_Start   Hook 点
//      <名字>_End     起点 + 边界（要搬走的那几条指令到此为止）
//  这正好就是原来 offsets.h 里的命名，所以老代码照样编译。
//
//  ★ 文件编码是 UTF-8 带 BOM。
//    工程其余源文件是 UTF-8（build_tool.bat 带 /utf-8），
//    但 offsets_AI.h 是 GBK —— 带 BOM 就能保证不论编译器开不开 /utf-8
//    都不会把中文注释读成乱码。
//
//  ★ 解析失败的条目【不会】写进去。
//    这是故意的：少一个常量 -> DLL 编译直接报「未定义标识符」，很吵；
//    写一个错的值 -> 编译通过、运行时静默出错，很难查。
//    要让失败变吵，不要让它变哑。
// =============================================================================

#include "SignatureDefinitionResolve.h"
#include "SignatureGameVersion.h"

#include <string>
#include <vector>

namespace signature_scan {

struct OffsetUpdateWriteResult
{
    bool        isWritten = false;
    std::string filePath;
    int         writtenConstantCount = 0;   // 实际写了几个常量（Trampoline 算两个）
    std::vector<std::string> failedSignatureNames;
    std::string errorMessage;
};

// outputDirectoryName 结尾不带反斜杠。
bool WriteOffsetUpdateHeader(const char* outputDirectoryName,
                             const char* fileName,
                             const GameVersionInformation& version,
                             const char* sourceImagePath,
                             uintptr_t imageBaseAddress,
                             const std::vector<ResolvedSignature>& resolvedList,
                             OffsetUpdateWriteResult& outResult);

} // namespace signature_scan
