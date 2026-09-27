// 读文件用 std::fopen 就够了，不想为了 MSVC 的安全检查去引 <filesystem>。
// 引擎（SignatureScan.cpp）也是这么处理的，保持一致。
#define _CRT_SECURE_NO_WARNINGS

#include "SignatureGameVersion.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace signature_scan {

namespace {

// 把整个文件读成一个 std::string。版本文件只有几百字节，不做流式处理。
bool ReadWholeTextFile(const char* filePath, std::string& outText)
{
    outText.clear();
    if (filePath == nullptr)
        return false;

    std::FILE* fileHandle = std::fopen(filePath, "rb");
    if (fileHandle == nullptr)
        return false;

    char chunkBuffer[4096];
    size_t readByteCount = 0;
    while ((readByteCount = std::fread(chunkBuffer, 1, sizeof(chunkBuffer), fileHandle)) > 0)
        outText.append(chunkBuffer, readByteCount);

    std::fclose(fileHandle);
    return true;
}

// 取出 <tagName>...</tagName> 中间那一段。找不到返回 false。
bool ExtractXmlElementText(const std::string& text,
                           const char* tagName,
                           std::string& outValue)
{
    outValue.clear();

    const std::string openTag  = std::string("<") + tagName + ">";
    const std::string closeTag = std::string("</") + tagName + ">";

    const size_t openPosition = text.find(openTag);
    if (openPosition == std::string::npos)
        return false;

    const size_t valueStart = openPosition + openTag.size();
    const size_t closePosition = text.find(closeTag, valueStart);
    if (closePosition == std::string::npos)
        return false;

    outValue = text.substr(valueStart, closePosition - valueStart);
    return !outValue.empty();
}

// 去首尾空白。版本号里混进空格会让「比版本」永远不相等。
std::string TrimWhitespace(const std::string& text)
{
    size_t startIndex = 0;
    while (startIndex < text.size() &&
           (text[startIndex] == ' ' || text[startIndex] == '\t' ||
            text[startIndex] == '\r' || text[startIndex] == '\n'))
    {
        ++startIndex;
    }

    size_t endIndex = text.size();
    while (endIndex > startIndex &&
           (text[endIndex - 1] == ' ' || text[endIndex - 1] == '\t' ||
            text[endIndex - 1] == '\r' || text[endIndex - 1] == '\n'))
    {
        --endIndex;
    }

    return text.substr(startIndex, endIndex - startIndex);
}

// 取出 "关键字=" 后面到行尾的那一段。
bool ExtractKeyValueOnLine(const std::string& text,
                           const char* keyName,
                           std::string& outValue)
{
    outValue.clear();

    const std::string key = std::string(keyName) + "=";
    const size_t keyPosition = text.find(key);
    if (keyPosition == std::string::npos)
        return false;

    const size_t valueStart = keyPosition + key.size();
    size_t valueEnd = text.find('\n', valueStart);
    if (valueEnd == std::string::npos)
        valueEnd = text.size();

    outValue = TrimWhitespace(text.substr(valueStart, valueEnd - valueStart));
    return !outValue.empty();
}

} // namespace

bool ReadGameVersionFromLauncherConfig(const char* filePath, GameVersionInformation& outVersion)
{
    outVersion = GameVersionInformation{};

    std::string text;
    if (!ReadWholeTextFile(filePath, text))
    {
        outVersion.errorMessage = std::string("打不开版本文件：") + (filePath ? filePath : "(空路径)");
        return false;
    }

    std::string clientVersion;
    std::string innerVersion;
    const bool hasClientVersion = ExtractXmlElementText(text, "ClientVersion", clientVersion);
    const bool hasInnerVersion  = ExtractXmlElementText(text, "InnerVersion", innerVersion);

    if (!hasClientVersion && !hasInnerVersion)
    {
        outVersion.errorMessage = "文件里既没有 ClientVersion 也没有 InnerVersion";
        return false;
    }

    outVersion.clientVersion = clientVersion;
    outVersion.innerVersion  = innerVersion;
    outVersion.isAvailable   = true;
    return true;
}

bool ReadGameVersionFromGeneratedHeader(const char* filePath, GameVersionInformation& outVersion)
{
    outVersion = GameVersionInformation{};

    std::string text;
    if (!ReadWholeTextFile(filePath, text))
    {
        // 上次还没生成过 —— 不算错误，调用方会当成「版本不同」重新扫。
        outVersion.errorMessage = "还没有生成过 offset_update.h";
        return false;
    }

    std::string clientVersion;
    std::string innerVersion;
    const bool hasClientVersion = ExtractKeyValueOnLine(text, "GameVersion", clientVersion);
    const bool hasInnerVersion  = ExtractKeyValueOnLine(text, "InnerVersion", innerVersion);

    if (!hasClientVersion && !hasInnerVersion)
    {
        outVersion.errorMessage = "offset_update.h 里没有版本标记行";
        return false;
    }

    outVersion.clientVersion = clientVersion;
    outVersion.innerVersion  = innerVersion;
    outVersion.isAvailable   = true;
    return true;
}

bool IsSameGameVersion(const GameVersionInformation& left, const GameVersionInformation& right)
{
    if (!left.isAvailable || !right.isAvailable)
        return false;
    return left.clientVersion == right.clientVersion &&
           left.innerVersion == right.innerVersion;
}

} // namespace signature_scan
