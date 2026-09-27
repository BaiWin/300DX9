#pragma once
// =============================================================================
//  SignatureConfig.h  —— 路径配置
// -----------------------------------------------------------------------------
//  整个工程只有这个文件里的路径需要按你的机器改。改完重新跑一次
//  build_tool.bat（它会重新编译再运行）。
//
//  ★ SIGNATURE_DEFINITION_FILE 是【编译期】路径，不是运行时路径。
//    原因：offset_update.h 里的常量是从特征码表算出来的，工具必须把表
//    #include 进来（表本身就是 C++ 代码），编译期就定死了。所以它写成一个
//    宏，由 SignatureTool.cpp 用 #include 展开 —— 挪动 offsets_AI.h 时改这里。
//    其余三个是运行时路径，工具直接按字符串打开。
// =============================================================================

// 特征码表。用正斜杠，绝对路径。
#define SIGNATURE_DEFINITION_FILE "E:/repos/300DX9/AI/offsets_AI.h"

namespace signature_scan {

// -----------------------------------------------------------------------------
//  游戏版本文件
// -----------------------------------------------------------------------------
//  游戏 exe 的版本资源是废的（四个 exe 全是 2012 年留下的 1.0.0.1），
//  真正的版本号在启动器配置里：
//      <ClientVersion>2026.09.24.2</ClientVersion>
//      <InnerVersion>1274</InnerVersion>
//  工具读它，和上次生成 offset_update.h 时记下的版本比：
//  一样 -> 什么都不做；不一样 -> 重新扫描并覆盖 offset_update.h。
constexpr const char* kGameVersionFilePath =
    "D:\\Game\\JumpGame\\300Hero\\LauncherCfg\\NewUpdateCfg.xml";

// -----------------------------------------------------------------------------
//  游戏主程序
// -----------------------------------------------------------------------------
//  直接读游戏目录里的 exe，不需要 dump。
//  已实测：磁盘 exe 和 9_23 dump 是同一个 build（PE 时间戳都是 0x6AB26F3D），
//  .text 只差镜像基址重定位（0x400000 vs 0xA00000），没有任何加密。
//  而且磁盘 exe 更干净 —— dump 里带着注入留下的补丁。
constexpr const char* kGameExecutablePath =
    "D:\\Game\\JumpGame\\300Hero\\300.exe";

// -----------------------------------------------------------------------------
//  offset_update.h 的输出目录（结尾不带反斜杠）
// -----------------------------------------------------------------------------
constexpr const char* kOffsetUpdateOutputDirectory = "E:\\repos\\300DX9\\AI";

// 生成出来的文件名
constexpr const char* kOffsetUpdateFileName = "offset_update.h";

} // namespace signature_scan
