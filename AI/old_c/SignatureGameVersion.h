#pragma once
// =============================================================================
//  SignatureGameVersion.h  —— 游戏版本号的读 / 比
// -----------------------------------------------------------------------------
//  为什么不用 exe 里的版本资源：
//      四个 exe 的 VS_VERSION_INFO 全是 2012 年留下的死数据 ——
//      FileVersion 1.0.0.1 / OriginalFilename XGame.exe / 上海跳跃网络科技。
//      拿它做「版本变了没有」的判断，永远是「没变」。
//
//  真正的版本号在启动器配置里：
//      LauncherCfg\NewUpdateCfg.xml
//          <ClientVersion>2026.09.24.2</ClientVersion>   给人看的
//          <InnerVersion>1274</InnerVersion>             给机器比的
//
//  注意别拿 Config\NewUpdateCfg.xml —— 那份是 0.2.303 + 192.168.1.222，
//  开发期忘在安装目录里的残留，不是线上版本。
//
//  判断「要不要重新扫描」用的是【两个版本号都相同】：
//      ClientVersion 相同 且 InnerVersion 相同 -> 什么都不做
//      任一不同                                -> 重新扫描
//  这样只要官方动了版本号就一定重扫，不会因为一个号没更新而漏掉。
// =============================================================================

#include <string>

namespace signature_scan {

struct GameVersionInformation
{
    std::string clientVersion;   // 例如 2026.09.24.2
    std::string innerVersion;    // 例如 1274

    bool        isAvailable = false;
    std::string errorMessage;

    bool IsEmpty() const { return clientVersion.empty() && innerVersion.empty(); }
};

// 从启动器配置 xml 里读。用字符串查找解析，不引 xml 库 —— 就两个标签，
// 而且这个文件格式几十年没变过。
bool ReadGameVersionFromLauncherConfig(const char* filePath, GameVersionInformation& outVersion);

// 从上次生成的 offset_update.h 里读回版本号（认 "GameVersion=" 这一行）。
// 文件不存在或者没这一行，都算「没读到」—— 那就当作版本变了，重新扫一次。
bool ReadGameVersionFromGeneratedHeader(const char* filePath, GameVersionInformation& outVersion);

// 两个版本号是否完全一致。
bool IsSameGameVersion(const GameVersionInformation& left, const GameVersionInformation& right);

} // namespace signature_scan
