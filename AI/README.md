# 300DX9 特征码扫描

**每周游戏更新后，双击 `update_offset.bat`。** 就这一步。

它会读游戏 exe、按特征码表算出全部地址、写出 `offset_update.h`，
然后打印版本号和成功/失败。没有编译，没有 Visual Studio，没有 dump。

```
双击 update_offset.bat
   ↓
AI\offset_update.h        ← 生成物，DLL 直接 #include 它
```

解决的两个维护痛点：

1. **游戏一更新，offsets.h 里的地址全废** —— 现在改成"特征码 + 自动解析"，
   更新后双击一下就知道哪几条挂了、为什么挂。
2. **加英雄要找新地址** —— 表和解析逻辑分开：找地址是你的事（IDA 里手工查，
   顺便写好特征码），算地址是工具的事。

> **命名约定：本工程一律用英文全称，不用缩写。**
> `relative_address` 而不是 `rva`，`signature_definition` 而不是 `sig_def`。
> 名字长一点，但一眼就知道是什么，读代码不用猜。

---

## 一、文件结构

```
update_offset.bat          ★ 双击这个。找 Python -> 跑 offset_update.py
offset_update.py           ★ 主流程：读版本 -> 扫 -> 写文件 -> 打印
signature_scan.py            引擎：表解析 + PE 解析 + 特征码扫描 + 三种类型解析
signature_config.py        ★ 配置：四个路径（游戏两条 / 工具两条）

offsets_AI.h               ★ 特征码表 —— 全工程唯一需要手工维护的文件
                              工具只读它，从不写它
offset_update.h            ★ 生成物。DLL 直接 #include 它

old_c/                        旧版 C++ 实现，已经不用了。确认 Python 版没问题后整个删掉
```

四个路径全在 [signature_config.py](signature_config.py) 里，
改游戏路径只改那一个文件：

```python
GAME_VERSION_FILE_PATH = r"D:\Game\JumpGame\300Hero\LauncherCfg\NewUpdateCfg.xml"
GAME_EXECUTABLE_PATH   = r"D:\Game\JumpGame\300Hero\300.exe"
```

工具侧那两条（`offsets_AI.h` 和 `offset_update.h`）默认跟着脚本走 ——
脚本在哪个目录就在哪个目录读写，所以整个 AI 文件夹搬到别处也不用改。

**分层的关键**：`signature_scan.py` 只认「一块内存 + 长度」，
它不知道什么叫游戏、什么叫 DLL。所以同一套代码既能扫磁盘上的 300.exe，
也能扫一个 dump —— 两者都是文件布局，字节是同一种。

```
   磁盘上的 300.exe ──读文件──▶ ┌──────────────────────┐
                                │  signature_scan.py   │──▶ 命中处
   （以后想扫别的镜像 ────────▶ │  表解析 + 扫描 + 解析 │──▶ 最终地址
     同一套代码不用改）         └──────────────────────┘
                                        ▲
                          offset_update.py（唯一的调用方）
```

所以"工具里算出来的地址，游戏里必然可用"不是巧合 —— 两边算的是同一个数，
而且 DLL 那一侧**根本不算**，它用的是工具算好写进头文件里的常量。

---

## 二、调用链 —— 从入口到最下层

一共两个入口，但性质完全不同：

- **入口 A：`update_offset.bat`（双击）** —— 一条真正的函数调用链，下面完整展开。
- **入口 B：DLL** —— **不是调用链**。DLL 只 `#include "offset_update.h"`，
  拿到一堆 `constexpr uintptr_t`，加个模块基址就用。**没有函数调用。**

每个名字后面跟 `文件:行号`。

### 入口 A：双击 update_offset.bat

bat 只做两件事：找 Python，然后跑脚本。**没有编译步骤。**

```
update_offset.bat
│  where py / where python 找解释器（先 py，后 python，都没有就报错暂停）
│
└─ python -B offset_update.py
   │
   └─ run()                                                    offset_update.py:327
      │  【主流程】七步。返回值就是进程退出码。
      │  0 全成功 / 1 写出来了但有条目失败 / 2 硬错误。
      │
      ├─ 1. read_game_version(GAME_VERSION_FILE_PATH)          offset_update.py:110
      │  │  读游戏的 LauncherCfg\NewUpdateCfg.xml，抠出版本号。
      │  │  ★ 为什么不去读 exe 的版本资源：那里面是死数据，
      │  │    永远是 1.0.0.1 / 2012 年。见「几个坑 5」。
      │  ├─ _read_text_file(路径)                              offset_update.py:99
      │  │    读文件；UTF-8 解不开就按 GBK 解。最下层。
      │  └─ _extract_xml_element_text(文本, "ClientVersion")   offset_update.py:67
      │       找 <标签>...</标签>，取中间那段。最下层。
      │
      ├─ 2. read_generated_version(output_file_path)           offset_update.py:136
      │     读上次生成的 offset_update.h，找 "GameVersion=" 那一行。
      │     ★ 读完【只打印】，不做任何比较 —— 见下面「为什么不做版本比较」。
      │     └─ _extract_key_value_on_line(文本, "GameVersion") offset_update.py:84
      │
      ├─ 3. load_signature_table(SIGNATURE_DEFINITION_FILE_PATH)
      │  │                                                signature_scan.py:534
      │  │  ★ 把 offsets_AI.h 里的 kSignatureTable[] 读出来。
      │  │    这是全工程唯一手工维护的数据，所以解析要够稳：
      │  ├─ read_entire_file(路径)                       signature_scan.py:351
      │  │    整个文件读成 bytes。读不到返回 None。
      │  ├─ _decode_source_text(raw_bytes)               signature_scan.py:396
      │  │    UTF-8 先试，不行再按 GBK —— 这个文件被编辑器另存成 GBK
      │  │    是很容易发生的事，两种都认就不会因为编码炸掉。
      │  ├─ _strip_comments(text)                        signature_scan.py:414
      │  │    ★ 去 // 和 /* */ 注释。必须认得字符串字面量：
      │  │      特征码是引号包起来的，不管引号的话，注释里的引号
      │  │      会把后面整段代码都当成字符串。
      │  ├─ _extract_braced_body(text, '=' 的位置)       signature_scan.py:466
      │  │    从 '=' 往后找第一个 { ... }，靠数括号配对取出数组体。
      │  ├─ _split_top_level(数组体)                     signature_scan.py:484
      │  │    ★ 按【最外层】逗号切开，一条一个元素。
      │  │      这是解析的关键：二级特征码 { "A1 ...", 0 } 里面的逗号
      │  │      在第二层，不能被当成条目分隔符。
      │  └─ _parse_table_entry(一条)                     signature_scan.py:570
      │     │  期望 7 个字段，多一个少一个都报错（不猜）。
      │     ├─ _extract_braced_body(...)                 signature_scan.py:466
      │     ├─ _split_top_level(...)                     signature_scan.py:484
      │     └─ _parse_quoted_string(...)                 signature_scan.py:523
      │          取引号里的内容。最下层。
      │
      ├─ 4. read_entire_file(GAME_EXECUTABLE_PATH)      signature_scan.py:351
      │     读游戏 exe。★ 不需要 dump —— 磁盘上的 exe 就够了。
      │
      ├─ 5. PortableExecutableImage.parse_as_file_layout(字节)
      │  │                                                signature_scan.py:243
      │  │  解析 PE 头，记下段表。不复制数据。
      │  │  不是 MZ 开头就当作裸内存镜像（文件偏移 == 相对地址）。
      │  │  只要 PE32（0x10B）—— PE32+ 的 ImageBase 在别处，混用会读出垃圾。
      │  └─ struct.unpack_from(...)                       （标准库）
      │
      ├─ 6. get_executable_code_section()                signature_scan.py:315
      │     取 .text 的字节和段相对地址。找不到 .text 就退化为第一个可执行段。
      │     这里【提前挡一道】：后面整表解析也会要代码段，
      │     但在这里挡掉能给出更准确的错误（"没有代码段"而不是"N 条全 MISS"）。
      │
      ├─ 7. resolve_entire_signature_table(镜像, 表)      signature_scan.py:1018
      │  │  把整张表跑一遍（就是这一行）：
      │  │      [resolve_signature_definition(image, d) for d in definitions]
      │  │
      │  └─ resolve_signature_definition(镜像, 定义)       signature_scan.py:958
      │     │  ★★ 核心。一条特征码 -> 一个 ResolvedSignature。
      │     │
      │     ├─ parse_signature_pattern(特征码文本)        signature_scan.py:80
      │     │  │  把 "E8 ?? ?? ?? ?? 80 38 00 74 16" 解析成
      │     │  │  期望字节 + 比较掩码，并【编译成正则】。
      │     │  │  ★ 为什么用正则而不是 Python 循环逐字节比：
      │     │  │    正则引擎是 C 实现的。扫 21MB 的 .text，差别是
      │     │  │    「零点几秒」和「几十秒」。
      │     │  │    整趟跑完实测 0.26 秒（旧版 C++ 是 0.2 秒，基本一样）。
      │     │  通配写法：? ?? **   连续的 ?? 算【一个】通配字节。
      │     │
      │     ├─ get_executable_code_section()               signature_scan.py:315
      │     │
      │     ├─ scan_memory_buffer(代码段, 段相对地址, 特征码, 上限=2)
      │     │  │                                          signature_scan.py:181
      │     │  │  ★ 上限传 2：只要唯一，找到第 2 处就已经知道不能用，
      │     │  │    没必要找完。返回 (命中总数, 命中列表)。
      │     │  └─ pattern.compiled_regex.finditer(...)      （标准库正则）
      │     │  ★ 命中 0 处 -> MISS，命中 >1 处 -> NOT-UNIQUE，两种都直接返回。
      │     │    全局扫、必须唯一 —— 这是整张表的铁律，没有例外。
      │     │
      │     ├─ 有效地址 = 命中处 + 偏移；越界 -> OUT-OF-CODE
      │     │
      │     └─ 按【类型】分三路：
      │        │
      │        ├─ resolve_function_entry(...)   Function     signature_scan.py:801
      │        │    有效地址处必须是 E8/E9，目标 = 有效地址 + 5 + rel32。
      │        │    不是 E8/E9 -> NOT-A-CALL；边界 != 5 -> BAD-BOUNDARY。
      │        │    剩下不到 5 字节读不出 rel32 -> OUT-OF-CODE（见坑 6）。
      │        │
      │        ├─ resolve_hook_span(...)        Trampoline   signature_scan.py:827
      │        │    有效地址就是 Hook 起点，结果是【一对】：
      │        │    起点 = 有效地址，终点 = 有效地址 + 边界。
      │        │    边界 <= 0 -> BAD-BOUNDARY。
      │        │
      │        └─ resolve_dword_value(...)      Dword        signature_scan.py:840
      │           │  【有二级码时】先反解出函数入口，再从入口向下扫二级码：
      │           ├─ is_call_opcode(字节)                    signature_scan.py:688
      │           │    主码偏移处必须是 E8/E9，否则 NOT-A-CALL。
      │           ├─ 函数入口 = 有效地址 + 5 + rel32
      │           │    跑到代码段外 -> OUT-OF-CODE。
      │           ├─ parse_signature_pattern(二级特征码)      signature_scan.py:80
      │           ├─ find_function_end_relative_address(...) signature_scan.py:777
      │           │  │  ★★ 划出【函数体】的末尾 = 第一条 ret 之后。
      │           │  │  ret 两种编码都认：C3（-> +1）、C2 iw（-> +3）。
      │           │  │  为什么必须有这一步见「几个坑 2」——
      │           │  │  编译器爱把几个一模一样的小 getter 排在一起。
      │           │  │  找不到 ret 返回 0，退回到 64KB 上限。
      │           │  └─ read_int32_at / read_uint32_at        signature_scan.py:680/684
      │           ├─ scan_memory_buffer(从函数入口起, 到函数体末尾)
      │           │                                       signature_scan.py:181
      │           │    取【第一个】命中。二级码通常不唯一，命中多处是正常的。
      │           │    一处没有 -> SECONDARY-MISS。
      │           ├─ scan_memory_buffer(函数体【外面】)     signature_scan.py:181
      │           │    纯诊断：数一下函数外还有几处同名，填进
      │           │    secondary_hit_count_beyond_function_end 让你看见（不算错）。
      │           ├─ 值所在处 = 二级命中处 + 二级偏移
      │           │
      │           └─ classify_immediate_value_site(指令字节)
      │              │                                       signature_scan.py:702
      │              │  ★ 看【操作码】决定两件事：4 字节在哪儿、
      │              │    要不要减镜像基址。认不出来 -> NOT-A-REFERENCE。
      │              │  规则见「六、字段速查」。
      │              ├─ ★ 边界对账：操作码推出来的长度必须 == 表里填的边界，
      │              │    对不上 -> BAD-BOUNDARY。
      │              │    这一条免费抓「偏移数错一位」。
      │              └─ 值 = 读 4 字节；绝对地址就减 ImageBase；再加修正
      │
      ├─ 8. print_resolve_table(结果列表)                   offset_update.py:276
      │     逐条打印：函数名 / 类型 / 状态 / 结果，外加二级扫描的诊断
      │     （"函数体外还有 N 处同名，已按 ret 排除"）。
      │
      └─ 9. write_offset_update_header(...)                 offset_update.py:243
         │  写 offset_update.h。【UTF-8 带 BOM】——
         │  DLL 那边编译时不一定带 /utf-8，带 BOM 编译器才能认出这是 UTF-8，
         │  否则里面的中文注释会按 GBK 解释成乱码。见「几个坑 1」。
         │  ★ 失败的条目【不写出常量】。少一个常量是编译错误（很吵），
         │    写一个错的值是静默错误（极难查）。要让失败变吵。
         ├─ build_offset_update_text(...)                   offset_update.py:175
         │  ├─ format_current_local_time()                  offset_update.py:162
         │  └─ get_constant_trailing_comment(类型)           offset_update.py:166
         └─ open(路径, "wb").write(BOM + UTF-8 文本)         （标准库）
```

### 入口 B：DLL

```cpp
#include "offset_update.h"     // 工具生成的。路径在 signature_config.py 里配

// 模块基址 —— 主模块 = 游戏 exe，不是本 DLL
uintptr_t moduleBaseAddress = (uintptr_t)GetModuleHandleW(nullptr);

// 直接用。常量一律是【相对地址】，用的时候加基址。
void* castSkillFunction = (void*)(moduleBaseAddress + offset_func_castskill);
int   clockValue        = *(int*)(moduleBaseAddress + dword_Clock);

// Trampoline 是【成对】的：Start 和 End 都在头文件里
WriteJmp((void*)(moduleBaseAddress + Trampoline_HookSendPack_Start),
         (void*)(moduleBaseAddress + Trampoline_HookSendPack_End),
         Trampoline_YourHook);
```

**没有初始化步骤，没有日志回调，没有"解析失败"这一档。**
常量是编译期就定死的，失败只可能发生在编译期（少一个常量 -> 编译错误）。

**这就是"只做独立工具"换来的东西**：DLL 里不存在"运行时解析失败"
这个状态，也就不存在"要不要在被破坏的地址上装 Hook"这种危险决策。

**★ 别在 DLL 里自己加个 0x400000。** 生成物里记的 `ImageBase=0x00400000`
是**磁盘 exe** 的基址（文件里写的 PreferredBase）。
游戏跑起来时基址是**随机**的（实测 0xA00000 那一档），
永远用 `GetModuleHandleW(nullptr)` 拿，永远 `基址 + 相对地址`。

---

## 三、为什么不做版本比较

之前有个"版本没变就跳过扫描"的逻辑，**删掉了**。现在每次运行都扫。

原因是一个很坏的失败模式：

```
你改了 offsets_AI.h  ->  双击 bat  ->  它说"版本没变，跳过扫描"
                                    你以为生效了，其实生成物还是旧的
```

游戏版本没变，但你改的是**表**，不是游戏。跳过扫描省下的是 0.26 秒，
换来的是一个**静默成功**的假象 —— 不划算。

**版本号照样记录、照样打印**，只是不拿它做判断：

```
 游戏版本     : 2026.09.24.2   (InnerVersion 1274)
 上次生成版本 : 2026.09.24.2   (InnerVersion 1274)
```

两行并排给你看，比不比是你自己的事。生成物顶部也会记下这一版的版本号。

---

## 四、配置添加

### 4.0 表长什么样

`offsets_AI.h` 底部那张 `kSignatureTable[]`，一条 7 个字段：

```cpp
{
    "offset_func_castskill",                  // 1. 名字（和顶部 constexpr 同名，方便对账）
    "84 C0 74 1B ... 50 E8",                  // 2. 特征码
    SignatureKind::Function,                  // 3. 类型
    16,                                       // 4. 偏移
    5,                                        // 5. 边界
    0,                                        // 6. 修正
    { nullptr, 0 }                            // 7. 二级
},
```

四个数字字段的统一口径 —— **都从「有效地址」起算，不是从命中处**：

```
                 命中处                        有效地址
                   │                             │
   特征码命中 ─────┼─────────────────────────────┼──────▶
                   │◀───────── 偏移 ─────────────▶│

   有效地址处那条指令：          长度 = 边界（工具会自己推一遍和它对账）
                                里面的 4 字节 = 读出来的值 + 修正
```

| 字段 | 含义 | 怎么填 |
|---|---|---|
| **偏移** | 命中处 → 有效地址 | 特征码末尾就是目标 -> 末尾那个字节的下标。可为负（往前指） |
| **边界** | 有效地址处那条指令（或那一段）的字节数 | Function 恒为 5；Trampoline 是 `End - Start`；Dword 是那条引用指令的长度 |
| **修正** | 读出来的 4 字节要加的数 | 默认 0。只有指令里的值本身不是你要的那个数时才用 |
| **二级** | `{ "特征码", 偏移 }` 或 `{ nullptr, 0 }` | 只有"进 call 取"的 Dword 才填。空 = 直接取 |

**★ 偏移和边界工具都会自己再推一遍，填错了会报错** ——
这一条免费抓「偏移数错一位」。所以别猜，先填个值跑一遍，它会告诉你对不对。

### 4.1 加一条 Function（要 call 进去的函数）

表里四条 `offset_func_*` 都是这一类。共同点：**特征码里有一个字节是那条 `call`**。

```cpp
{
    // 原 offsets.h: offset_func_getCDRecordObj = 0x81D260
    "offset_func_getCDRecordObj",
    "E8 ?? ?? ?? ?? 80 38 00 74 16",
    SignatureKind::Function, 0, 5, 0, { nullptr, 0 }
},
```

- **特征码**：`E8` + `?? ?? ?? ??`（rel32 必须通配掉，它随版本变）+ 后面几个字节凑唯一。
- **偏移**：`call` 在特征码里的下标。在开头就是 `0`；
  `offset_func_treeFindPlayerObj` 是 30 字节里**只有最后一个字节是 call** -> `29`。
- **边界**：恒为 `5`（`E8` + 4 字节 rel32）。填别的会报 `BAD-BOUNDARY`。
- **解出来的值**：`有效地址 + 5 + rel32` —— 那个函数真正的地址。

> `offset_func_castSummonerSkill` 注意：它的特征码里**有两条 call 的 E8**
> （偏移 18 和偏移 25）。偏移填的是**你要的那一条**，别看错。

### 4.2 加一条 Trampoline（Hook 点）

表里三条 `Trampoline_*` 都是这一类。结果是一对 `(Start, End)`。

```cpp
{
    // 原 offsets.h: Trampoline_HookSendPack_Start = 0x1DFAE4
    //               Trampoline_HookSendPack_End   = 0x1DFAEE
    "Trampoline_HookSendPack",
    "0F B7 46 08 89 85 1C FF FF FF E8 ?? ?? ?? ?? 84 C0 74 68",
    SignatureKind::Trampoline, 0, 10, 0, { nullptr, 0 }
},
```

- **偏移**：`Start` 在特征码里的下标。`0` 表示命中处就是 `Start`。
- **边界**：`End - Start`，**被搬进 trampoline 的那几条指令的总长**。
  ★ 这个差值在任何版本都不变，所以【只需要定位 Start】—— 表里不用填 End。
  现有三条分别是 `6` / `6` / `10`。
- **两个硬条件**（要人工确认，工具帮不了你）：
  `边界 >= 5`（要放得下 5 字节的 `E8 rel32`），
  且 `Start`、`End` 都必须是**指令边界** —— 写到指令中间就是"游戏在无关的地方崩溃"。
- 生成物里出的是两个常量：`Trampoline_HookSendPack_Start` / `_End`。

**怎么选 `Start` / `End`**：在 x64dbg 里看，`Start` 是你要拦的那条指令，
`End` 是**覆盖到哪条指令为止的下一条指令地址**。例子：

```
001DFAE4  0F B7 46 08        movzx eax, word ptr [esi+8]     ◀─ Start（要重放）
001DFAE8  89 85 1C FF FF FF  mov [ebp-0E4h], eax            ◀─ 也要重放
001DFAEE  E8 ...             call sub_D70E70                 ◀─ End（不重放，从这里回去）
```

`End - Start = 0xA = 10`。前两条指令会被搬进 trampoline 原样重放，
`E8` 覆盖掉 `[Start, End)`，执行完 `ret` 回到 `End`。

### 4.3 加一条 Dword（取指令里的 4 字节）

分两种，先判断你要的值**存在某个固定地址**，还是**某个函数返回的**：

| 在 x64dbg 里看到的那行 | 是什么 | 二级填什么 |
|---|---|---|
| `mov eax, [0x246A76C]` | 指令里**直接引用**的全局变量 | `{nullptr, 0}` — 直接取 |
| `mov [0x246A76C], eax` | 同上，写入方向 | `{nullptr, 0}` |
| `mov ax, word ptr [esi+46F38h]` | 结构体成员偏移，**不是地址** | `{nullptr, 0}` |
| `call sub_7C890`，你要的是**它返回的** dword | 值只在运行时存在 | **填二级码**，进那个函数找 |
| `mov ecx, [ebx+0x46F38]`，但那是死偏移 | 结构体偏移 | **别进表**，写死在自己 header 里 |

#### 情况 A：直接取（二级留空）

```cpp
{
    // 原 offsets.h: dword_SkillTable_Slot_SkillIDOffset = 0x46F38
    // 偏移 -6 指到【上一条】指令的起点：mov ax, word ptr [esi+46F38h]，6 字节。
    "dword_SkillTable_Slot_SkillIDOffset",
    "66 A3 ?? ?? ?? ?? B8 02 00 00 00 66 A3",
    SignatureKind::Dword, -6, 6, 0, { nullptr, 0 }
},
```

**偏移可以是负的** —— 这里 `-6` 指到命中处**往前 6 字节**那条指令的开头。

**这个例子的修正也是 0**，因为读出来的 `0x46F38` 恰好就是你要的结构体偏移。
这就是"减不减 ImageBase 由操作码决定"的好处：

```
66 A3 <imm32>   moffs 绝对寻址  ->  imm32 是绝对地址   ->  自动减 ImageBase
8B 86 <disp32>  [寄存器+偏移]   ->  disp32 是结构体偏移 ->  自动【不】减
```

`66 A3` 是绝对寻址，但这一条正好是**上一条指令** `mov ax, word ptr [esi+46F38h]`，
它是 `8B 86` 形式 —— 所以读出来的是纯偏移，不减。
**同一条特征码、两个偏移，出来两个语义完全不同的值**，靠的就是这个自动判断。

#### 情况 B：进 call 取（填二级）

你在 `offsets_AI.h` 里注释的"首指令就是 call，进入 call 进行二级扫描"就是这个：

```cpp
{
    // 原 offsets.h: dword_HoverStruct = 0x1A6A76C
    "dword_HoverStruct",
    "E8 ?? ?? ?? ?? 8B C8 E8 ...",              // 主码：那条 call
    SignatureKind::Dword, 0, 5, 0,
    { "A1 ?? ?? ?? ?? 8B 4D F4 64 89 0D 00 00 00 00 59 5E 8B E5 5D C3", 0 }
    //  二级特征码                                         二级偏移
},
```

工具做的事，按顺序：

```
1. 主码命中、唯一
2. 有效地址处必须是 E8/E9  ->  函数入口 = 有效地址 + 5 + rel32
3. 从函数入口向下扫二级码，扫到【第一条 ret 为止】  <- ★ 关键，见「几个坑 2」
4. 取【第一个】命中
5. 值所在处 = 第一个命中 + 二级偏移
6. 看那条指令的操作码，决定 4 字节在哪儿、要不要减 ImageBase
```

- **二级特征码**通常**不是唯一的**（`A1 ?? ?? ?? ?? C3` 就是一句
  `mov eax, [全局]` + `ret`，全镜像到处都是）。靠"限制在函数体内 + 取第一个"定位。
- **二级偏移**一般是 `0`（二级命中处就是那条引用指令）。
- **同一处命中可以出两个元素**，靠偏移区分 ——
  `dword_CurrentProcessed_SkillID` 和 `dword_SkillTable_Slot_SkillIDOffset`
  就是共用同一条特征码（偏移 `0` 和 `-6`）。
  **同一条码只有一个命中处**，所以这不是重复劳动。

#### 修正：指令里的值不是你要的那个数

```cpp
{
    // 指令里的立即数是 X+2，所以要修正 -2 才回到 X。
    "dword_CurrentProcessed_SkillID",
    "66 A3 ?? ?? ?? ?? B8 02 00 00 00 66 A3",
    SignatureKind::Dword, 0, 6, -2, { nullptr, 0 }
},
```

`66 A3` 这条指令写的是 `[X+2]`，读出来的就是 `X+2`。填 `-2` 补回来。
**只有在"读出来的数看着像、但差一点点"时才用修正**，别拿它当万能膏药。

#### ★ 分界线

**签名表负责地址（会随版本变），你自己的 header 负责偏移（结构体布局，不随版本变）。**

`0x46F38` 这种结构体偏移，只有当它**恰好出现在指令的立即数里**、
可以顺手读出来时才进表（那是白捡的）；否则一律写死在你自己的 header 里。

### 4.4 加完之后

```bat
双击 update_offset.bat
```

新条目大面积报 `MISS` 说明码里混进了**随版本变的字节**
（最典型的是半截 `call` 位移，见「几个坑 3」）—— 重新取码，别急着用。

---

## 五、出错了怎么办

` offsets_AI.h` 改完双击，只看最后那个 `[成功]` / `[失败]`。

报错状态是这一串短名字，含义和对策：

| 状态 | 含义 | 怎么办 |
|---|---|---|
| `MISS` | 一处都没命中 | 码指的地方没了，重新取码 |
| `NOT-UNIQUE` | 命中多处 | 码太短，加长特征码 |
| `BAD-PATTERN` | 特征码字符串本身有问题 | 检查写法（非法字符 / 全是通配） |
| `NOT-A-CALL` | 偏移指到的不是 `E8`/`E9` | 偏移数错了 |
| `NOT-A-REFERENCE` | 偏移指到的不是引用指令 | 偏移数错了 |
| `BAD-BOUNDARY` | 边界和操作码推出来的长度不一致 | 偏移或边界数错了（**多半是偏移**） |
| `SECONDARY-MISS` | 函数体里没有二级码 | 二级码要重取，或函数变了 |
| `OUT-OF-CODE` | 算出来的地址跑到 `.text` 外面 | 偏移填错了 |
| `NO-CODE-SECTION` | 这镜像没有代码段 | 文件传错了 |

**这些全是「失败」，没有"疑似"这一档。** 因为全局扫 + 必须唯一，
不存在"地址看着像对但差几 KB"的中间状态 —— 要么唯一命中并算出来，要么明确报错。

另外 `[失败] 特征码表读不动：...` 是表本身写坏了（少个括号、字段数不对），
错误信息会指出是**哪一条**、**哪个字段**。

---

## 六、字段速查：三种类型

```python
有效地址 = 命中处 + 偏移        # 先算这个
然后按【类型】解释「有效地址」处的那条指令
```

| `SignatureKind` | 有效地址处是什么 | 结果 | 边界是 |
|---|---|---|---|
| `Function` | 一条 `E8`/`E9` | `有效地址 + 5 + rel32`（一个地址） | **5** |
| `Trampoline` | Hook 点本身 | 一对 `(有效地址, 有效地址 + 边界)` | `End - Start` |
| `Dword` | 一条引用指令 | 指令里那 4 字节 + 修正 | 那条指令的长度 |

**为什么"偏移"必须在"类型"之前**：有的特征码为了凑唯一性会从目标前面多取一段，
导致真正关心的那条指令落在特征码中间。例如 `offset_func_treeFindPlayerObj`：
特征码 30 字节，**最后一个字节才是 `call`**，所以偏移 `29` + `Function`。

**为什么"边界"每种类型都要填**：它是**交叉校验**用的。
工具按操作码自己推一遍长度，和表里填的对不上就报 `BAD-BOUNDARY`。
免费抓「偏移数错一位」—— 数错一位，那边就不是一条完整指令了。

**为什么"减不减 ImageBase"不用填**：完全由 ModRM 决定，填是多余的、且是错的机会。

```
绝对寻址      A1 / 66 A3 / 8B 0D ...   读出来是绝对地址   -> 减
[寄存器+偏移] 8B 86 ...                 读出来是结构体偏移 -> 不减
```

---

## 七、几个坑（都踩过）

### 1. 编码

- `.bat` **必须纯 ASCII**。cmd.exe 按 OEM 代码页（中文系统是 GBK）解析 `.bat`，
  UTF-8 中文注释会让脚本静默出错。所以 `update_offset.bat` 里一个中文都没有 ——
  **中文输出全部来自 Python**，Python 会用 Windows 控制台接口正确打印 Unicode，
  不需要 `chcp 65001`，也不需要 `SetConsoleOutputCP`。
- **生成物 `offset_update.h` 要带 BOM。** 它是 UTF-8 的，
  但 DLL 那边编译时不一定带 `/utf-8`，编译器会按 GBK 去读它 ——
  带 BOM 才能让编译器认出"这是 UTF-8"。
- **`offsets_AI.h` 两种编码都认。** 工具先按 UTF-8 解，解不开再按 GBK 解 ——
  被编辑器另存成 GBK 是很容易发生的事，不该因为这个就报一堆看不懂的错。
  （旧 C++ 版是靠 `/utf-8` 编译选项，配 GBK 文件会报 30 多条 warning C4828。）

### 2. ★★ 二级码必须限制在函数体内（靠第一条 `ret`）

**这是最隐蔽的一个坑，因为它会静默出错。**

`dword_Clock` 的二级码是 `A1 ?? ?? ?? ?? C3` —— 一句 `mov eax, [全局]` 加 `ret`。
编译器把两个一模一样的小 getter 排在了相邻 16 字节：

```
001C11B0  A1 40 0C E7 01  C3   mov eax, dword_1A70C40 ; ret   ◀─ 要的是这个
001C11C0  A1 44 0C E7 01  C3   mov eax, dword_1A70C44 ; ret   ◀─ 隔壁那个（差 4 字节）
```

两个函数**只差那 4 字节的立即数**，后面都是 `CC` 填充。
所以**加长特征码分不开它们** —— 唯一的区别就是你要找的那个值本身。

"从函数入口往下取第一个"这条规则当时**碰巧**是对的。
但如果编译器哪天把这两个 getter 调个头，就会静默返回一个**差 4 字节**的地址，
而且它是"看起来完全正常"的地址 —— 最难查的一类 bug。

**修法**：把二级扫描范围收在函数体内 —— 从函数入口到**第一条 `ret`**：

```
find_function_end_relative_address(...)
    C3       -> 返回 offset + 1
    C2 iw    -> 返回 offset + 3
    找不到   -> 返回 0，退回到 64KB 上限
```

这些二级码指向的都是固定的底层小函数（只有一个 `ret`），所以 `ret` 就是函数体末尾。
有了这条边界，上面那个"隔壁的"落在第一条 `ret` 之后，自然被排除。

**为什么这个划法即使不精确也是安全的**：它只是"划一条边界"，不是完整反汇编。
万一 `C3` 出现在某条指令的立即数里，边界会**画早**。
画早的后果是 `SECONDARY-MISS`（很吵，一眼能看见），
而不是选错一个地址（静默，极难查）—— **失败方向是安全的**。

**工具会把这件事打出来**：报告里如果出现
`函数体外还有 N 处同名，已按 ret 排除`，说明这条二级码本身不唯一，
全靠 `ret` 边界兜着。N 越大，越该考虑换一条更长的二级码。

现在 `dword_SkillTable` 和 `dword_Clock` 都会打出这一句（各有 1 处），
`dword_HoverStruct` 没有 —— 它的二级码本来就唯一。

### 3. ★ 特征码别在指令中间截断

取码窗口的最后一两条指令可能正好跨在边界上，
只按窗口长度去读，就会读到一个**残缺的 4 字节操作数**，
算出来的目标地址是垃圾，于是那 4 个字节被当成实字节留在特征码里 ——
可它们其实是随版本变的。

症状：`dword_HoverStruct` 的码末尾曾经是 `E8 48 CC` —— 一条 `call` 的操作码
加上它的 rel32 位移的**头两个字节**。`48 CC` 在那个版本是对的，
换个版本 call 点和目标的距离一变，这个码就再也命中不了。
（旧版是靠 `StabilizeWindow` 按"窗口长度 + 3"去读来解决的，
现在没有自动取码了，全部手工写 —— **手工写码时更要留神别在指令中间断**。）

**自查**：新加的条目如果只有当前版本成功、换个 dump 就全 `MISS`，
几乎一定是这个病 —— 重新取码，别去调偏移。

### 4. `66` 前缀不改地址宽度

`66` 只是**操作数尺寸**前缀（`eax` → `ax`），**不改变 moffs 地址的宽度**。

```
66 A1 <imm32>    imm32 在 +2（不是 +3！）
66 8B 0D <imm32> imm32 在 +3   ← 真正顺延的是「前缀 + opcode + ModRM」三条字节
66 8B 86 <disp32>  是 7 字节，不是 6 字节（6 字节的那个是 8B 86）
```

**教训**：偏移要照着**指令编码规则**推，不要照着"看起来像几个字节"数。
写完拿几条真实指令在 x64dbg 里对一遍。

### 5. 游戏版本号不在 exe 里

`300.exe` 的版本资源（`VS_VERSION_INFO`）是**死数据**：
永远是 `1.0.0.1` / 2012 年。拿它判断版本会永远认为"没更新"。

真版本在这里：

```
D:\Game\JumpGame\300Hero\LauncherCfg\NewUpdateCfg.xml
    <ClientVersion>2026.09.24.2</ClientVersion>
    <InnerVersion>1274</InnerVersion>
```

### 6. 段的「文件大小」和「内存大小」不是一回事

PE 段头里有两个大小：`rawDataSize`（磁盘上占多少字节）和 `virtualSize`
（加载到内存里占多少）。**内存里那截尾巴在文件里根本不存在**，
必须取两者中小的那个，否则会读到别的段的数据。

这也是一类"读出来是个像地址的垃圾值"的坑：不会崩，只会给你一个错的值。
Python 版里这个切片在 `get_executable_code_section` 里，只有一处，好核对。

### 7. rel32 读到段外

C++ 版读 `有效地址 + 1` 处的 4 字节时**不做边界检查** ——
指令正好落在 `.text` 最后几个字节时，它会读进文件里紧跟着的别的段。
不崩，但得到的是垃圾。Python 版在这里**主动报 `OUT-OF-CODE`**：
读不到 4 字节就不猜。

（这是移植时唯一一处**故意和 C++ 版行为不同**的地方，
而且只会影响"特征码命中在 .text 最末尾"这种本来就不该发生的情况。）

---

## 八、为什么只做独立工具（不做运行时扫描）

一开始是"工具 + DLL 运行时扫描"两个入口共用引擎。**现在 DLL 侧那一半删掉了。**

| | 独立工具 | （原来的）DLL 运行时扫描 |
|---|---|---|
| 一次迭代 | **0.26 秒** | 改代码→编译→注入→启游戏→进对局，几分钟 |
| 跨版本验证 | 能拿历史 dump 对比 | 做不到（不可能同时跑 8 个版本的游戏） |
| 运行时开销 | 0（不跑） | 扫 21MB 的 .text，每次注入都要 |
| 失败模式 | 报错，你改表 | **运行时才知道挂了**，而且是在别人机器上 |
| 依赖 | 只有 Python 和 300.exe | 要注入成功、要能读进程内存 |

**关键的一条是"失败模式"**：运行时扫描失败的时候，你已经进了游戏。
这时只有两个坏选择 —— 要么用一个算错的地址装 Hook（把 `CALL` 写进别的指令中间，
表现为"游戏在完全无关的地方崩溃"），要么放弃这次注入。

**而"只做独立工具"让这个状态根本不存在**：常量是编译期常量，
工具没算出某个值，生成物里就没有那个常量，DLL 编译不过。
**失败从"运行时静默"变成了"编译期响亮"。**

代价是：游戏更新后必须先双击一次 bat、重新编译 DLL，才能进游戏。
更新本来就不是天天发生的事，这个代价值得。

---

## 九、实测数据

12 条特征码在 `300.exe` 上全部解析成功，和旧 C++ 版输出**逐个字节相同**：

```
名称                                      类型         状态              结果
offset_func_castskill                     Function     OK               0x00AFEE60
offset_func_castSummonerSkill             Function     OK               0x00774460
offset_func_treeFindPlayerObj             Function     OK               0x000601F0
offset_func_getCDRecordObj                Function     OK               0x0081D260
Trampoline_CastNormalSkillRecvCD          Trampoline   OK               0x00AFDBD1 ~ 0x00AFDBD7
Trampoline_SwitchSkill                    Trampoline   OK               0x00824B59 ~ 0x00824B5F
Trampoline_HookSendPack                   Trampoline   OK               0x001DFAE4 ~ 0x001DFAEE
dword_SkillTable                          Dword        OK               0x01A6A4B8
dword_SkillTable_Slot_SkillIDOffset       Dword        OK               0x00046F38
dword_CurrentProcessed_SkillID            Dword        OK               0x01E2D670
dword_Clock                               Dword        OK               0x01A70C40
dword_HoverStruct                         Dword        OK               0x01A6A76C
```

二级扫描的诊断也一致：

```
dword_SkillTable    二级: 函数体 0006E620..0006E675  命中 1 处  0x0006E660  | 函数体外还有 1 处同名，已按 ret 排除
dword_Clock         二级: 函数体 001C11B0..001C11B6  命中 1 处  0x001C11B0  | 函数体外还有 1 处同名，已按 ret 排除
dword_HoverStruct   二级: 函数体 0007C890..0007C8E5  命中 1 处  0x0007C8D0
```

整趟（读 28MB exe + 扫 21MB .text + 写文件）**0.26 秒**。

**正常版本位移是几 KB 量级**（历史上 8.13 → 9.23 六个版本累计才漂 30~40KB）。
所以"地址变了"不等于"码挂了" —— 这也是"全局扫 + 必须唯一"能成立的原因：
唯一命中就够了，不需要期望地址。

---

## 十、还没做完的

- `offsets_AI.h` 里还有一批常量没进特征码表（`offset_SlotStruct`、
  `dword_MousePos`、`dword_SummonerSkillStruct`、`xRight`、`xBottom` 等）。
  真全局变量照 4.3 情况 A 加；**`Slot_SkillIDOffset` 是结构体偏移不是地址**，
  不该进表。
- 自动取码工具（旧 C++ 版的 `--gen` / `--gen-global` / `--cross-version matrix`）
  跟着旧代码一起进 `old_c` 了。现在加条目是**在 IDA 里手工写特征码**。
  如果哪天觉得手工写太慢，可以把它搬成 Python —— 大概 400 行。
- 失败时只报"哪几条挂了"，还没做"挂的那条往前/往后挪几字节会命中什么"这类提示。
- 英雄框架（`IHero` 接口 + 注册表 + `GameApi`）还没开始，属于"其他先放着"那一档。
