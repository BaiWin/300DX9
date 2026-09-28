#pragma once
#include "300.h"

//参数上方是初始 8.28 的分析；参数下的是最新，包括signature

//sub AF6E40   8.28
constexpr uintptr_t offset_func_castskill = 0xAFEE60;
//F3 0F 10 44 24 14 F3 0F 11 40 30 F3 0F 10 44 24 10 F3 0F 11 40 34 C7 40 38 00 00 00 00
//找方法名

//********************************************

// 9.11
constexpr uintptr_t offset_func_castSummonerSkill = 0x774460;
//89 7D D0 89 45 C4 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 83 FF FF


// 9.24  7AB972的位置
constexpr uintptr_t offset_func_treeFindPlayerObj = 0x601F0;
//81 FE FF FF FF 7F 0F 84 D9 00 00 00 8D 45 A0 89 75 A0 50 8D 45 A4 8D 8F 94 00 00 00 50 E8
// if上面的一行，即 ((void (__stdcall *)(float *, float *))aaaTreeFind_601F0)(&v178, &v177); 
// 就是函数 601F0
/*
// base = 模块基址
typedef void* (__thiscall *TFindLowerBound)(void* thisP94, void* out12, const uint32_t* key);

uint32_t* id  = (uint32_t*)(P + 0x7C);          // 悬停 ID
uint8_t   out[12] = {0};                        // 12 字节，清零
void*     node = nullptr;

__try {                                         // 建议包 SEH，读野指针不会带崩自己
    ((TFindLowerBound)(base + 0x601F0))((void*)(P + 0x94), out, id);
    node = *(void**)(out + 8);                  // ← lower_bound 节点在 +0x08
} __except (EXCEPTION_EXECUTE_HANDLER) { node = nullptr; }

void* entity;
if (node && *(uint8_t*)((uint8_t*)node + 0x0D) == 0        // +0x0D = _Isnil
         && *(uint32_t*)((uint8_t*)node + 0x10) == *id)    // +0x10 = key，必须等值才算命中
    entity = *(void**)((uint8_t*)node + 0x14);             // +0x14 = 实体指针
else
    entity = *(void**)(P + 0x100);                         // 游戏自己的兜底（占位实体）
*/

// 9.24
constexpr uintptr_t offset_func_getCDRecordObj = 0x81D260;
//E8 ?? ?? ?? ?? 80 38 00 74 16  进入位置方法里面

//9.24
constexpr uintptr_t offset_func_moveToPoint = 0x83F920;
//C7 44 24 18 00 00 00 00 8B 44 24 18 66 0F D6 0A 89 42 08 E8 最后一个call就是函数

//********************************************

// AF5BB1 
// AF5BB7
//.text:00AF5BB1                 mov     ecx, [eax + 8]
//.text : 00AF5BB4                 mov     eax, [eax + 4]
//.text:00AF5BB7                 mov[ebp + 44Ch + var_470], ecx
constexpr uintptr_t Trampoline_CastNormalSkillRecvCD_Start = 0xAFDBD1;
constexpr uintptr_t Trampoline_CastNormalSkillRecvCD_End = 0xAFDBD7;
//8B 48 08 8B 40 04 89 4D DC 89 45 F0 E8 ?? ?? ?? ?? 8B 4D F0 8B 55 DC
// New:
//.text:AFCEA1                 mov     ecx, [eax+8]
//.					                mov     eax, [eax + 4]
//.text : AFCEA7                 mov[ebp + 44Ch + var_470], ecx

//********************************************

// 81D679 
// 81D67F 
//.text:0081D679                 mov     eax, [esp+10h+arg_4] esp + 0x18
//.text:0081D67D                 mov[eax], esi
//.text:0081D67F                 call    sub_7C890
constexpr uintptr_t Trampoline_SwitchSkill_Start = 0x824B59;
constexpr uintptr_t Trampoline_SwitchSkill_End = 0x824B5F;
//0F B7 C0 50 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 57 50 B9  在这段的上方
// New:
//.text:00824259                 mov     eax, [esp + 10h + arg_4]
//.text:0082425D                 mov[eax], esi
//.text:0082425F                 call    sub_7C890

//********************************************
// SendPack 9.23
//.text:001DFAE4                 movzx   eax, word ptr[esi + 8]
//.text : 001DFAE8                 mov[ebp + var_E4], eax
//.text : 001DFAEE                 call    sub_D70E70
constexpr uintptr_t Trampoline_HookSendPack_Start = 0x1DFAE4;
constexpr uintptr_t Trampoline_HookSendPack_End = 0x1DFAEE;
//8B 75 08 C7 85 28 FF FF FF 00 00 00 00 85 F6 0F 84 95 0B 00 00 0F B7 46 08 89 85 1C FF FF FF
//********************************************

// 9.23
constexpr uintptr_t MinHook_SkillTypeCheck = 0x87A2B0;
//E8 ?? ?? ?? ?? 83 C4 04 84 C0 74 5A 8B 87 24 03 00 00 第一个call

// 9.23
constexpr uintptr_t MinHook_TrySelectSkillTarget = 0x7A98C0;
//E8 ?? ?? ?? ?? 83 F8 FF 74 81 3D FF FF FF 7F 第一个call

//1A51398 // SUB 6E620
constexpr uintptr_t dword_SkillTable = 0x1A6A4B8;
//E8 ?? ?? ?? ?? 89 70 48  进入call的方法里，取到最后return的dword
//1A474B8

//0x46EF8  // .text:00816704    mov     word ptr dword_1E12B28+2, ax   // 8.26 sub_8166A0
constexpr uintptr_t dword_SkillTable_Slot_SkillIDOffset = 0x46F38;
//8D 04 40 8D 04 45 DB 0D 00 00 03 C1 8B 4D F4 64 89 0D 00 00 00 00 向下找到 //word ptr dword_1E12B28+2, ax 的上面一行的DWORD
//0x46EF4

//.text:00816704    mov     word ptr dword_1E12B28+2, ax
constexpr uintptr_t dword_CurrentProcessed_SkillID = 0x1E2D670;
//66 8B 86 F8 6E 04 00 66 A3 ?? ?? ?? ?? B8 02 00 00 00  找HIGHWORD的DWORD
//或者搜索8D 04 40 8D 04 45 DB 0D 00 00 03 C1，找下面的66 8B 86 F8 6E...
//1E08CD0

constexpr uintptr_t dword_Clock = 0x1A70C40;
//E8 ?? ?? ?? ?? 80 38 00 74 39 8B 70 08 03 70 04 E8 ?? ?? ?? ?? 2B F0   v23 = v22 - aaaGetClock_1C11B0();
// 进入方法里找dword


//------玩家位置的获取-------
//v9.11 
constexpr uintptr_t dword_HoverStruct = 0x1A6A76C;
//查找1：(观察过程在这里看)
//53 56 6A FF 8B CD E8 ?? ?? ?? ?? B0 01 5F 5E 5D 5B 8B E5 5D C3
//该函数的首参数也会用到dword_PlayerInfo
//查找2：(取偏移这里更方便)
//E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? F3 0F 10 90 84 00 00 00 F3 0F 10 88 88 00 00 00 F3 0F 10 80 8C 00 00 00
//找到 v23 = (_DWORD *)sub_7C890();   //DWORD 在这里，到sub_7C890里找
//     v24 = (float*)sub_7ACFE0(v23);    这里进入函数后可以看到是+44的偏移
//查找3：
//E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 8A 80 06 43 00 00 3A 86 06 43 00 00 74 22 EB 27


//struct HoverHost
//{                  // P = [[0x1A6A76C]]，由 0x7C890 取
//    /*+0x00*/ BYTE   pad00[0x24];
//    /*+0x24*/ int    unk24;           // 全程 0；被挑选器 0x7A9BB0 读 —— 不是鼠标坐标
//    /*+0x28*/ int    unk28;           // 同上
//    /*+0x2C*/ BYTE   pad2C[0x7C - 0x2C];
//    /*+0x7C*/ int    hoverId;         // ★ 悬停实体 ID；0x7FFFFFFF = 无目标      位置 9.23 7AB952
//    /*+0x40*/                         // 玩家自己的id
//    /*+0x44*/                         // 玩家自己的实体指针
//    /*+0x80*/ BYTE   pad80[0x94 - 0x80];
//    /*+0x94*/ void* entMap;          // ★ std::map<int,Entity*> 的 _Myhead        位置  9.23 07AB972
//    /*+0x98*/ BYTE   pad98[0x100 - 0x98];
//    /*+0x100*/ Entity* unkEntity;     // 兜底实体：类型 0xFFFF、ID=0xFFFFFFFF、血 5000、坐标 162.5/163.5；map 空时被当"自己"取 +0x4580 bit0x40
//    /*+0x104*/ BYTE  pad104[...];
//    /*+0xF5C*/ ptr commandHolder      // C7 44 24 18 00 00 00 00 8B 44 24 18 66 0F D6 0A 89 42 08 E8
//};










// 疾步 1F5C
// 闪现 1F63
// 治疗 1F5D
// 重生 1F57

// 果子/修仙道具 0x42 66  修仙发的槽位信息
// 0X41  65
// 0X40  64






// TreeFind 节点布局	_Left+0 / _Parent+4 / _Right+8 / _Color+0xC / _Isnil+0xD / key +0x10 / value +0x14

//struct Entity
//{                     // 实测 ≥ 0x4584 字节
//    /*+0x00*/ void* pad00;           // 实测 0（无 vtable，RTTI 解不出类名）
//    /*+0x04*/ DWORD  pad04;           // 0
//    /*+0x08*/ DWORD  pad08;           // 玩家 0x3FA2F701 / 假人 0
//    /*+0x0C*/ DWORD  pad0C[10];       // 含 0xFFFFFFFF×4，未逆
//    /*+0x34*/ DWORD  pad34;           // 15（和 +0x528 同值，疑似等级）
//    /*+0x38*/ BYTE   pad38[0x3C];
//    /*+0x74*/ float  yaw1;            // 朝向弧度（玩家 4.71 / 假人 1.57）
//    /*+0x78*/ float  yaw2;            // 同值
//    /*+0x7C*/ float  yaw3;            // 同值
//    /*+0x80*/ float  footZ;           // 0.40
//    /*+0x84*/ float  x;               // ★ 地图坐标 X
//    /*+0x88*/ float  y;               // ★ 地图坐标 Y
//    /*+0x8C*/ float  z;               // ★ 地图坐标 Z（0.40）
//    /*+0x90*/ float  yaw4;            // 朝向弧度
//    /*+0x94*/ float  pad94[3];        // 0,0,0
//    /*+0xA0*/ float  yaw5;            // 同 +0x90
//    /*+0xA4*/ float  x2;              // ★ 坐标第二份副本 X
//    /*+0xA8*/ float  y2;              // ★ 副本 Y
//    /*+0xAC*/ float  z2;              // ★ 副本 Z
//    /*+0xB0*/ float  range;           // 40000.0（视野/范围？）
//    /*+0xB4*/ float  padB4;           // 玩家 1.522 / 假人 nan / 0
//    /*+0xB8*/ BYTE   padB8[0x42D0 - 0xB8];
//    /*+0x4036*/                        阵营：1/2 = 两个阵营，3 = 中立，0 = 无效/未同步     // 不要硬编码"1 是我方、2 是敌方"——自己站哪边就可能是哪个值，必须拿自己的实体去比。 // 在 0x7AB998 处   9.23
//    /*+0x42D0*/ int  hpCopy1;         // 22312（玩家）/ 900000（假人）← 你给的偏移，是对的
//    /*+0x42D4*/ int  hpCopy2;         // 同值
//    /*+0x42D8*/ BYTE pad42D8[0x4518 - 0x42D8];
//    /*+0x4518*/ int  id;              // ★ 实体 ID（= [P+0x94] map 的 key）
//    /*+0x451C*/ BYTE pad451C[0x4580 - 0x451C];
//    /*+0x4580*/ DWORD flags;          // 玩家 0xFFFF0045 / 假人 0x00200015；bit0x40 = 可行动/可锁定
//    /*+0x4582*/ WORD  type;           // 0xFFFF 玩家 / 0x20 假人 / 0x19 小兵/ 0x17,0x1B,0x1E,0x1C,0x10 其他/建筑
//    /*+0x4584*/ BYTE  pad4584[0x45C0 - 0x4584];
//    /*+0x45C0*/ void* sub45C0;        // 筛选器类型 0x1B 分支把 &[e+0x45C0] 传给 0x1C0520
//    /*+0x45C4*/ BYTE  pad45C4[0x494 - 0x45C4];
//    /*+0x494*/ float  pickX;          // 每帧被筛选器写（拾取/投影点 X）
//    /*+0x498*/ float  pickY;
//    /*+0x49C*/ float  depth;          // 悬停时被写（玩家 2.08 / 假人 1.74）
//    /*+0x4A0*/ BYTE   pad4A0[0x4E4 - 0x4A0];
//    /*+0x4E4*/ float  highlight;      // 高亮 alpha：1.0 / 0.5 / 0.0
//    /*+0x4E8*/ BYTE   pad4E8[0x528 - 0x4E8];
//    /*+0x528*/ int    lv;             // 15
//    /*+0x52C*/ int    pad52C;         // -1
//    /*+0x530*/ int    hpMax;          // 22312 / 900000
//    /*+0x534*/ int    hpCur;          // ★ 实时血量 22294 / 894808
//    /*+0x538*/ int    hpMax2;         // 22312 / 900000
//    /*+0x53C*/ int    pad53C;         // 玩家 200 / 假人 1
//    /*+0x540*/ BYTE   pad540[8];
//    /*+0x548*/ int    pad548;         // 1
//    /*+0x54C*/ int    pad54C;         // 120
//    /*+0x550*/ ...
//};



//结构① 技能数据表对象 G
//
//G([[RVA 0x1A6A4B8]] ，懒加载单例，new 的尺寸 0x47880)
//+ 0x000  map1.head      // MSVC map = {head@+0, size@+4}；空表时 head 三指针自指
//+ 0x004  map1.size
//+ 0x008  map2.head      // ctor 里第二个初始化的就是它（0x81C1AA lea esi,[edi+8]）
//+ 0x00C  map2.size
//+ 0x014  ★CD表.head     // map<u16 技能id, CD记录>  ← 查CD就是查这张表
//+ 0x018  CD表.size
//+ 0x024  ★CD默认记录     // 查不到时返回它，valid=0
//+ 0x040  int64 语义未确认  // 写:0x81D4D0  读:0x81D4E0
//+ 0x048  dword            // 写:0x6977DA（dword，不是 word）
//+ 0x155  word             // 写:0x6977B8
//+ 0x46F38 u32[17]         // 槽位0..16 → 技能id（低16位），-1 = 空
//+ 0x00DDB 6字节×N         // 槽位17..25（天赋槽）
//位置	opcode	注释
//0x6E653	A1 BC A4 46 02	mov eax, [0x1A6A4BC] ← 守卫全局；0x6E658 3B 86 28 00 00 00 与[tls + 0x28] 比，未初始化就构造
//0x6E660	A1 B8 A4 46 02	mov eax, [0x1A6A4B8] ← 读 G（getter 的返回路径）
//0x6E6A2	68 80 78 04 00 / 0x6E6A7 C7 86 08 00 00 00 80 78 04 00	push 0x47880 / mov[esi + 8], 0x47880 ← 对象尺寸（注意：这个立即数在别处也出现 101 次，见锚点表说明）
//0x6E6B1	E8 C5 9F 1B 01	call 0x122867B = operator new
//0x6E6C6	E8 85 DA 7A 00	call 0x81C150 = G 的构造函数（thiscall，ecx = 新对象）
//0x6E6D1	C6 45 FC 00	mov byte[ebp - 4], 0 ← 守卫置位
//0x6E6D5	A3 B8 A4 46 02	mov[0x1A6A4B8], eax ← G 全局的唯一写入点
//0x6E6E1	68 BC A4 46 02	push 0x1A6A4BC ← 注册析构
//0x81C19B	66 C7 40 0C 01 01	mov word[eax + 0xC], 0x101 ← map 哨兵节点的 _Color / _Isnil
//0x81C1AA	8D 77 08	lea esi, [edi + 8] ← 第二个 map 头在 G + 8
//0x81C1B0	6A 20	push 0x20 ← 该 map 的节点 = 0x20 字节
//0x81C1BF	E8 B7 C4 A0 00	call operator new
//0x81C1C4	89 00 89 40 04	mov[eax], eax; mov[eax + 4], eax ← 空表哨兵自指
//0x6977B8	66 89 B0 55 01 00 00	mov word[eax + 0x155], si ← G + 0x155（eax = 刚 call getter 的返回）
//0x6977DA	89 70 48	mov dword[eax + 0x48], esi ← G + 0x48
//0x6977DD	66 83 7F 4F 02	cmp word[edi + 0x4F], 2 ← 这个 0x4F 是别的对象(edi)，不是 G，别混
//0x81D4D0	8B 44 24 04 99 89 41 40 89 51 44	写 G + 0x40 / +0x44（int64），调用点 0x6977CC
//0x81D4E0	8B 41 40 C3	读 G + 0x40
//结构② CD 表节点 + CD 记录
//
//CDNode(0x20 字节)              // std::map<u16 技能id, CD记录> 的红黑树节点
//+ 0x00  _Left
//+ 0x04  _Parent                  // ★根 = [head+4]，head = [G+0x14]
//+ 0x08  _Right
//+ 0x0C  _Color
//+ 0x0D  _Isnil                   // 查表时先判它
//+ 0x10  u16 技能id(key)         // ★u16 **无符号**比较
//+ 0x12  u16 未知
//+ 0x14  CDRecord  ★内联，不是指针
//
//CDRecord(12 字节)
//+ 0x00  u8  valid                // 过期也不清零，别只看它
//+ 0x01  pad[3]
//+ 0x04  u32 开始ms
//+ 0x08  u32 时长ms               // ← "CD在+8" 指的就是它




//结构 运行时技能对象 SkillRuntime（operator new(0x398)，共 0x398 字节）
//偏移      类型          字段                     来源 / 说明
//------------------------------------------------------------------------------------
//+ 0x000    void* pVTable                  虚表指针
//+ 0x00C    u16           nSkillID         ← 配置 f1(nSkillID)
//+ 0x00E    u16           nSkillLevel      ← 配置 f2(nLevel)
//+ 0x010    u16                            ← 配置 f23(nProfessionSkillForSkillUI)
//+ 0x012    u16                            ← 配置 f24(nProfessionSkillForLearnSkillUI)
//+ 0x014    u32   ★★★   nObjectEnum 位掩码   ← 配置 f27，"3;6;7" 型字符串 bts 而成
//bit0 = 自身 bit1 = 友方单位 bit2 = 友方小兵
//bit3 = 敌方小兵 bit4 = 友方英雄 bit5 = 野怪
//bit6 = 敌方英雄 bit7 = 建筑 / 守卫 bit8 = 炮塔
//+ 0x018    u32(掩码区紧邻，语义未定)
//+ 0x01C    std::string(24B)             实测技能301 -> "single_attack_attcom"
//+ 0x07C    u8(某 bool，上面那串分隔串处理时被置 0 / 1)
//+ 0x080    std::string(24B)             ← 配置 f ? （0x11D9EE 处赋值）
//+ 0x098    std::string(24B)             ← 配置 f ? （0x11DA05 处赋值）
//+ 0x0BC    std::string(24B)             ← 配置 f ? （0x11DA1C 处赋值）
//+ 0x0EC    std::string(24B)             ← 配置 f ? （0x11DA2D 处赋值）
//+ 0x134    std::string(24B)             ← 配置 f47
//实测技能301 -> "skill\070_mingren\s1_putonggongji\hit\tx"
//+ 0x194    u16                             ← 配置 f26 nDragtoNewSkill  ★不是掩码！实测恒 0
//+ 0x196    u16           nSkillActPlaySpd_Male    ← 配置 f29(实测 533 / 1200 / 800 与 json 逐条吻合)
//+ 0x198    u16           nSkillActPlaySpd_Female  ← 配置 f30
//+ 0x19C    u32           nSkillTypeForUI          ← 配置 f22
//+ 0x1A0    u8            bPassive                 ← 配置 f25
//+ 0x1A4    u32           nSkillColddownType       ← 配置 f31(实测恒 0xFFFFFFFF，存疑)
//+ 0x1A8    float         fAngle × π               ← 配置 f11(技能7 = 2π / 技能1 = π)
//+ 0x1AC    float  ★★★   fRange  施法距离          ← 配置 f8(0.0 / 2.2 / 4.0 / 5.0 / 6.0 / 99.0 / 999.0)
//+ 0x1B0    float         fTargetRange             ← 配置 f58(实测 2.2 吻合)
//+ 0x1B4    float                                   ← 配置 f78 槽（类型矛盾，未决）
//+ 0x1B8    u32           nCDTime                  ← 配置 f18
//+ 0x1BC    u32           nPrepareTime             ← 配置 f17
//+ 0x1C0    u32           nShowCounts              ← 配置 f93
//+ 0x1C4    u32                                     ← 配置 f ? （0x11DD18 处，来自[esi + 0x1c]）
//+ 0x1C8    u32                                     ← 配置 f ? （0x11DD21 处，来自[esi + 0x20]）
//+ 0x1CC    u32                                     ← 配置 f ? （0x11DD2A 处，来自[esi + 0x168]）
//+ 0x1D0    u32                                     ← 配置 f ? （0x11DD36 处，来自[esi + 0x16c]）
//+ 0x1E0    u8                                      ← 由配置 f33 经函数 0xEBE80 变换后取低字节
//+ 0x1E4    u32           nSpeicalSkillFlag        ← 配置 f34
//+ 0x1E8    u32   ★★★   nSkillIndication 指示器   ← 配置 f94 '#' 前的数字(技能403 -> 3)
//+ 0x200    std::string   strIndicationParam       ← 配置 f94 '#' 之后的部分
//+ 0x210    u32(上面 str 的 size)
//+ 0x214    u32(上面 str 的 capacity； > 0xF ⇒[对象 + 0x200] 是堆指针)



















//Skill obj
// +C skill id
// +1e8 type                        //8629E9 in 9.18





//9.11
constexpr uintptr_t offset_SlotStruct = 0x24F8418;  // 忽略
//FF 75 C4 FF 75 D0 50 E8 ? ? ? ? ? ? ? ? 8B C8 E8
//v95 = ((__int64 (*)(void))sub_B041E0)();

//9.11
constexpr uintptr_t dword_SummonerSkillStruct = 0x1A684B8; // 忽略
//57 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 0F B7 40 03 50 89 45 EC
// v9 = (char *)sub_6E620();
// v30 = *(_WORD*)(sub_824360(v9, a3) + 3);


//sub 8166A0   8.28
constexpr uintptr_t offset_func_getskillid = 0x81CF30;   // 忽略
//66 8B 86 F8 6E 04 00 66 A3 ?? ?? ?? ?? B8 02 00 00 00 66 A3 ?? ?? ?? ?? B8 NOT WORK
//8D 04 40 8D 04 45 DB 0D 00 00 03 C1
//找方法名

//------鼠标坐标的获取------
constexpr uintptr_t dword_MousePos = 0x1AFFB30; //忽略
//E8 ?? ?? ?? ?? 66 0F 6E 80 64 02 00 00 0F 5B C0 F3 0F 11 45 C0 E8 ?? ?? ?? ?? 66 0F 6E 80 68 02 00 00 8D 45 B8 50 0F 5B C0 
//找到 v55 = (float)*(int *)(sub_66AC80() + 612); 中的 sub_66AC80()


// 9.11
constexpr uintptr_t offset_func_world2Screen = 0x103A450;   // 忽略
//F3 0F 10 86 84 00 00 00 F3 0F 11 44 24 0C F3 0F 10 86 88 00 00 00 F3 0F 11 44 24 10 F3 0F 10 86 8C 00 00 00 C7 44 24 08 00 00 00 00 C7 44 24 04 00 00 00 00 F3 0F 11 44 24 14

// World2Screen相关                                         // 忽略
constexpr uintptr_t dword_castSummnerSkillParam1Up = 0x1A68578; //F3 0F 10 86 88 00 00 00 F3 0F 11 44 24 10 F3 0F 10 86 8C 00 00 00 C7 44 24 08 00 00 00 00 C7 44 24 04 00 00 00 00 下面
constexpr uintptr_t dword_castSummnerSkillParam1 = 0x2C05C;
constexpr uintptr_t dword_castSummnerSkillxRight = 0x1A4A0D4;  // 函数的最后两个参数 xRight，xBottom
constexpr uintptr_t dword_castSummnerSkillxBottom = 0x1A4A0D8;