#pragma once
// =============================================================================
//  SignatureUpdateFlow.h  —— 双击 build_tool.bat 后走的流程
// -----------------------------------------------------------------------------
//  流程（对应你说的「如果版本号不同，就开始扫描」）：
//
//      1. 读游戏当前版本          LauncherCfg\NewUpdateCfg.xml
//      2. 读上次生成的版本        offset_update.h 里的 GameVersion= 那一行
//      3. 两个版本一样？          是 -> 打印「没变」直接退出，一个字节都不动
//                                否 -> 往下走
//      4. 读游戏 exe（文件布局，不需要 dump）
//      5. 按 offsets_AI.h 的表逐条解析，每条都要求【命中且唯一】
//      6. 写 offset_update.h
//
//  退出码：
//      0  成功（含「版本没变，跳过」）
//      1  写出来了，但有条目没解析成功（offset_update.h 顶上会列出来）
//      2  硬错误：exe 读不进来 / 文件写不出去
// =============================================================================

#include "SignatureDefinitionResolve.h"
#include "SignatureOutputWriter.h"

#include <cstdint>
#include <string>
#include <vector>

namespace signature_scan {

// 载入一份镜像（按【文件布局】解析，也就是磁盘上的 300.exe）。
bool LoadGameImage(const char* filePath,
                   std::vector<uint8_t>& imageBuffer,
                   PortableExecutableImage& outImage,
                   std::string& outErrorMessage);

// 把 offsets_AI.h 里那张表整张跑一遍。
void ResolveEntireSignatureTable(const PortableExecutableImage& image,
                                 std::vector<ResolvedSignature>& outResolvedList);

// 打印解析结果表。--matrix 和更新流程都用它。
void PrintResolveTable(const std::vector<ResolvedSignature>& resolvedList);

// 双击 bat 走的入口。forceRescan = true 时无视版本号强制重扫。
int RunOffsetUpdateFlow(bool forceRescan);

} // namespace signature_scan
