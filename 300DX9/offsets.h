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