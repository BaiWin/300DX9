#pragma once
// =============================================================================
//  SignatureDefinitionResolve.h  —— 按 offsets_AI.h 里的表解析一条特征码
// -----------------------------------------------------------------------------
//  这一层是「语义」和「字节」之间的翻译：
//
//      offsets_AI.h 里的一条 = { 函数名, 特征码, 类型, 偏移, 边界, 修正, 二级 }
//      类型只有三种（Function / Trampoline / Dword），但三者走的路完全不同，
//      而且 Dword 还要看【操作码】才知道立即数在哪儿、要不要减镜像基址。
//
//  三种类型的走法（详细说明在 offsets_AI.h 的注释块里）：
//
//      Function    有效地址处是 E8/E9 -> 目标 = 有效地址 + 5 + rel32
//      Trampoline  有效地址就是 Hook 点 -> 给出一对 (起点, 起点 + 边界)
//      Dword       有效地址处是引用指令 -> 读指令里的 4 字节
//                  有二级就先按主码反解出函数入口，再从函数入口向下扫二级码
//
//  ★ 为什么「偏移」「边界」都不用手工算指令长度：
//      immediateValueOffset 由操作码推出来 —— A1 是 +1、66 A3 是 +2、
//      8B 86 是 +2…… 让人填就是留一个填错的机会。
//      「边界」反过来，是拿来【校验】推出来的长度的：对不上就报错，
//      免费抓「偏移数错一位」。
//
//  ★ 为什么「减不减镜像基址」不用填：
//      绝对寻址（A1 / 66 A3 / 8B 0D）里的 4 字节是绝对地址 -> 减
//      [寄存器+偏移]（8B 86）里的 4 字节是结构体偏移     -> 不减
//      这一条完全由 ModRM 决定，填是多余的。
// =============================================================================

#include "SignatureConfig.h"   // 必须先来：SIGNATURE_DEFINITION_FILE 这个宏在它里面
#include "SignatureScan.h"

#include <cstdint>
#include <string>
#include <vector>

// 表本身的定义（SignatureDefinition / SignatureKind / SecondarySignature）
// 在 offsets_AI.h 里。这里只引用类型，不重复定义 —— 两份定义是灾难的开始。
#include SIGNATURE_DEFINITION_FILE

namespace signature_scan {

// =============================== 解析状态 ===============================

enum class SignatureResolveStatus
{
    Ok = 0,                  // 命中且唯一，解析成功
    InvalidPattern,          // 特征码字符串本身有问题
    ImageHasNoCodeSection,   // 镜像里找不到 .text
    NotFound,                // 一处都没命中
    NotUnique,               // 命中多处（最危险：这次可能碰巧对）
    NotACallInstruction,     // 偏移指到的地方不是 E8/E9
    NotAReferenceInstruction,// 偏移指到的地方不是一条绝对寻址/带位移的引用指令
    BadBoundaryLength,       // 边界和操作码推出来的指令长度对不上
    SecondaryNotFound,       // 二级码在那个函数里没找到
    SecondaryNotUnique,      // 二级码在那个函数里命中多处
    AddressOutOfCodeSection, // 算出来的地址跑到 .text 外面去了
};

const char* GetResolveStatusText(SignatureResolveStatus status);

// SignatureKind 的文本形式（Function / Trampoline / Dword），打印报告用。
const char* GetSignatureKindText(SignatureKind kind);

// =============================== 解析结果 ===============================

struct ResolvedSignature
{
    const SignatureDefinition* definition = nullptr;

    SignatureResolveStatus status = SignatureResolveStatus::Ok;
    std::string            errorMessage;

    size_t    hitCount           = 0;  // 主码命中几处
    uintptr_t hitRelativeAddress = 0;  // 主码命中处（相对地址）
    uintptr_t effectiveRelativeAddress = 0; // 命中处 + 偏移

    // 只有 Trampoline 才有的第二段：起点 + 边界
    bool      isHookSpan                = false;
    uintptr_t boundaryEndRelativeAddress = 0;

    // 主结果。地址一律是【相对地址】，用的时候要加模块基址。
    bool      hasRelativeAddress = false;
    uintptr_t relativeAddress    = 0;

    // 二级扫描的诊断信息：从函数入口往下前几次命中在哪儿。
    // 「取第一个」这个约定唯一的风险点就是它 —— 打出来就能一眼看出
    // 第二个命中是不是离得太近（离得近说明二级码太短，换个版本可能前移）。
    std::vector<uintptr_t> secondaryHitRelativeAddresses;
    uintptr_t              secondaryFunctionEntryRelativeAddress = 0;

    // 二级扫描的【边界】：函数入口 -> 第一条 ret 之后。
    // 这些函数都是固定的底层小函数（只有一个 ret），所以 ret 就是函数体末尾。
    // 有了它，「取第一个」才真的安全 —— 函数外的同名指令不会被算进来。
    // 0 表示没找到 ret，那时退回到 64KB 上限。
    uintptr_t secondaryFunctionEndRelativeAddress = 0;

    // 落在函数体【外面】的同名命中数。不算错，但要知道有。
    // 计数值 > 0 说明这条二级码本身不唯一，全靠 ret 边界兜住的 ——
    // 换个版本如果函数变复杂了（多一个 ret），这条就可能选错。
    size_t secondaryHitCountBeyondFunctionEnd = 0;

    bool IsOk() const { return status == SignatureResolveStatus::Ok; }
};

// 解析一条。image 由调用方解析好再传进来 —— 文件布局或内存布局都行，
// PortableExecutableImage 自己知道是哪种。
ResolvedSignature ResolveSignatureDefinition(const PortableExecutableImage& image,
                                             const SignatureDefinition& definition);

// 按名字取一条解析结果。找不到返回 nullptr。
const ResolvedSignature* FindResolvedSignature(const std::vector<ResolvedSignature>& resolvedList,
                                               const char* signatureName);

} // namespace signature_scan
