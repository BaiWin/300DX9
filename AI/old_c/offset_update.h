#pragma once
// =============================================================================
//  offset_update.h  —— 由 SignatureTool 自动生成，请勿手工修改
// -----------------------------------------------------------------------------
//  重新生成：双击 build_tool.bat（它会先编译再运行）
//  特征码表：offsets_AI.h（要加英雄 / 加 hook，改那张表，不改这个文件）
// -----------------------------------------------------------------------------
//  GameVersion=2026.09.24.2
//  InnerVersion=1274
//  SourceImage=D:\Game\JumpGame\300Hero\300.exe
//  ImageBase=0x00400000
//  GeneratedAt=2026-09-27 11:01:16
//
//  ★ 下面全部是【相对地址】。用的时候一律：
//        实际地址 = 模块基址 + 这里的值
//    模块基址必须现取：GetModuleHandleW(nullptr)
//    游戏实际加载的基址【不是】PE 头里写的那个（dump 里是 0xA00000），
//    写死 0x400000 一定错。
//
//  生成方式：扫 300.exe 的 .text，每条特征码都要求命中且【唯一】。
// =============================================================================

#include <cstdint>

constexpr uintptr_t offset_func_castskill = 0x00AFEE60;  // 函数入口
constexpr uintptr_t offset_func_castSummonerSkill = 0x00774460;  // 函数入口
constexpr uintptr_t offset_func_treeFindPlayerObj = 0x000601F0;  // 函数入口
constexpr uintptr_t offset_func_getCDRecordObj = 0x0081D260;  // 函数入口
constexpr uintptr_t Trampoline_CastNormalSkillRecvCD_Start = 0x00AFDBD1;  // Hook 点 起点（6 字节）
constexpr uintptr_t Trampoline_CastNormalSkillRecvCD_End = 0x00AFDBD7;    // Hook 点 终点（起点 + 6）
constexpr uintptr_t Trampoline_SwitchSkill_Start = 0x00824B59;  // Hook 点 起点（6 字节）
constexpr uintptr_t Trampoline_SwitchSkill_End = 0x00824B5F;    // Hook 点 终点（起点 + 6）
constexpr uintptr_t Trampoline_HookSendPack_Start = 0x001DFAE4;  // Hook 点 起点（10 字节）
constexpr uintptr_t Trampoline_HookSendPack_End = 0x001DFAEE;    // Hook 点 终点（起点 + 10）
constexpr uintptr_t dword_SkillTable = 0x01A6A4B8;  // 全局变量
constexpr uintptr_t dword_SkillTable_Slot_SkillIDOffset = 0x00046F38;  // 全局变量
constexpr uintptr_t dword_CurrentProcessed_SkillID = 0x01E2D670;  // 全局变量
constexpr uintptr_t dword_Clock = 0x01A70C40;  // 全局变量
constexpr uintptr_t dword_HoverStruct = 0x01A6A76C;  // 全局变量
