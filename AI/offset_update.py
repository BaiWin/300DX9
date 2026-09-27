# -*- coding: utf-8 -*-
"""
offset_update.py —— 双击 update_offset.bat 后跑的就是这个

流程（每次运行都扫，不做「要不要扫」的判断）：

    1. 读游戏当前版本        LauncherCfg\\NewUpdateCfg.xml
    2. 读上次生成的版本      offset_update.h 里的 GameVersion= 那一行
    3. 把两个版本号打印出来  【只打印，不比较】—— 比不比是你自己看的事
    4. 读游戏 exe            磁盘上的 300.exe，不需要 dump
    5. 按 offsets_AI.h 的表逐条解析，每条都要求【命中且唯一】
    6. 写 offset_update.h
    7. 打印结果表 + 成功/失败

★ 为什么不做版本比较：
   比较的用处只有一个 —— 「没变就别浪费时间重扫」。可是重扫只要一两秒，
   而比较带来一个很坏的失败模式：你改了 offsets_AI.h、双击、它说「版本没变，
   跳过」，你以为生效了，其实生成物还是旧的。
   省一两秒换来一个静默错误，不划算。所以每次都扫，版本号只记录、只打印。

退出码：
    0  全部成功
    1  写出来了，但有条目没解析成功（offset_update.h 顶上会列出来）
    2  硬错误：exe 读不进来 / 表读不动 / 文件写不出去
"""

import os
import sys
import time

import signature_config
import signature_scan
from signature_scan import SignatureKind, SignatureResolveStatus

EXIT_CODE_SUCCESS = 0
EXIT_CODE_SOME_SIGNATURE_FAILED = 1
EXIT_CODE_HARD_ERROR = 2

_UTF8_BYTE_ORDER_MARK = b"\xef\xbb\xbf"

_SEPARATOR = "-" * 80
_HEAVY_SEPARATOR = "=" * 80


# =============================================================================
#  版本号
# =============================================================================

class GameVersionInformation:
    """一个版本号。两个字段都记，两个都打印。"""

    __slots__ = ("client_version", "inner_version", "is_available", "error_message")

    def __init__(self):
        self.client_version = ""
        self.inner_version = ""
        self.is_available = False
        self.error_message = ""

    def describe(self):
        if not self.is_available:
            return self.error_message
        inner = self.inner_version if self.inner_version else "-"
        return "%s   (InnerVersion %s)" % (self.client_version, inner)


def _extract_xml_element_text(text, tag_name):
    """取出 <标签>内容</标签> 里的内容。找不到返回 None。"""
    open_tag = "<%s>" % tag_name
    close_tag = "</%s>" % tag_name

    open_position = text.find(open_tag)
    if open_position < 0:
        return None
    value_start = open_position + len(open_tag)
    close_position = text.find(close_tag, value_start)
    if close_position < 0:
        return None

    value = text[value_start:close_position].strip()
    return value if value else None


def _extract_key_value_on_line(text, key_name):
    """取出 "关键字=" 后面到行尾的那一段。找不到返回 None。"""
    key = "%s=" % key_name
    key_position = text.find(key)
    if key_position < 0:
        return None
    value_start = key_position + len(key)
    value_end = text.find("\n", value_start)
    if value_end < 0:
        value_end = len(text)

    value = text[value_start:value_end].strip()
    return value if value else None


def _read_text_file(file_path):
    """读一个文本文件，UTF-8 不行就按 GBK 读。读不到返回 None。"""
    raw_bytes = signature_scan.read_entire_file(file_path)
    if raw_bytes is None:
        return None
    try:
        return raw_bytes.decode("utf-8-sig")
    except UnicodeDecodeError:
        return raw_bytes.decode("gbk", "replace")


def read_game_version(file_path):
    """
    从游戏的 LauncherCfg\\NewUpdateCfg.xml 读当前版本。

    ★ 为什么不去读 exe 的版本资源：那里面是死数据，永远是 1.0.0.1 / 2012 年。
    """
    version = GameVersionInformation()

    text = _read_text_file(file_path)
    if text is None:
        version.error_message = "打不开版本文件：%s" % file_path
        return version

    client_version = _extract_xml_element_text(text, "ClientVersion")
    inner_version = _extract_xml_element_text(text, "InnerVersion")

    if client_version is None and inner_version is None:
        version.error_message = "文件里既没有 ClientVersion 也没有 InnerVersion"
        return version

    version.client_version = client_version or ""
    version.inner_version = inner_version or ""
    version.is_available = True
    return version


def read_generated_version(file_path):
    """从上次生成的 offset_update.h 里读版本。"""
    version = GameVersionInformation()

    text = _read_text_file(file_path)
    if text is None:
        version.error_message = "还没有生成过 offset_update.h"
        return version

    client_version = _extract_key_value_on_line(text, "GameVersion")
    inner_version = _extract_key_value_on_line(text, "InnerVersion")

    if client_version is None and inner_version is None:
        version.error_message = "offset_update.h 里没有版本标记行"
        return version

    version.client_version = client_version or ""
    version.inner_version = inner_version or ""
    version.is_available = True
    return version


# =============================================================================
#  写 offset_update.h
# =============================================================================

def format_current_local_time():
    return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())


def get_constant_trailing_comment(signature_kind):
    """常量后面跟的那句说明。看一眼生成的文件就知道每个地址是什么性质。"""
    if signature_kind == SignatureKind.FUNCTION:
        return "函数入口"
    if signature_kind == SignatureKind.TRAMPOLINE:
        return "Hook 点"
    return "全局变量"


def build_offset_update_text(game_version, source_image_path, image_base_address,
                             resolved_list):
    """拼出 offset_update.h 的全部内容。"""
    lines = []

    lines.append("// " + "=" * 77)
    lines.append("//  offset_update.h  —— 由 offset_update.py 自动生成，请勿手工修改")
    lines.append("// " + "-" * 77)
    lines.append("//  重新生成：双击 update_offset.bat")
    lines.append("//  特征码表：offsets_AI.h（要加英雄 / 加 hook，改那张表，不改这个文件）")
    lines.append("// " + "-" * 77)
    lines.append("//  GameVersion=%s" % (game_version.client_version or "UNKNOWN"))
    lines.append("//  InnerVersion=%s" % (game_version.inner_version or "UNKNOWN"))
    lines.append("//  SourceImage=%s" % source_image_path)
    lines.append("//  ImageBase=0x%08X" % image_base_address)
    lines.append("//  GeneratedAt=%s" % format_current_local_time())
    lines.append("//")
    lines.append("//  ★ 下面全部是【相对地址】。用的时候一律：")
    lines.append("//        实际地址 = 模块基址 + 这里的值")
    lines.append("//    模块基址必须现取：GetModuleHandleW(nullptr)")
    lines.append("//    游戏实际加载的基址【不是】PE 头里写的那个（dump 里是 0xA00000），")
    lines.append("//    写死 0x400000 一定错。")
    lines.append("//")
    lines.append("//  生成方式：扫 300.exe 的 .text，每条特征码都要求命中且【唯一】。")
    lines.append("// " + "=" * 77)
    lines.append("")

    # 失败的条目在顶上大声列出来。
    # ★ 它们【不会】生成常量 —— 少一个常量是编译错误，很吵；
    #   写一个错的值是静默错误，极难查。要让失败变吵，不要让它变哑。
    failed_list = [item for item in resolved_list if not item.is_ok()]
    if failed_list:
        lines.append("// " + "#" * 75)
        lines.append("// ##  下面这些特征码这次没解析出来，对应的常量【没有生成】      ##")
        lines.append("// ##  去 offsets_AI.h 修那条特征码，然后重新双击 update_offset.bat ##")
        lines.append("// " + "#" * 75)
        for item in failed_list:
            lines.append("//  [%s] %s：%s"
                         % (item.status, item.definition.signature_name, item.error_message))
        lines.append("//")
        lines.append("")

    lines.append("#include <cstdint>")
    lines.append("")

    for item in resolved_list:
        if not item.is_ok():
            continue

        name = item.definition.signature_name
        comment = get_constant_trailing_comment(item.definition.signature_kind)

        if item.is_hook_span:
            # Trampoline 出两个常量，成对
            lines.append("constexpr uintptr_t %s_Start = 0x%08X;  // %s 起点（%d 字节）"
                         % (name, item.relative_address, comment,
                            item.definition.boundary_length))
            lines.append("constexpr uintptr_t %s_End = 0x%08X;    // %s 终点（起点 + %d）"
                         % (name, item.boundary_end_relative_address, comment,
                            item.definition.boundary_length))
        else:
            lines.append("constexpr uintptr_t %s = 0x%08X;  // %s"
                         % (name, item.relative_address, comment))

    lines.append("")
    return "\n".join(lines)


def write_offset_update_header(output_directory, output_file_name, game_version,
                               source_image_path, image_base_address, resolved_list):
    """
    写文件。返回 (是否成功, 完整路径, 错误信息, 写出的常量个数)。

    UTF-8 带 BOM：DLL 那边编译时不一定带 /utf-8，带 BOM 编译器才能认出这是 UTF-8，
    否则里面的中文注释会按 GBK 解释成乱码。
    """
    file_path = os.path.join(output_directory, output_file_name)

    text = build_offset_update_text(game_version, source_image_path,
                                   image_base_address, resolved_list)

    try:
        with open(file_path, "wb") as file_handle:
            file_handle.write(_UTF8_BYTE_ORDER_MARK)
            file_handle.write(text.encode("utf-8"))
    except OSError as error:
        return False, file_path, "写不开文件：%s（%s）" % (file_path, error), 0

    written_constant_count = 0
    for item in resolved_list:
        if not item.is_ok():
            continue
        written_constant_count += 2 if item.is_hook_span else 1

    return True, file_path, "", written_constant_count


# =============================================================================
#  打印
# =============================================================================

def print_resolve_table(resolved_list):
    """逐条打印：函数名 / 类型 / 状态 / 结果。"""
    print("%-40s %-11s %-16s %s" % ("函数名", "类型", "状态", "结果"))
    print(_SEPARATOR)

    success_count = 0
    failed_count = 0

    for item in resolved_list:
        name = item.definition.signature_name

        if item.is_ok():
            success_count += 1
            if item.is_hook_span:
                result_text = "0x%08X ~ 0x%08X  (%d 字节)" % (
                    item.relative_address, item.boundary_end_relative_address,
                    item.definition.boundary_length)
            else:
                result_text = "0x%08X" % item.relative_address
        else:
            failed_count += 1
            result_text = item.error_message

        print("%-40s %-11s %-16s %s"
              % (name, item.definition.signature_kind, item.status, result_text))

        # 二级扫描的诊断：命中在哪儿、函数体划到哪儿、外面还有几处同名。
        # 「取第一个」这个约定唯一的风险点就是它 —— 打出来才能一眼看出
        # 第二个命中是不是离得太近（离得近说明二级码太短，换个版本可能前移）。
        if item.secondary_hit_relative_addresses:
            hit_text = "  ".join("0x%08X" % address
                                 for address in item.secondary_hit_relative_addresses)
            line = ("%42s二级: 函数体 %08X..%08X  命中 %d 处  %s"
                    % ("", item.secondary_function_entry_relative_address,
                       item.secondary_function_end_relative_address,
                       len(item.secondary_hit_relative_addresses), hit_text))
            if item.secondary_hit_count_beyond_function_end:
                line += ("   | 函数体外还有 %d 处同名，已按 ret 排除"
                         % item.secondary_hit_count_beyond_function_end)
            print(line)

    print(_SEPARATOR)
    print(" 共 %d 条：成功 %d，失败 %d" % (len(resolved_list), success_count, failed_count))

    return success_count, failed_count


# =============================================================================
#  主流程
# =============================================================================

def run():
    print(_HEAVY_SEPARATOR)
    print(" offset_update.h 生成")
    print(_HEAVY_SEPARATOR)

    # ---- 1. 游戏当前版本 ----
    game_version = read_game_version(signature_config.GAME_VERSION_FILE_PATH)
    print(" 游戏版本     : %s" % game_version.describe())

    # ---- 2. 上次生成的版本 ----
    output_file_path = os.path.join(signature_config.OUTPUT_DIRECTORY,
                                   signature_config.OUTPUT_FILE_NAME)
    generated_version = read_generated_version(output_file_path)
    print(" 上次生成版本 : %s" % generated_version.describe())

    print(_SEPARATOR)

    # ---- 3. 读特征码表 ----
    try:
        definitions = signature_scan.load_signature_table(
            signature_config.SIGNATURE_DEFINITION_FILE_PATH)
    except signature_scan.SignatureTableError as error:
        print(" [失败] 特征码表读不动：%s" % error)
        print(_HEAVY_SEPARATOR)
        return EXIT_CODE_HARD_ERROR

    print(" 特征码表     : %s  （%d 条）"
          % (signature_config.SIGNATURE_DEFINITION_FILE_PATH, len(definitions)))

    # ---- 4. 读游戏 exe ----
    print(" 正在读取     : %s" % signature_config.GAME_EXECUTABLE_PATH)
    image_bytes = signature_scan.read_entire_file(signature_config.GAME_EXECUTABLE_PATH)
    if image_bytes is None:
        print(" [失败] 读不进这个文件")
        print(_HEAVY_SEPARATOR)
        return EXIT_CODE_HARD_ERROR

    image = signature_scan.PortableExecutableImage()
    if not image.parse_as_file_layout(image_bytes):
        print(" [失败] 这不是一个能解析的 PE 文件（需要 32 位的 300.exe）")
        print(_HEAVY_SEPARATOR)
        return EXIT_CODE_HARD_ERROR

    if image.get_executable_code_section() is None:
        print(" [失败] 这个镜像里没有可执行的代码段")
        print(_HEAVY_SEPARATOR)
        return EXIT_CODE_HARD_ERROR

    print(" 镜像大小     : %d 字节，ImageBase 0x%08X"
          % (len(image_bytes), image.image_base_address))
    print(_SEPARATOR)

    # ---- 5. 整表解析 ----
    resolved_list = signature_scan.resolve_entire_signature_table(image, definitions)
    success_count, failed_count = print_resolve_table(resolved_list)

    # ---- 6. 写文件 ----
    is_written, file_path, write_error, written_constant_count = write_offset_update_header(
        signature_config.OUTPUT_DIRECTORY, signature_config.OUTPUT_FILE_NAME,
        game_version, signature_config.GAME_EXECUTABLE_PATH,
        image.image_base_address, resolved_list)

    if not is_written:
        print(" [失败] %s" % write_error)
        print(_HEAVY_SEPARATOR)
        return EXIT_CODE_HARD_ERROR

    print(" 已写出       : %s" % file_path)
    print(" 常量个数     : %d 个（Trampoline 各算两个）" % written_constant_count)
    print(_HEAVY_SEPARATOR)

    if failed_count == 0:
        print(" [成功] %d 条特征码全部解析成功" % success_count)
        print(_HEAVY_SEPARATOR)
        return EXIT_CODE_SUCCESS

    print(" [失败] %d 条没解析出来，对应的常量【没有生成】：" % failed_count)
    for item in resolved_list:
        if not item.is_ok():
            print("          [%s] %s" % (item.status, item.definition.signature_name))
    print("        去 offsets_AI.h 修那几条特征码，然后重新双击 update_offset.bat")
    print(_HEAVY_SEPARATOR)
    return EXIT_CODE_SOME_SIGNATURE_FAILED


if __name__ == "__main__":
    sys.exit(run())
