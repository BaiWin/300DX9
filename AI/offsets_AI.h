#pragma once

constexpr uintptr_t offset_func_castskill = 0xAFEE60;
//84 C0 74 1B FF 75 0C 8D 46 0B FF 75 10 6A 00 50 E8
//最后一个指令就是callxxx，取到这个xxx

constexpr uintptr_t offset_func_castSummonerSkill = 0x774460;
//66 39 47 0C 75 3C FF B5 74 FF FF FF 53 FF 76 14 8B CE E8 ?? ?? ?? ?? 8B C8 E8
//最后一个指令就是callxxx，取到这个xxx

constexpr uintptr_t offset_func_treeFindPlayerObj = 0x601F0;
//81 FE FF FF FF 7F 0F 84 D9 00 00 00 8D 45 A0 89 75 A0 50 8D 45 A4 8D 8F 94 00 00 00 50 E8
//最后一个指令就是callxxx，取到这个xxx

constexpr uintptr_t offset_func_getCDRecordObj = 0x81D260;
//E8 ?? ?? ?? ?? 80 38 00 74 16
//第一个指令就是callxxx，取到这个xxx

//********************************************

//.text:                 mov     ecx, [eax + 8]
//.text :                 mov     eax, [eax + 4]
//.text:                 mov[ebp + 44Ch + var_470], ecx
constexpr uintptr_t Trampoline_CastNormalSkillRecvCD_Start = 0xAFDBD1;
constexpr uintptr_t Trampoline_CastNormalSkillRecvCD_End = 0xAFDBD7;
//8B 48 08 8B 40 04 89 4D DC 89 45 F0 E8 ?? ?? ?? ?? 8B 4D F0 8B 55 DC
//Start就是第一个指令处 边界长度6

// 81D679 
// 81D67F 
//.text:                 mov     eax, [esp+10h+arg_4] esp + 0x18
//.text:                 mov[eax], esi
//.text:                 call    sub_7C890
constexpr uintptr_t Trampoline_SwitchSkill_Start = 0x824B59;
constexpr uintptr_t Trampoline_SwitchSkill_End = 0x824B5F;
//8B 44 24 18 89 30 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 0F B7 C0 50 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 57 50 B9
//Start是的第一个指令处 边界长度6

//********************************************
// SendPack 9.23
//.text:001DFAE4                 movzx   eax, word ptr[esi + 8]
//.text : 001DFAE8                 mov[ebp + var_E4], eax
//.text : 001DFAEE                 call    sub_D70E70
constexpr uintptr_t Trampoline_HookSendPack_Start = 0x1DFAE4;
constexpr uintptr_t Trampoline_HookSendPack_End = 0x1DFAEE;
//0F B7 46 08 89 85 1C FF FF FF E8 ?? ?? ?? ?? 84 C0 74 68
//Start是的第一个指令处 边界长度10

//********************************************

constexpr uintptr_t dword_SkillTable = 0x1A6A4B8;
//E8 ?? ?? ?? ?? 89 70 48
//首指令就是call，进入call进行二级扫描
//A1 ?? ?? ?? ?? 8B 4D F4 64 89 0D 00 00 00 00 59 5E 8B E5 5D C3
//首指令就是mov eax, dword_1A6A4B8，取到这个dword

constexpr uintptr_t dword_SkillTable_Slot_SkillIDOffset = 0x46F38;
//66 A3 ?? ?? ?? ?? B8 02 00 00 00 66 A3
//首指令的上一条指令就是mov ax, word ptr ds:dword_46F38[esi]，6字节，取到dword

//.text:00816704    mov     word ptr dword_1E12B28+2, ax
constexpr uintptr_t dword_CurrentProcessed_SkillID = 0x1E2D670;
//66 A3 ?? ?? ?? ?? B8 02 00 00 00 66 A3
//首指令就是mov word ptr dword_1E2D670+2, ax，取到dword

constexpr uintptr_t dword_Clock = 0x1A70C40;
//E8 ?? ?? ?? ?? 80 38 00 74 39 8B 70 08 03 70 04 E8
//尾指令就是call，进入call进行二级扫描
//A1 ?? ?? ?? ?? C3
//首指令就是mov eax, dword_1A70C40，取到这个dword

constexpr uintptr_t dword_HoverStruct = 0x1A6A76C;
//E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? F3 0F 10 90 84 00 00 00 F3 0F 10 88 88 00 00 00 F3 0F 10 80 8C 00 00 00
//首指令就是call，进入call进行二级扫描
//A1 ?? ?? ?? ?? 8B 4D F4 64 89 0D 00 00 00 00 59 5E 8B E5 5D C3
//首指令就是mov eax, dword_1A6A76C，取到这个dword




//表的结构
//offset_func_castskill 变量名
//84 C0 74 1B FF 75 0C 8D 46 0B FF 75 10 6A 00 50 E8 特征码
//特征码偏移
//类型分三类{Function, Trampoline, Dword}
//Trampoline边界
//二级特征码结构{特征码，特征码偏移}


// =============================================================================
//  特征码表
// -----------------------------------------------------------------------------
//  顶部那些 constexpr 保持原样（老代码还在引用）。这张表是唯一需要手工维护的地方。
//
//  字段顺序：函数名 / 特征码 / 类型 / 偏移 / 边界 / 修正 / 二级
//
//    偏移   命中处 -> 有效地址 的字节差，可为负。
//           有效地址处那条指令就是突破口，三种类型都从这里开始解释。
//
//    边界   从【最终有效地址】起算的那条指令（或那一段）的字节数。
//           Function   -> 那条 call，5 字节
//           Trampoline -> 要重放到蹦床里的那段，就是 End - Start
//           Dword      -> 真正读出值的那条指令（有二级时是二级那一侧）
//                         A1 + imm32 = 5、66 A3 + imm32 = 6、8B 86 + disp32 = 6
//                         （别写成 66 8B 86 —— 那是 7 字节，66 只在操作数是
//                           ax/cx 这种 16 位寄存器时才出现，参见 README 坑 7）
//           工具按操作码自己推一遍长度和它对，对不上就报错
//           —— 免费抓「偏移数错一位」。
//
//    修正   从指令里读出的 4 字节要加的修正值，默认 0。
//           只有指令里的值本身不是你要的那个数时才用：
//           例 mov word ptr [X+2], ax 读到的是 X+2，修正 -2 才回到 X。
//
//    二级   只有「进 call 取」的 Dword 才填。填了就先按主码反解出函数入口，
//           再从函数入口向下扫二级码，取【第一个】命中
//           （这些函数格式固定、偏底层、只有一个 ret）。
//           二级为空(nullptr) = 直接取，不进 call。
//
//  三种类型怎么走：
//    Function   有效地址处是一条 E8/E9 call
//               目标 = 有效地址 + 5 + rel32
//    Trampoline 有效地址就是 Hook 点的起点，结果是一对地址
//               起点 = 有效地址，终点 = 有效地址 + 边界
//               （写 E8 之前要先确认 边界 >= 5 且落在指令边界上）
//    Dword      有效地址处是一条引用指令
//               值 = 从 (有效地址 + 操作码推出的偏移) 处读 4 字节，再加修正
//               ★ 减不减 ImageBase 由【操作码】决定，表里不用填：
//                 绝对寻址      A1 / 66 A3 / 8B 0D ... -> 读出来是绝对地址   -> 减
//                 [寄存器+偏移] 8B 86 ...              -> 读出来是结构体偏移 -> 不减
//
//  同一处命中可以出两个元素，靠【偏移】区分，互不冲突：
//    dword_CurrentProcessed_SkillID        用偏移  0 取命中处那条指令里的值
//    dword_SkillTable_Slot_SkillIDOffset   用偏移 -6 取上一条指令里的值
//  它们共用同一条特征码（同一条码只有一个命中处）。
// =============================================================================

enum class SignatureKind
{
    Function   = 0,   // 取 call 的目标
    Trampoline = 1,   // 取一段指令的首尾
    Dword      = 2,   // 取指令里的 4 字节
};

struct SecondarySignature
{
    const char* patternText;     // 二级特征码；nullptr = 不进 call，直接取
    int         patternOffset;   // 二级命中处 -> 二级有效地址
};

struct SignatureDefinition
{
    const char*        signatureName;       // 函数名（和顶部 constexpr 同名，方便对账）
    const char*        patternText;         // 特征码
    SignatureKind      signatureKind;       // 类型
    int                patternOffset;       // 偏移
    int                boundaryLength;      // 边界
    int                valueAdjustment;     // 修正
    SecondarySignature secondarySignature;  // 二级
};

constexpr SignatureDefinition kSignatureTable[] =
{
    // =====================================================================
    //  一、Function —— 有效地址处是一条 call，取它的目标
    // =====================================================================

    {
        // 原 offsets.h: offset_func_castskill = 0xAFEE60
        // 末尾那个 E8 就是那条 call，前面是给它压参数的上下文。
        "offset_func_castskill",
        "84 C0 74 1B FF 75 0C 8D 46 0B FF 75 10 6A 00 50 E8",
        SignatureKind::Function, 16, 5, 0, { nullptr, 0 }
    },
    {
        // 原 offsets.h: offset_func_castSummonerSkill = 0x774460
        // 末尾的 E8 才是要的那条 call —— 模式里偏移 18 处还有另一条 call 的 E8，
        // 别数错。
        "offset_func_castSummonerSkill",
        "66 39 47 0C 75 3C FF B5 74 FF FF FF 53 FF 76 14 8B CE E8 ?? ?? ?? ?? 8B C8 E8",
        SignatureKind::Function, 25, 5, 0, { nullptr, 0 }
    },
    {
        // 原 offsets.h: offset_func_treeFindPlayerObj = 0x601F0
        // 30 字节里只有最后 1 个字节是 call，前 29 字节是调用点的上下文。
        "offset_func_treeFindPlayerObj",
        "81 FE FF FF FF 7F 0F 84 D9 00 00 00 8D 45 A0 89 75 A0 50 8D 45 A4 8D 8F 94 00 00 00 50 E8",
        SignatureKind::Function, 29, 5, 0, { nullptr, 0 }
    },
    {
        // 原 offsets.h: offset_func_getCDRecordObj = 0x81D260
        // 这条反过来：call 在【第一个】字节，所以偏移是 0。
        "offset_func_getCDRecordObj",
        "E8 ?? ?? ?? ?? 80 38 00 74 16",
        SignatureKind::Function, 0, 5, 0, { nullptr, 0 }
    },

    // =====================================================================
    //  二、Trampoline —— 有效地址就是 Hook 点，结果是一对 (起点, 起点+边界)
    // =====================================================================

    {
        // 原 offsets.h: Trampoline_CastNormalSkillRecvCD_Start = 0xAFDBD1
        //               Trampoline_CastNormalSkillRecvCD_End   = 0xAFDBD7
        // 重放 mov ecx,[eax+8] / mov eax,[eax+4] / mov [ebp-..],ecx
        // 边界 6 = End - Start
        "Trampoline_CastNormalSkillRecvCD",
        "8B 48 08 8B 40 04 89 4D DC 89 45 F0 E8 ?? ?? ?? ?? 8B 4D F0 8B 55 DC",
        SignatureKind::Trampoline, 0, 6, 0, { nullptr, 0 }
    },
    {
        // 原 offsets.h: Trampoline_SwitchSkill_Start = 0x824B59
        //               Trampoline_SwitchSkill_End   = 0x824B5F
        // 重放 mov eax,[esp+18h] / mov [eax],esi
        // 边界 6 = End - Start
        "Trampoline_SwitchSkill",
        "8B 44 24 18 89 30 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 0F B7 C0 50 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 57 50 B9",
        SignatureKind::Trampoline, 0, 6, 0, { nullptr, 0 }
    },
    {
        // 原 offsets.h: Trampoline_HookSendPack_Start = 0x1DFAE4
        //               Trampoline_HookSendPack_End   = 0x1DFAEE
        // 重放 movzx eax,word ptr[esi+8] / mov [ebp+var_E4],eax
        // 边界 10 = End - Start
        "Trampoline_HookSendPack",
        "0F B7 46 08 89 85 1C FF FF FF E8 ?? ?? ?? ?? 84 C0 74 68",
        SignatureKind::Trampoline, 0, 10, 0, { nullptr, 0 }
    },

    // =====================================================================
    //  三、Dword —— 有效地址处是一条引用指令，取它里面的 4 字节
    // =====================================================================

    {
        // 原 offsets.h: dword_SkillTable = 0x1A6A4B8
        // 主码偏移 0 是一条 call -> 进那个函数；
        // 二级扫到 mov eax, dword_1A6A4B8（A1 是绝对寻址，读出来要减 ImageBase）。
        "dword_SkillTable",
        "E8 ?? ?? ?? ?? 89 70 48",
        SignatureKind::Dword, 0, 5, 0,
        { "A1 ?? ?? ?? ?? 8B 4D F4 64 89 0D 00 00 00 00 59 5E 8B E5 5D C3", 0 }
    },
    {
        // 原 offsets.h: dword_SkillTable_Slot_SkillIDOffset = 0x46F38
        // 偏移 -6 指到【上一条】指令的起点：mov ax, word ptr [esi+46F38h]，6 字节。
        // ★ 这是 [寄存器+偏移] 形式，读出来的 46F38 是结构体偏移不是地址，
        //   所以自动【不减 ImageBase】，修正也是 0。
        // 和下面 dword_CurrentProcessed_SkillID 共用同一条特征码，靠偏移区分：
        // 这条取上一条指令，那条取命中处这条。
        "dword_SkillTable_Slot_SkillIDOffset",
        "66 A3 ?? ?? ?? ?? B8 02 00 00 00 66 A3",
        SignatureKind::Dword, -6, 6, 0, { nullptr, 0 }
    },
    {
        // 原 offsets.h: dword_CurrentProcessed_SkillID = 0x1E2D670
        // 偏移 0 = 命中处那条 mov word ptr [X+2], ax（66 A3 是 moffs 绝对寻址，6 字节）。
        // 指令里的立即数是 X+2，所以要修正 -2 才回到 X。
        "dword_CurrentProcessed_SkillID",
        "66 A3 ?? ?? ?? ?? B8 02 00 00 00 66 A3",
        SignatureKind::Dword, 0, 6, -2, { nullptr, 0 }
    },
    {
        // 原 offsets.h: dword_Clock = 0x1A70C40
        // 主码偏移 16 是【末尾】那条 call -> 进那个函数；
        // 二级扫到 mov eax, dword_1A70C40。
        "dword_Clock",
        "E8 ?? ?? ?? ?? 80 38 00 74 39 8B 70 08 03 70 04 E8",
        SignatureKind::Dword, 16, 5, 0,
        { "A1 ?? ?? ?? ?? C3", 0 }
    },
    {
        // 原 offsets.h: dword_HoverStruct = 0x1A6A76C
        // 主码偏移 0 是【首个】那条 call -> 进那个函数；
        // 二级扫到 mov eax, dword_1A6A76C。
        //
        // 二级码和 dword_SkillTable 那条一模一样 —— 不冲突，因为二级的扫描范围是
        // 【各自那个函数】，各自取函数里的第一个命中。
        "dword_HoverStruct",
        "E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? F3 0F 10 90 84 00 00 00 F3 0F 10 88 88 00 00 00 F3 0F 10 80 8C 00 00 00",
        SignatureKind::Dword, 0, 5, 0,
        { "A1 ?? ?? ?? ?? 8B 4D F4 64 89 0D 00 00 00 00 59 5E 8B E5 5D C3", 0 }
    },
};

constexpr unsigned kSignatureTableCount =
    (unsigned)(sizeof(kSignatureTable) / sizeof(kSignatureTable[0]));
