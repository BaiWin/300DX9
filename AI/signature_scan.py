# -*- coding: utf-8 -*-
"""
signature_scan.py —— 特征码扫描引擎

纯 Python，不需要编译，不依赖任何第三方库。

它只认「一块内存 + 长度」，不知道什么叫游戏、什么叫 DLL ——
所以同一套代码既能扫磁盘上的 300.exe，也能扫一个 dump，
两者都是文件布局，字节是同一种。

本文件从下往上分四层：

    一、特征码解析   "E8 ?? ?? 80 38"  ->  期望字节 + 比较掩码 + 正则
    二、扫描         在一段内存里找所有匹配
    三、PE 解析      读 .text 段（按文件布局）
    四、表 + 解析    按 offsets_AI.h 里的一条定义算出真地址

第四层是「语义」和「字节」之间的翻译。三种类型走法完全不同：

    Function    有效地址处是 E8/E9  ->  目标 = 有效地址 + 5 + rel32
    Trampoline  有效地址就是 Hook 点  ->  给出 (起点, 起点 + 边界)
    Dword       有效地址处是引用指令  ->  读它里面的 4 字节
                有二级就先按主码反解出函数入口，再从入口向下扫二级码

★ 两个「不让人填」的设计（填就是留一个填错的机会）：

    减不减镜像基址，由【操作码】决定，表里不填：
        绝对寻址      A1 / 66 A3 / 8B 0D ...   读出来是绝对地址   -> 减
        [寄存器+偏移] 8B 86 ...                 读出来是结构体偏移 -> 不减

    边界填错了会被抓出来：
        工具按操作码自己推一遍指令长度，和表里填的边界对账，
        对不上就报 BAD-BOUNDARY —— 免费抓「偏移数错一位」。
"""

import os
import re
import struct

# =============================================================================
#  常量
# =============================================================================

# 二级扫描的范围上限。找不到 ret 时退回到它。
SECONDARY_SCAN_MAXIMUM_BYTE_COUNT = 0x10000

# 二级命中最多报告几处（只是报告，取的是第一个）
SECONDARY_HIT_REPORT_COUNT = 4

# 一条引用指令至少要能读到 8 字节才敢认。
# 意义：.text 末尾那几条指令如果只剩不到 8 字节，宁可判定「认不出来」，
# 也不要在残缺的字节上推操作码 —— 推出来的是垃圾，而且不会报错。
MINIMUM_INSTRUCTION_BYTE_COUNT = 8

_HEX_DIGIT_CHARACTERS = "0123456789abcdefABCDEF"


# =============================================================================
#  一、特征码解析
# =============================================================================

class SignaturePattern:
    """一条解析好的特征码。"""

    __slots__ = ("pattern_text", "expected_bytes", "comparison_mask",
                 "exact_byte_count", "first_exact_byte_index",
                 "compiled_regex", "is_valid", "error_message")

    def __init__(self):
        self.pattern_text = ""
        self.expected_bytes = b""
        self.comparison_mask = b""
        self.exact_byte_count = 0
        self.first_exact_byte_index = -1
        self.compiled_regex = None
        self.is_valid = False
        self.error_message = ""


def parse_signature_pattern(pattern_text):
    """
    把 "E8 ?? ?? ?? ?? 80 38 00 74 16" 解析成 期望字节 + 比较掩码。

    通配写法：? ?? **      分隔符：空格、制表、逗号、换行
    掩码 1 = 要比较，0 = 通配。

    连续的 ?? 算【一个】通配字节（和 C++ 版行为一致）。
    """
    pattern = SignaturePattern()
    pattern.pattern_text = pattern_text or ""

    if not pattern.pattern_text:
        pattern.error_message = "特征码为空"
        return pattern

    expected_bytes = bytearray()
    comparison_mask = bytearray()

    text = pattern.pattern_text
    text_length = len(text)
    index = 0

    while index < text_length:
        character = text[index]

        # 分隔符
        if character.isspace() or character == ",":
            index += 1
            continue

        # 通配
        if character == "?" or character == "*":
            while index < text_length and text[index] == character:
                index += 1
            expected_bytes.append(0)
            comparison_mask.append(0)
            continue

        # 十六进制字节
        if character not in _HEX_DIGIT_CHARACTERS:
            pattern.error_message = "非法字符 '%s'" % character
            return pattern
        high_nibble = int(character, 16)
        index += 1

        if index >= text_length or text[index] not in _HEX_DIGIT_CHARACTERS:
            pattern.error_message = "十六进制字节不完整（少了一个半字节）"
            return pattern
        low_nibble = int(text[index], 16)
        index += 1

        expected_bytes.append((high_nibble << 4) | low_nibble)
        comparison_mask.append(1)

    if not expected_bytes:
        pattern.error_message = "特征码解析后为空"
        return pattern

    pattern.expected_bytes = bytes(expected_bytes)
    pattern.comparison_mask = bytes(comparison_mask)

    for byte_index in range(len(comparison_mask)):
        if comparison_mask[byte_index]:
            pattern.exact_byte_count += 1
            if pattern.first_exact_byte_index < 0:
                pattern.first_exact_byte_index = byte_index

    if pattern.first_exact_byte_index < 0:
        pattern.error_message = "特征码全是通配，无法定位"
        return pattern

    # 编译成正则表达式。
    # 为什么用正则而不是 Python 循环逐字节比：
    # 正则引擎是 C 实现的，扫 21MB 的 .text 差别是「零点几秒」和「几十秒」。
    regex_pieces = []
    for expected_byte, is_exact in zip(pattern.expected_bytes, pattern.comparison_mask):
        if is_exact:
            regex_pieces.append(re.escape(bytes([expected_byte])))
        else:
            regex_pieces.append(b".")
    pattern.compiled_regex = re.compile(b"".join(regex_pieces), re.DOTALL)

    pattern.is_valid = True
    return pattern


# =============================================================================
#  二、扫描
# =============================================================================

class SignatureHit:
    """一处命中。"""

    __slots__ = ("relative_address", "buffer_offset")

    def __init__(self):
        self.relative_address = 0
        self.buffer_offset = 0


def scan_memory_buffer(memory_data, buffer_start_address, pattern,
                       maximum_hit_count=64):
    """
    在 memory_data 里找 pattern 的【所有】匹配。

    返回 (命中总数, 命中列表)。
    命中总数可能大于列表长度 —— 收够 maximum_hit_count 条就停下来，
    因为调用方要的往往只是「唯一不唯一」，不需要数完。
    """
    hits = []
    if not pattern.is_valid:
        return 0, hits

    total_hit_count = 0
    for match in pattern.compiled_regex.finditer(memory_data):
        total_hit_count += 1
        if len(hits) < maximum_hit_count:
            hit = SignatureHit()
            hit.buffer_offset = match.start()
            hit.relative_address = buffer_start_address + match.start()
            hits.append(hit)
        if len(hits) >= maximum_hit_count:
            break

    return total_hit_count, hits


# =============================================================================
#  三、PE 解析（文件布局）
# =============================================================================

class PortableExecutableSection:
    """一个段。"""

    __slots__ = ("section_name", "relative_address", "virtual_size",
                 "raw_data_file_offset", "raw_data_size", "is_executable")

    def __init__(self):
        self.section_name = ""
        self.relative_address = 0       # 段在内存里的相对地址（RVA）
        self.virtual_size = 0
        self.raw_data_file_offset = 0   # 段数据在【文件里】的偏移（注意：不是 RVA！）
        self.raw_data_size = 0
        self.is_executable = False


class PortableExecutableImage:
    """
    一个 PE 镜像。

    本工程只走【文件布局】—— 磁盘上的 300.exe 和 dump 都是这一种。
    文件布局的意思是：段数据在文件里的 raw_data_file_offset 处，
    所以「相对地址 -> 字节」必须经过它换算。
    活进程的内存布局（段直接在 模块基址 + 相对地址 处）本工程用不到。
    """

    def __init__(self):
        self.image_data = b""
        self.image_base_address = 0
        self.sections = []
        self.is_flat_file_layout = False

    def parse_as_file_layout(self, image_bytes):
        """解析 PE 头。不复制数据，只记段表。"""
        self.image_data = image_bytes
        self.image_base_address = 0
        self.sections = []
        self.is_flat_file_layout = False

        image_size = len(image_bytes)
        if image_size < 0x40:
            return False

        # ---- 不是 PE？当作裸内存镜像处理 ----
        # 某些 dumper 产出的是「按内存布局平铺」的文件，没有可用的 PE 头。
        # 这种情况下文件偏移 == 相对地址，仍然可以扫描，只是没有段信息。
        if image_bytes[0:2] != b"MZ":
            self.is_flat_file_layout = True

            flat_section = PortableExecutableSection()
            flat_section.section_name = "{flat-image}"
            flat_section.relative_address = 0
            flat_section.virtual_size = image_size
            flat_section.raw_data_file_offset = 0
            flat_section.raw_data_size = image_size
            flat_section.is_executable = True
            self.sections.append(flat_section)
            return True

        pe_header_file_offset = struct.unpack_from("<I", image_bytes, 0x3C)[0]
        if pe_header_file_offset + 24 > image_size:
            return False
        if image_bytes[pe_header_file_offset:pe_header_file_offset + 4] != b"PE\0\0":
            return False

        section_count = struct.unpack_from("<H", image_bytes, pe_header_file_offset + 6)[0]
        optional_header_size = struct.unpack_from("<H", image_bytes, pe_header_file_offset + 20)[0]
        optional_header_offset = pe_header_file_offset + 24

        if optional_header_offset + optional_header_size > image_size:
            return False
        if optional_header_size < 96:
            return False

        # 只要 PE32（本工程扫的是 32 位游戏）。
        # PE32+ 是 0x20B，它的 ImageBase 在别处、字段宽度也不同，混用会读出垃圾。
        magic_number = struct.unpack_from("<H", image_bytes, optional_header_offset)[0]
        if magic_number != 0x10B:
            return False

        self.image_base_address = struct.unpack_from("<I", image_bytes, optional_header_offset + 28)[0]

        section_table_offset = optional_header_offset + optional_header_size
        for section_index in range(section_count):
            header_offset = section_table_offset + section_index * 40
            if header_offset + 40 > image_size:
                break

            section = PortableExecutableSection()
            # 段名是 8 字节定长、后面补 \0（".text\0\0\0"），要去掉补位
            section.section_name = (image_bytes[header_offset:header_offset + 8]
                                    .rstrip(b"\0").decode("ascii", "replace"))
            section.virtual_size = struct.unpack_from("<I", image_bytes, header_offset + 8)[0]
            section.relative_address = struct.unpack_from("<I", image_bytes, header_offset + 12)[0]
            section.raw_data_size = struct.unpack_from("<I", image_bytes, header_offset + 16)[0]
            section.raw_data_file_offset = struct.unpack_from("<I", image_bytes, header_offset + 20)[0]

            characteristics = struct.unpack_from("<I", image_bytes, header_offset + 36)[0]
            section.is_executable = (characteristics & 0x20000000) != 0

            self.sections.append(section)

        return True

    def get_executable_code_section(self):
        """
        取可执行代码段（通常叫 .text）。

        返回 (段字节, 段相对地址)；找不到返回 None。
        找不到 .text 时退化为第一个可执行段。
        """
        code_section = None
        for section in self.sections:
            if section.section_name == ".text":
                code_section = section
                break
        if code_section is None:
            for section in self.sections:
                if section.is_executable:
                    code_section = section
                    break
        if code_section is None:
            return None

        # 只扫文件里真正有字节的那部分：
        # rawDataSize 是磁盘上占多少，virtualSize 是内存里占多少，
        # 内存里那截尾巴（未初始化部分）在文件里根本不存在，不能扫。
        usable_size = code_section.raw_data_size
        if code_section.virtual_size and usable_size > code_section.virtual_size:
            usable_size = code_section.virtual_size
        if code_section.raw_data_file_offset + usable_size > len(self.image_data):
            usable_size = len(self.image_data) - code_section.raw_data_file_offset
        if usable_size <= 0:
            return None

        start_offset = code_section.raw_data_file_offset
        return (self.image_data[start_offset:start_offset + usable_size],
                code_section.relative_address)


def read_entire_file(file_path):
    """把整个文件读成 bytes。读不到返回 None。"""
    try:
        with open(file_path, "rb") as file_handle:
            return file_handle.read()
    except OSError:
        return None


# =============================================================================
#  四之一、表：offsets_AI.h 里的 kSignatureTable[]
# =============================================================================

class SignatureKind:
    """三种类型。"""

    FUNCTION = "Function"      # 取 call 的目标
    TRAMPOLINE = "Trampoline"  # 取一段指令的首尾
    DWORD = "Dword"            # 取指令里的 4 字节

    ALL = (FUNCTION, TRAMPOLINE, DWORD)


class SignatureDefinition:
    """表里的一条。字段顺序和 offsets_AI.h 里写的一致。"""

    __slots__ = ("signature_name", "pattern_text", "signature_kind",
                 "pattern_offset", "boundary_length", "value_adjustment",
                 "secondary_pattern_text", "secondary_pattern_offset")

    def __init__(self):
        self.signature_name = ""
        self.pattern_text = ""
        self.signature_kind = SignatureKind.DWORD
        self.pattern_offset = 0
        self.boundary_length = 0
        self.value_adjustment = 0
        self.secondary_pattern_text = None   # None = 不进 call，直接取
        self.secondary_pattern_offset = 0


class SignatureTableError(Exception):
    """表本身读不动 / 格式不对。"""


def _decode_source_text(raw_bytes):
    """
    把 offsets_AI.h 解成文本。

    先按 UTF-8 解，失败再按 GBK 解。
    为什么要这样：这个文件以前是 GBK，被编辑器另存成 GBK 是很容易发生的事，
    而 UTF-8/GBK 混用会变成一堆看不懂的报错。这里直接两种都认。
    """
    try:
        return raw_bytes.decode("utf-8-sig")
    except UnicodeDecodeError:
        pass
    try:
        return raw_bytes.decode("gbk")
    except UnicodeDecodeError:
        raise SignatureTableError("offsets_AI.h 既不是 UTF-8 也不是 GBK，无法读取")


def _strip_comments(text):
    """
    去掉 // 和 /* */ 注释。

    ★ 必须认得字符串字面量：特征码是引号包起来的，
       如果不管引号，注释里的引号会把后面整段代码都当成字符串，解析全乱。
    """
    result = []
    index = 0
    text_length = len(text)
    in_string = False

    while index < text_length:
        character = text[index]

        if in_string:
            result.append(character)
            if character == "\\" and index + 1 < text_length:
                result.append(text[index + 1])
                index += 2
                continue
            if character == '"':
                in_string = False
            index += 1
            continue

        if character == '"':
            in_string = True
            result.append(character)
            index += 1
            continue

        # 行注释：吃到行尾（注释里的引号不算字符串，因为整段被跳过了）
        if character == "/" and index + 1 < text_length and text[index + 1] == "/":
            while index < text_length and text[index] != "\n":
                index += 1
            continue

        # 块注释
        if character == "/" and index + 1 < text_length and text[index + 1] == "*":
            index += 2
            while index + 1 < text_length and not (text[index] == "*" and text[index + 1] == "/"):
                index += 1
            index += 2
            continue

        result.append(character)
        index += 1

    return "".join(result)


def _extract_braced_body(text, start_search_index):
    """从 start_search_index 往后找第一个 { ... }，返回里面的内容。"""
    open_index = text.find("{", start_search_index)
    if open_index < 0:
        raise SignatureTableError("找不到 '{'")

    depth = 0
    for index in range(open_index, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[open_index + 1:index]

    raise SignatureTableError("'{' 没有对应的 '}'，文件是不是被截断了")


def _split_top_level(text, separator=","):
    """
    按 separator 切开，但只切【最外层】的 —— 大括号里的逗号不算。

    这是整个解析的关键：表里每条之间的逗号在最外层，
    而二级特征码 { "A1 ...", 0 } 里面的逗号在第二层，不能被当成条目分隔。
    """
    pieces = []
    depth = 0
    current_start = 0
    in_string = False
    is_escaped = False

    for index in range(len(text)):
        character = text[index]

        if in_string:
            if is_escaped:
                is_escaped = False
            elif character == "\\":
                is_escaped = True
            elif character == '"':
                in_string = False
            continue

        if character == '"':
            in_string = True
        elif character in "{(":
            depth += 1
        elif character in "})":
            depth -= 1
        elif character == separator and depth == 0:
            pieces.append(text[current_start:index])
            current_start = index + 1

    pieces.append(text[current_start:])
    return pieces


def _parse_quoted_string(field_text):
    """从一个字段里取出引号包起来的内容。没有引号返回 None。"""
    first_quote = field_text.find('"')
    if first_quote < 0:
        return None
    last_quote = field_text.rfind('"')
    if last_quote <= first_quote:
        return None
    return field_text[first_quote + 1:last_quote]


def load_signature_table(header_file_path):
    """
    从 offsets_AI.h 里读出 kSignatureTable[]。

    只读数组本身。文件顶部那些 constexpr 是给人看的老地址，工具不用它们 ——
    那些值会随游戏更新而过时，拿它们做判断只会每周报一次假警。
    """
    raw_bytes = read_entire_file(header_file_path)
    if raw_bytes is None:
        raise SignatureTableError("打不开特征码表：%s" % header_file_path)

    text = _strip_comments(_decode_source_text(raw_bytes))

    array_name_index = text.find("kSignatureTable")
    if array_name_index < 0:
        raise SignatureTableError("offsets_AI.h 里找不到 kSignatureTable[]")

    # kSignatureTable[] 后面先出现 '='，再出现 '{'。从 '=' 往后找更稳。
    equals_index = text.find("=", array_name_index)
    if equals_index < 0:
        raise SignatureTableError("kSignatureTable 后面没有 '='")
    array_body = _extract_braced_body(text, equals_index)

    definitions = []
    for entry_text in _split_top_level(array_body):
        entry_text = entry_text.strip()
        if not entry_text:
            continue
        definitions.append(_parse_table_entry(entry_text))

    if not definitions:
        raise SignatureTableError("kSignatureTable[] 是空的")

    return definitions


def _parse_table_entry(entry_text):
    """解析 { "名字", "特征码", 类型, 偏移, 边界, 修正, { 二级 } } 这一条。"""
    entry_body = _extract_braced_body(entry_text, 0)
    fields = _split_top_level(entry_body)

    if len(fields) != 7:
        raise SignatureTableError(
            "一条表项应该有 7 个字段，实际 %d 个：%s"
            % (len(fields), entry_text.strip()[:80]))

    definition = SignatureDefinition()

    definition.signature_name = _parse_quoted_string(fields[0]) or "(无名)"
    definition.pattern_text = _parse_quoted_string(fields[1]) or ""

    # 类型：SignatureKind::Function -> Function
    kind_text = fields[2].split("::")[-1].strip()
    if kind_text not in SignatureKind.ALL:
        raise SignatureTableError(
            "%s 的类型 '%s' 不认识（只能是 Function / Trampoline / Dword）"
            % (definition.signature_name, kind_text))
    definition.signature_kind = kind_text

    definition.pattern_offset = int(fields[3].strip())
    definition.boundary_length = int(fields[4].strip())
    definition.value_adjustment = int(fields[5].strip())

    # 二级：{ nullptr, 0 } 或 { "A1 ?? ?? ?? ?? C3", 0 }
    secondary_fields = _split_top_level(_extract_braced_body(fields[6], 0))
    if len(secondary_fields) != 2:
        raise SignatureTableError(
            "%s 的二级字段应该是 2 个值，实际 %d 个"
            % (definition.signature_name, len(secondary_fields)))
    if "nullptr" not in secondary_fields[0]:
        definition.secondary_pattern_text = _parse_quoted_string(secondary_fields[0])
    definition.secondary_pattern_offset = int(secondary_fields[1].strip())

    return definition


def find_definition_by_name(definitions, signature_name):
    """按名字找一条表项。找不到返回 None。"""
    for definition in definitions:
        if definition.signature_name == signature_name:
            return definition
    return None


# =============================================================================
#  四之二、解析：一条定义 -> 一个真地址
# =============================================================================

class SignatureResolveStatus:
    """解析状态。打印出来就是这一串短名字。"""

    OK = "OK"
    INVALID_PATTERN = "BAD-PATTERN"
    IMAGE_HAS_NO_CODE_SECTION = "NO-CODE-SECTION"
    NOT_FOUND = "MISS"
    NOT_UNIQUE = "NOT-UNIQUE"
    NOT_A_CALL_INSTRUCTION = "NOT-A-CALL"
    NOT_A_REFERENCE_INSTRUCTION = "NOT-A-REFERENCE"
    BAD_BOUNDARY_LENGTH = "BAD-BOUNDARY"
    SECONDARY_NOT_FOUND = "SECONDARY-MISS"
    SECONDARY_NOT_UNIQUE = "SECONDARY-NOT-UNIQUE"
    ADDRESS_OUT_OF_CODE_SECTION = "OUT-OF-CODE"


class ResolvedSignature:
    """一条定义解析出来的结果 + 诊断信息。"""

    def __init__(self, definition):
        self.definition = definition

        self.status = SignatureResolveStatus.OK
        self.error_message = ""

        self.hit_count = 0                        # 主码命中几处
        self.hit_relative_address = 0             # 主码命中处
        self.effective_relative_address = 0       # 命中处 + 偏移

        # 只有 Trampoline 才有的第二段：起点 + 边界
        self.is_hook_span = False
        self.boundary_end_relative_address = 0

        # 主结果。一律是【相对地址】，用的时候要加模块基址。
        self.has_relative_address = False
        self.relative_address = 0

        # 二级扫描的诊断信息
        self.secondary_hit_relative_addresses = []
        self.secondary_function_entry_relative_address = 0
        self.secondary_function_end_relative_address = 0
        self.secondary_hit_count_beyond_function_end = 0

    def is_ok(self):
        return self.status == SignatureResolveStatus.OK


class ImmediateValueSite:
    """从操作码上认出来的「那个 4 字节在哪儿、要不要减镜像基址」。"""

    __slots__ = ("immediate_offset", "is_absolute_address", "instruction_length")

    def __init__(self, immediate_offset, is_absolute_address, instruction_length):
        self.immediate_offset = immediate_offset
        self.is_absolute_address = is_absolute_address
        self.instruction_length = instruction_length


def read_uint32_at(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def read_int32_at(data, offset):
    return struct.unpack_from("<i", data, offset)[0]


def is_call_opcode(opcode_byte):
    """E8 = call rel32，E9 = jmp rel32。两条都是「+5+rel32」。"""
    return opcode_byte == 0xE8 or opcode_byte == 0xE9


# 认得出来的「引用指令」操作码。0x10/0x11 只在 F3/F2 0F 后面才算（下面拦）。
_SUPPORTED_REFERENCE_OPCODES = frozenset([
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D,       # mov 家族
    0xC6, 0xC7,                               # mov 立即数
    0xF7, 0xFF,                               # 组指令（含 FF 15 = call [imm32]）
    0x10, 0x11,                               # movss / movsd
])


def classify_immediate_value_site(instruction_bytes):
    """
    看【操作码】决定两件事：4 字节在哪儿、要不要减镜像基址。

    认不出来返回 None。规则：

        A1 / A3              moffs32，imm32 在 +1，绝对地址 -> 减
        66 A1 / 66 A3        imm32 在 +2（66 只改操作数尺寸，【不改地址宽度】），绝对地址 -> 减
        <op> <modrm>         imm32 在 +2
        66 <op> <modrm>      imm32 在 +3
        F3/F2 0F 10/11 <modrm>  imm32 在 +4

        mod=00 rm=101        没有基址寄存器，disp32 就是绝对地址 -> 减
        mod=10 rm!=4         [基址寄存器 + 位移]，是纯偏移 -> 不减
    """
    if len(instruction_bytes) < MINIMUM_INSTRUCTION_BYTE_COUNT:
        return None

    # A1 / A3：moffs32，直接就是绝对地址
    if instruction_bytes[0] == 0xA1 or instruction_bytes[0] == 0xA3:
        return ImmediateValueSite(1, True, 5)

    # 66 A1 / 66 A3
    if instruction_bytes[0] == 0x66 and instruction_bytes[1] in (0xA1, 0xA3):
        return ImmediateValueSite(2, True, 6)

    opcode_index = 0
    is_single_precision_floating_point = False
    if instruction_bytes[0] == 0x66:
        opcode_index = 1                        # 操作数尺寸前缀
    elif instruction_bytes[0] in (0xF3, 0xF2) and instruction_bytes[1] == 0x0F:
        opcode_index = 2                        # F3 0F 10 / F3 0F 11
        is_single_precision_floating_point = True

    opcode_byte = instruction_bytes[opcode_index]
    if opcode_byte not in _SUPPORTED_REFERENCE_OPCODES:
        return None
    # 0x10 / 0x11 单摆着不是 movss，必须有 F3/F2 0F 前缀才算
    if opcode_byte in (0x10, 0x11) and not is_single_precision_floating_point:
        return None

    modrm_index = opcode_index + 1
    modrm_byte = instruction_bytes[modrm_index]
    mod_bits = modrm_byte >> 6
    register_memory_bits = modrm_byte & 0x07

    # mod=00 rm=101：没有基址寄存器，disp32 就是绝对地址
    if mod_bits == 0 and register_memory_bits == 5:
        return ImmediateValueSite(modrm_index + 1, True, modrm_index + 5)
    # mod=10 且不是 SIB：disp32 是 [基址寄存器 + 位移]，是纯偏移，不是地址
    if mod_bits == 2 and register_memory_bits != 4:
        return ImmediateValueSite(modrm_index + 1, False, modrm_index + 5)

    return None


# -----------------------------------------------------------------------------
#  函数体末尾 = 第一条 ret 之后
# -----------------------------------------------------------------------------
#  为什么需要它：二级码（例如 A1 ?? ?? ?? ?? C3，mov eax,[全局] ; ret）通常
#  【不是】唯一的，靠「从函数入口往下取第一个」来定位。可是编译器常把几个
#  一模一样的小 getter 排在一起：
#
#      001C11B0  A1 40 0C E7 01  C3    mov eax, dword_1A70C40 ; ret   <- 要的
#      001C11C0  A1 44 0C E7 01  C3    mov eax, dword_1A70C44 ; ret   <- 隔壁的
#
#  两个函数只差 16 字节，后面都是 CC 填充，靠加长特征码【分不开】——
#  唯一的区别就是要找的那个值本身。只要把扫描范围收到函数体内，
#  第二个就自然被排除了：它在上一条 ret 之后。
#
#  ret 的两种编码都认：C3（ret）、C2 iw（ret imm16）。
#  这里只是「划一条边界」，不是完整反汇编：万一 C3 出现在某条指令的立即数里，
#  边界会画早。画早的后果是 SECONDARY-MISS（吵，一眼能看见），
#  而不是选错一个地址（静默，极难查）—— 失败方向是安全的。
# -----------------------------------------------------------------------------
def find_function_end_relative_address(code_section_data, code_section_start,
                                       code_section_end, function_entry_relative_address,
                                       maximum_scan_byte_count):
    scan_byte_count = code_section_end - function_entry_relative_address
    if scan_byte_count > maximum_scan_byte_count:
        scan_byte_count = maximum_scan_byte_count
    if scan_byte_count <= 0:
        return 0

    base_index = function_entry_relative_address - code_section_start
    for offset in range(scan_byte_count):
        byte_value = code_section_data[base_index + offset]
        if byte_value == 0xC3:
            return function_entry_relative_address + offset + 1
        if byte_value == 0xC2 and offset + 2 < scan_byte_count:
            return function_entry_relative_address + offset + 3

    return 0


# -----------------------------------------------------------------------------
#  三种类型各自的解析
# -----------------------------------------------------------------------------

def resolve_function_entry(effective_address, effective_pointer, definition, result):
    """有效地址处是一条 E8/E9，目标是 有效地址 + 5 + rel32。"""
    if not is_call_opcode(effective_pointer[0]):
        result.status = SignatureResolveStatus.NOT_A_CALL_INSTRUCTION
        result.error_message = ("偏移指到的地方是 %02X，不是 E8/E9，偏移是不是填错一位"
                                % effective_pointer[0])
        return

    # 读 rel32 要 5 个字节。指令落在 .text 最后几个字节时不够读，
    # 这时宁可报错也不要去读段外的字节 —— 那里是别的东西，读出来是个像地址的垃圾值。
    if len(effective_pointer) < 5:
        result.status = SignatureResolveStatus.ADDRESS_OUT_OF_CODE_SECTION
        result.error_message = ("这条 call 只剩 %d 个字节，读不出 4 字节 rel32"
                                % len(effective_pointer))
        return

    if definition.boundary_length != 5:
        result.status = SignatureResolveStatus.BAD_BOUNDARY_LENGTH
        result.error_message = ("边界填的是 %d，但 E8/E9 这条 call 是 5 字节"
                                % definition.boundary_length)
        return

    result.relative_address = effective_address + 5 + read_int32_at(effective_pointer, 1)
    result.has_relative_address = True


def resolve_hook_span(effective_address, definition, result):
    """有效地址就是 Hook 点，给出一对 (起点, 起点 + 边界)。"""
    if definition.boundary_length <= 0:
        result.status = SignatureResolveStatus.BAD_BOUNDARY_LENGTH
        result.error_message = "Trampoline 的边界必须大于 0（它是被搬走的那几条指令的总长）"
        return

    result.is_hook_span = True
    result.relative_address = effective_address
    result.boundary_end_relative_address = effective_address + definition.boundary_length
    result.has_relative_address = True


def resolve_dword_value(image, code_section_data, code_section_start, code_section_end,
                        effective_address, effective_pointer, definition, result):
    """有效地址处是一条引用指令，读它里面的 4 字节。

    有二级码时，先按主码反解出函数入口，再从函数入口向下扫二级码，取第一个命中。
    """
    value_site_address = effective_address

    if definition.secondary_pattern_text is not None:
        # ---- 第一步：主码这条 call 反解出函数入口 ----
        if not is_call_opcode(effective_pointer[0]):
            result.status = SignatureResolveStatus.NOT_A_CALL_INSTRUCTION
            result.error_message = ("这条是二级查找，主码偏移处应该是 call（E8/E9），实际是 %02X"
                                    % effective_pointer[0])
            return

        if len(effective_pointer) < 5:
            result.status = SignatureResolveStatus.ADDRESS_OUT_OF_CODE_SECTION
            result.error_message = ("这条 call 只剩 %d 个字节，读不出 4 字节 rel32"
                                    % len(effective_pointer))
            return

        function_entry_address = effective_address + 5 + read_int32_at(effective_pointer, 1)
        result.secondary_function_entry_relative_address = function_entry_address

        if function_entry_address < code_section_start or function_entry_address >= code_section_end:
            result.status = SignatureResolveStatus.ADDRESS_OUT_OF_CODE_SECTION
            result.error_message = "按 call 反解出来的函数入口跑到代码段外面了"
            return

        # ---- 第二步：在【函数体内】扫二级码，取第一个命中 ----
        # 二级码通常【不是】唯一的，所以命中多处是正常的，不算错。
        # 但范围必须收在函数体内（入口 -> 第一条 ret）。
        secondary_pattern = parse_signature_pattern(definition.secondary_pattern_text)
        if not secondary_pattern.is_valid:
            result.status = SignatureResolveStatus.INVALID_PATTERN
            result.error_message = "二级特征码有问题：" + secondary_pattern.error_message
            return

        # 函数体末尾。找不到 ret 就退回 64KB 上限。
        function_end_address = find_function_end_relative_address(
            code_section_data, code_section_start, code_section_end,
            function_entry_address, SECONDARY_SCAN_MAXIMUM_BYTE_COUNT)
        result.secondary_function_end_relative_address = function_end_address

        scan_end_address = function_end_address if function_end_address else code_section_end
        if scan_end_address > function_entry_address + SECONDARY_SCAN_MAXIMUM_BYTE_COUNT:
            scan_end_address = function_entry_address + SECONDARY_SCAN_MAXIMUM_BYTE_COUNT
        if scan_end_address > code_section_end:
            scan_end_address = code_section_end

        scan_byte_count = scan_end_address - function_entry_address
        scan_start_index = function_entry_address - code_section_start

        _, secondary_hits = scan_memory_buffer(
            code_section_data[scan_start_index:scan_start_index + scan_byte_count],
            function_entry_address, secondary_pattern, SECONDARY_HIT_REPORT_COUNT)

        if not secondary_hits:
            result.status = SignatureResolveStatus.SECONDARY_NOT_FOUND
            result.error_message = ("函数体 %08X..%08X（%d 字节，按第一条 ret 划的）里没有二级码"
                                    % (function_entry_address, scan_end_address, scan_byte_count))
            return

        for hit in secondary_hits:
            result.secondary_hit_relative_addresses.append(hit.relative_address)

        # 诊断：函数体【外面】还有多少同名命中。
        # 有的话说明这条二级码本身不唯一，只是靠 ret 边界兜住了 —— 要让你看见。
        if scan_end_address < code_section_end:
            beyond_byte_count = code_section_end - scan_end_address
            if beyond_byte_count > SECONDARY_SCAN_MAXIMUM_BYTE_COUNT:
                beyond_byte_count = SECONDARY_SCAN_MAXIMUM_BYTE_COUNT

            beyond_start_index = scan_end_address - code_section_start
            _, hits_beyond = scan_memory_buffer(
                code_section_data[beyond_start_index:beyond_start_index + beyond_byte_count],
                scan_end_address, secondary_pattern, SECONDARY_HIT_REPORT_COUNT)
            result.secondary_hit_count_beyond_function_end = len(hits_beyond)

        value_site_address = (secondary_hits[0].relative_address
                              + definition.secondary_pattern_offset)

        if value_site_address < code_section_start or value_site_address >= code_section_end:
            result.status = SignatureResolveStatus.ADDRESS_OUT_OF_CODE_SECTION
            result.error_message = "二级命中处 + 二级偏移 跑到代码段外面了"
            return

    # ---- 第三步：看操作码，决定 4 字节在哪儿、要不要减镜像基址 ----
    value_site_index = value_site_address - code_section_start
    value_site_bytes = code_section_data[value_site_index:value_site_index + 16]

    value_site = classify_immediate_value_site(value_site_bytes)
    if value_site is None:
        result.status = SignatureResolveStatus.NOT_A_REFERENCE_INSTRUCTION
        result.error_message = ("地址 %08X 处的字节是 %s，不是认得的引用指令形状"
                                % (value_site_address,
                                   " ".join("%02X" % b for b in value_site_bytes[:4])))
        return

    # 用「边界」对账：操作码推出来的长度必须和表里填的一致。
    # 这一条免费抓「偏移数错一位」—— 数错了那边就不是一条完整指令。
    if value_site.instruction_length != definition.boundary_length:
        result.status = SignatureResolveStatus.BAD_BOUNDARY_LENGTH
        result.error_message = ("边界填的是 %d，但 %08X 处那条指令按操作码推出来是 %d 字节"
                                % (definition.boundary_length, value_site_address,
                                   value_site.instruction_length))
        return

    value = read_uint32_at(value_site_bytes, value_site.immediate_offset)
    if value_site.is_absolute_address:
        value -= image.image_base_address      # 绝对地址 -> 换成相对地址
    value += definition.value_adjustment       # 修正

    result.relative_address = value
    result.has_relative_address = True


def resolve_signature_definition(image, definition):
    """解析一条。返回 ResolvedSignature（成功失败都在里面）。"""
    result = ResolvedSignature(definition)

    pattern = parse_signature_pattern(definition.pattern_text)
    if not pattern.is_valid:
        result.status = SignatureResolveStatus.INVALID_PATTERN
        result.error_message = pattern.error_message
        return result

    code_section = image.get_executable_code_section()
    if code_section is None:
        result.status = SignatureResolveStatus.IMAGE_HAS_NO_CODE_SECTION
        result.error_message = "镜像里找不到可执行的代码段"
        return result
    code_section_data, code_section_relative_address = code_section

    # 只要唯一，所以最多找 2 处就够了 —— 找到第 2 处就已经知道不能用。
    total_hit_count, hits = scan_memory_buffer(
        code_section_data, code_section_relative_address, pattern, 2)
    result.hit_count = max(total_hit_count, len(hits))

    if result.hit_count == 0:
        result.status = SignatureResolveStatus.NOT_FOUND
        result.error_message = "整个代码段一处都没命中"
        return result
    if result.hit_count > 1:
        result.status = SignatureResolveStatus.NOT_UNIQUE
        result.error_message = "命中 %d 处，必须唯一才敢用" % result.hit_count
        return result

    result.hit_relative_address = hits[0].relative_address

    code_section_start = code_section_relative_address
    code_section_end = code_section_start + len(code_section_data)
    effective_address = result.hit_relative_address + definition.pattern_offset

    if effective_address < code_section_start or effective_address >= code_section_end:
        result.status = SignatureResolveStatus.ADDRESS_OUT_OF_CODE_SECTION
        result.error_message = "命中处 + 偏移 跑到代码段外面了，偏移是不是填错了"
        return result
    result.effective_relative_address = effective_address

    effective_index = effective_address - code_section_start
    effective_pointer = code_section_data[effective_index:]

    if definition.signature_kind == SignatureKind.FUNCTION:
        resolve_function_entry(effective_address, effective_pointer, definition, result)
    elif definition.signature_kind == SignatureKind.TRAMPOLINE:
        resolve_hook_span(effective_address, definition, result)
    elif definition.signature_kind == SignatureKind.DWORD:
        resolve_dword_value(image, code_section_data, code_section_start, code_section_end,
                            effective_address, effective_pointer, definition, result)
    else:
        result.status = SignatureResolveStatus.INVALID_PATTERN
        result.error_message = "类型字段是个不认识的值"

    return result


def resolve_entire_signature_table(image, definitions):
    """把整张表跑一遍。"""
    return [resolve_signature_definition(image, definition) for definition in definitions]
