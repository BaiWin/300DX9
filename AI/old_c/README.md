# 300DX9 特征码扫描

解决两个维护痛点：

1. **游戏一更新，offsets.h 里的地址全废** —— 现在改成"特征码 + 自动解析"，
   更新后双击一个 bat，就知道哪几条挂了、为什么挂。
2. **不知道做独立工具还是塞进 DLL** —— 答案是**只做独立工具**。
   工具扫磁盘上的 `300.exe`，生成一份 `offset_update.h`，
   DLL 直接 `#include` 它。**DLL 里一行扫描代码都没有。**
   理由见「五、为什么只做独立工具」。

> **命名约定：本工程一律用英文全称，不用缩写。**
> `RelativeAddress` 而不是 `Rva`，`SignatureDefinition` 而不是 `SigDef`，
> `PortableExecutableImage` 而不是 `PeImage`。
> 名字长一点，但一眼就知道是什么，读代码不用猜。

---

## 一、文件结构

```
offsets_AI.h                         ★ 特征码表 —— 全工程唯一需要手工维护的文件
                                       顶部是原来的 constexpr（老代码还在引用，保持原样）
                                       底部是 kSignatureTable[]，工具读它

SignatureDefinitionResolve.h/.cpp   表的语义层：按一条定义算出真地址
SignatureScan.h/.cpp                引擎：PE 解析 + 特征码扫描（上面那个用它）

SignatureConfig.h                    ★ 配置：三个路径都在这儿，只有这一个地方写死路径
SignatureGameVersion.h/.cpp          读游戏版本号（两个来源：xml、已生成的头）
SignatureOutputWriter.h/.cpp         写 offset_update.h
SignatureUpdateFlow.h/.cpp           把上面几步串成一条流程
SignatureTool.cpp                    工具本体：main + 报告 + 取码（--gen / --gen-global）

build_tool.bat                       ★ 双击这个。编译 + 跑更新流程
offset_update.h                      ★ 生成物。DLL 直接 #include 它
```

分层的关键：**引擎只认「一块内存 + 长度」**，它不知道什么叫游戏、什么叫 DLL。

```
   磁盘上的 300.exe ──读文件──▶ ┌────────────────────┐
                                │  SignatureScan 引擎 │──▶ 命中处
   （以后想扫别的镜像 ────────▶ │  + 语义解析层       │──▶ 最终地址
     同一套代码不用改）         └────────────────────┘
                                        ▲
                        SignatureTool.cpp（唯一的调用方）
```

所以"工具里算出来的地址，游戏里必然可用"不是巧合 —— 两边算的是同一个数，
而且 DLL 那一侧**根本不算**，它用的是工具算好写进头文件里的常量。

---

## 二、调用链 —— 从入口到最下层

一共两个入口，但它们的性质完全不同：

- **入口 A：`build_tool.bat`（双击）** —— 一条真正的函数调用链，下面完整展开。
- **入口 B：DLL** —— **不是调用链**。DLL 只 `#include "offset_update.h"`，
  拿到一堆 `constexpr uintptr_t`，加个模块基址就用。**没有函数调用。**

每个函数后面跟 `文件:行号`。

### 入口 A：双击 build_tool.bat

`build_tool.bat` 做三件事：编译工具 → 单独编译一次引擎（编译检查）→ 跑工具。
工具不带参数就是更新流程：

```
main(argc, argv)                                              SignatureTool.cpp:916
│  【控制台入口】SetConsoleOutputCP(CP_UTF8) 把输出页切到 UTF-8（否则中文报告乱码）。
│  ★ argc < 2（双击 bat 就是这种）-> 直接走更新流程。
│    命令行参数是给开发期用的，见下面「入口 A 的另外四条路」。
│
└─ RunOffsetUpdateFlow(forceRescan = false)                   SignatureUpdateFlow.cpp:173
   │  【更新流程】六步，对应你说的「版本号不同就扫描」。
   │  返回：0 成功（含"版本没变"）/ 1 写出来了但有条目失败 / 2 硬错误。
   │
   ├─ 1. ReadGameVersionFromLauncherConfig(路径, 出参:版本)    SignatureGameVersion.cpp:103
   │  │  读游戏目录的 LauncherCfg\NewUpdateCfg.xml，抠出 <ClientVersion> / <InnerVersion>。
   │  │  ★ 为什么不去读 exe 的版本资源：exe 里的 VS_VERSION_INFO 是【死数据】，
   │  │    永远是 1.0.0.1 / 2012。真版本在这个 xml 里。见「几个坑 9」。
   │  ├─ ReadWholeTextFile(路径, 出参:文本)                    SignatureGameVersion.cpp:16
   │  │    整个文件读进 std::string（fopen/fread 那一套）。
   │  └─ ExtractXmlElementText(文本, "ClientVersion", 出参)    SignatureGameVersion.cpp:36
   │       找 <标签>...</标签>，返回中间那段。最下层。
   │
   ├─ 2. ReadGameVersionFromGeneratedHeader(路径, 出参:版本)   SignatureGameVersion.cpp:131
   │  │  读上次生成的 offset_update.h，找顶部注释里的 "GameVersion=" 那一行。
   │  │  文件不存在 / 那行找不到 -> 返回 false（当作"没生成过"，必然要重扫）。
   │  ├─ ReadWholeTextFile(...)                                SignatureGameVersion.cpp:16
   │  └─ ExtractKeyValueOnLine(文本, "GameVersion", 出参)      SignatureGameVersion.cpp:81
   │       在一行里找 "键="，取到行尾，两头去空白。
   │
   ├─ 3. IsSameGameVersion(当前, 上次)                          SignatureGameVersion.cpp:160
   │      ★ 两个字段（ClientVersion 和 InnerVersion）都要相等、【且都读到了】才算没变。
   │        任一侧读不到 -> 返回 false -> 继续扫描。
   │        这里的取舍：读不到就重扫（多花 200 毫秒），而不是跳过（可能漏一次更新）。
   │
   ├─ 4. LoadGameImage(游戏exe路径, 出参:缓冲, 出参:镜像, 出参:错误)
   │  │                                                          SignatureUpdateFlow.cpp:29
   │  │  读游戏 exe 并按【文件布局】解析。★ 不需要 dump。
   │  ├─ ReadEntireFileIntoBuffer(路径, 出参:缓冲)              SignatureScan.cpp:550
   │  ├─ PortableExecutableImage::ParseAsFileLayout(...)        SignatureScan.cpp:305
   │  │  │  ★ 按磁盘文件的【文件布局】解析：段数据在 rawDataFileOffset 处。
   │  │  ├─ ReadUInt32(指针)                                    SignatureScan.cpp:278
   │  │  └─ ReadUInt16(指针)                                    SignatureScan.cpp:285
   │  │        小端读取。只被 ParseAsFileLayout 用。
   │  └─ PortableExecutableImage::GetExecutableCodeSection(...) SignatureScan.cpp:456
   │        取 .text 的指针 / 大小 / 段相对地址。找不到 .text 就退化为第一个可执行段。
   │        ★ 这里是【提前挡一道】：后面整表解析也会要代码段，
   │          但在这里挡掉能给出更准确的错误（"这个 exe 没有代码段"而不是"N 条全 MISS"）。
   │
   ├─ 5. ResolveEntireSignatureTable(镜像, 出参:结果列表)       SignatureUpdateFlow.cpp:63
   │  │  把 kSignatureTable[] 整张跑一遍。这张表的长度是 offsets_AI.h 里的
   │  │  kSignatureTableCount，编译期就知道。
   │  │
   │  └─ ResolveSignatureDefinition(镜像, 定义)                 SignatureDefinitionResolve.cpp:443
   │     │  ★★ 核心。一条特征码 -> 一个 ResolvedSignature。
   │     │
   │     ├─ ParseSignaturePattern(特征码文本)                   SignatureScan.cpp:29
   │     │  │  把 "E8 ?? ?? ?? ?? 80 38 00 74 16" 解析成
   │     │  │  expectedBytes + comparisonMask。
   │     │  └─ HexDigitValue(字符)                              SignatureScan.cpp:19
   │     │        单个十六进制字符转数值。最下层。
   │     │
   │     ├─ GetExecutableCodeSection(...)                       SignatureScan.cpp:456
   │     │
   │     ├─ ScanMemoryBuffer(代码段, 大小, 段相对地址,
   │     │                  特征码, 出参:命中, 上限=2)          SignatureScan.cpp:137
   │     │  │  ★ 上限传 2：只要唯一，找到第 2 处就已经知道不能用，没必要找完。
   │     │  │  先 memchr 找锚点字节再逐位比，不是傻瓜式逐字节扫描。
   │     │  └─ DoesPatternMatchAt(...)                          SignatureScan.cpp:119
   │     │        比一条候选。static inline，最下层。
   │     │  ★ 命中 0 处 -> NotFound，命中 >1 处 -> NotUnique，两种都直接返回。
   │     │    全局扫、必须唯一 —— 这是整张表的铁律，没有例外。
   │     │
   │     ├─ 有效地址 = 命中处 + 偏移；越界 -> AddressOutOfCodeSection
   │     │
   │     └─ 按【类型】分三路：
   │        │
   │        ├─ ResolveFunctionEntry(...)  Function                SignatureDefinitionResolve.cpp:225
   │        │    有效地址处必须是 E8/E9，目标 = 有效地址 + 5 + rel32。
   │        │    不是 E8/E9 -> NotACallInstruction；边界 != 5 -> BadBoundaryLength。
   │        │
   │        ├─ ResolveHookSpan(...)  Trampoline                   SignatureDefinitionResolve.cpp:256
   │        │    有效地址就是 Hook 起点，结果是【一对】：
   │        │    起点 = 有效地址，终点 = 有效地址 + 边界。
   │        │    边界 <= 0 -> BadBoundaryLength。
   │        │
   │        └─ ResolveDwordValue(...)  Dword                      SignatureDefinitionResolve.cpp:275
   │           │  【有二级码时】先反解出函数入口，再从入口向下扫二级码：
   │           ├─ IsCallOpcode(字节)                             SignatureDefinitionResolve.cpp:67
   │           │    主码偏移处必须是 E8/E9，否则 NotACallInstruction。
   │           ├─ 函数入口 = 有效地址 + 5 + rel32
   │           │    跑到代码段外 -> AddressOutOfCodeSection。
   │           ├─ ParseSignaturePattern(二级特征码)              SignatureScan.cpp:29
   │           ├─ FindFunctionEndRelativeAddress(...)            SignatureDefinitionResolve.cpp:195
   │           │  │  ★★ 划出【函数体】的末尾 = 第一条 ret 之后。
   │           │  │  ret 两种编码都认：C3（-> +1）、C2 iw（-> +3）。
   │           │  │  为什么必须有这一步见「几个坑 8」—— 编译器爱把几个
   │           │  │  一模一样的小 getter 排在一起，不收范围就会静默选到隔壁那个。
   │           │  │  找不到 ret 返回 0，退回到 64KB 上限（并打出来让你看见）。
   │           │  └─ ReadInt32At / ReadUInt32At                  SignatureDefinitionResolve.cpp:53/60
   │           ├─ ScanMemoryBuffer(从函数入口起, 到函数体末尾)    SignatureScan.cpp:137
   │           │    取【第一个】命中。二级码通常不唯一，命中多处是正常的。
   │           │    一处没有 -> SecondaryNotFound。
   │           ├─ ScanMemoryBuffer(函数体【外面】)               SignatureScan.cpp:137
   │           │    纯诊断：数一下函数外还有几处同名，填进
   │           │    secondaryHitCountBeyondFunctionEnd 让你看见（不算错）。
   │           ├─ 值所在处 = 二级命中处 + 二级偏移
   │           │
   │           └─ ClassifyImmediateValueSite(指令字节, 出参:站点) SignatureDefinitionResolve.cpp:106
   │              │  ★ 看【操作码】决定两件事：4 字节在哪儿、要不要减镜像基址。
   │              │  认不出形状 -> NotAReferenceInstruction。
   │              ├─ IsSupportedReferenceOpcode(字节)            SignatureDefinitionResolve.cpp:92
   │              │    88/89/8A/8B/8C/8D/C6/C7/F7/FF，以及 F3/F2 0F 后面的 10/11。
   │              └─ ★ 边界对账：操作码推出来的长度必须 == 表里填的边界，
   │                    对不上 -> BadBoundaryLength。
   │                    这一条免费抓「偏移数错一位」—— 数错了那边就不是一条完整指令。
   │              ★ 减不减 ImageBase 由 ModRM 决定，表里【不填】：
   │                mod=00 rm=101（A1 / 8B 0D）-> disp32 是绝对地址 -> 减
   │                mod=10 且 rm!=4（8B 86）   -> disp32 是结构体偏移 -> 不减
   │
   ├─ 6. PrintResolveTable(结果列表)                            SignatureUpdateFlow.cpp:75
   │     逐条打印：函数名 / 类型 / 状态 / 结果，外加二级扫描的诊断
   │     （"函数体外还有 N 处同名，已按 ret 排除"）。--matrix 也用它。
   │
   └─ WriteOffsetUpdateHeader(输出目录, 文件名, 版本, 来源exe, ImageBase,
   │                          结果列表, 出参:写入结果)          SignatureOutputWriter.cpp:48
      │  写 offset_update.h。【UTF-8 带 BOM】（DLL 编译时用的是 GBK 代码页，
      │  没 BOM 的话中文注释会变乱码 —— 见「几个坑 1」）。
      │  ★ 失败的条目【不写出常量】。少一个常量是编译错误（很吵），
      │    写一个错的值是静默错误（极难查）。要让失败变吵。
      ├─ FormatCurrentLocalTime()                               SignatureOutputWriter.cpp:17
      ├─ GetConstantTrailingComment(类型)                        SignatureOutputWriter.cpp:35
      └─ JoinPath(目录, 文件名)                                   SignatureUpdateFlow.cpp:15
```

### 入口 A 的另外四条路（开发期用）

```
main                                                        SignatureTool.cpp:916
│
├─ --help / -h  ->  PrintUsageInstructions()                 SignatureTool.cpp:55
│
├─ --force      ->  RunOffsetUpdateFlow(forceRescan = true)   SignatureUpdateFlow.cpp:173
│                     同一条流程，但跳过第 3 步的版本比对，无条件重扫。
│                     改了 offsets_AI.h 之后用这个，不然版本没变它不动。
│
├─ --matrix 镜像1 镜像2 ...  ->  RunCrossVersionMatrix(...)    SignatureTool.cpp:263
│  │  【跨版本对比】同一张表在 N 个历史 dump 上各解析一遍，排成矩阵。
│  │  这是独立工具存在的最大理由 —— 不可能同时跑 8 个版本的游戏。
│  │  ★ 用【短状态文本】一格一格填（GetShortResolveStatusText），
│  │    打不下的格子写状态名，不打一长串中文错误。
│  ├─ GetShortResolveStatusText(状态)                         SignatureTool.cpp:238
│  ├─ LoadImageFromFile(路径, 出参:缓冲, 出参:镜像)            SignatureTool.cpp:79
│  │    文件布局解析。工具里所有需要镜像的地方都走它。
│  │    -> ReadEntireFileIntoBuffer / ParseAsFileLayout
│  └─ ResolveEntireSignatureTable(...)                        SignatureUpdateFlow.cpp:63
│        ★ 和更新流程【同一个函数】。矩阵和生成物不可能不一致。
│
├─ --gen 镜像 目标相对地址 [最大长度]  ->  GenerateSignatureForAddress(...)
│  │                                                          SignatureTool.cpp:372
│  │  【取码 · 目标是代码】函数序言 / Hook 点。从目标地址往后滑窗口，
│  │  找"开头是实字节、通配最少、全镜像唯一"的最短码。
│  ├─ LoadImageFromFile(...)
│  ├─ GetExecutableCodeSection(...)                           SignatureScan.cpp:456
│  ├─ StabilizeWindow(镜像, 窗口起点, 可读长度, 窗口起始相对地址,
│  │                 窗口长度, 出参:掩码)                      SignatureTool.cpp:120
│  │  │  把窗口里"看起来会随版本变"的 4 字节通配掉：
│  │  │    (a) 落在镜像内的绝对地址（ImageBase + 相对地址）
│  │  │    (b) 解出来落在镜像内的 rel32 相对偏移
│  │  │  这样生成的码天然跨版本可用。
│  │  │  ★ 可读长度 = 窗口长度 + 3：窗口末端的指令可能正好跨在边界上，
│  │  │    多读 3 字节才能把那条指令看完整（否则会留下半截位移，见「几个坑 6」）。
│  │  └─ PortableExecutableImage::IsAddressInsideImage(相对地址) SignatureScan.cpp:541
│  │     │  该地址是否落在任意已映射段里。
│  │     └─ FindSectionContainingAddress(相对地址)             SignatureScan.cpp:383
│  │           线性遍历段表，返回包含该地址的段。最下层。
│  ├─ BuildPatternText(字节, 掩码, 长度, 出参:实字节数, 出参:通配数)
│  │                                                          SignatureTool.cpp:173
│  │     把 expectedBytes + comparisonMask 拼成 "E8 ?? ?? 8B 0D" 形式的字符串。
│  ├─ ParseSignaturePattern(...)                              SignatureScan.cpp:29
│  │     自检：拼出来的字符串必须能解析回去。
│  ├─ ScanMemoryBuffer(...)                                   SignatureScan.cpp:137
│  │     自检：拼出来的码必须【全镜像唯一】。不唯一就加长窗口再试。
│  └─ IsBetterCandidate(候选A, 候选B)                          SignatureTool.cpp:219
│        排序规则：通配少 > 长度短 > 地址靠前。
│
└─ --gen-global 镜像 目标相对地址 [最大长度]  ->  GenerateGlobalVariableSignature(...)
   │                                                          SignatureTool.cpp:652
   │  【取码 · 目标是全局变量】变量住在 .data/.rdata 里。32 位下全局变量是绝对寻址，
   │  所以先找引用它的指令，再在那条指令上取码，把 imm32 通配掉 ——
   │  命中后读出来的就是本版本的地址。
   ├─ LoadImageFromFile(...)
   ├─ GetExecutableCodeSection(...)                           SignatureScan.cpp:456
   ├─ LooksLikeAbsoluteAddressingInstruction(指令字节, imm32偏移)
   │                                                          SignatureTool.cpp:576
   │     白名单，用来滤掉"字节碰巧相同"的假引用。
   │     ★ 它和 ClassifyImmediateValueSite 是同一个知识的两份实现：
   │       这份是【生成时】用（挑出可信的引用点），
   │       那份是【解析时】用（从引用点读出值）。改一个记得看另一个。
   ├─ StabilizeWindow(...)                                     SignatureTool.cpp:120（同上）
   ├─ BuildPatternText(...)                                    SignatureTool.cpp:173（同上）
   ├─ ParseSignaturePattern(...)                               SignatureScan.cpp:29（同上）
   ├─ ScanMemoryBuffer(...)                                    SignatureScan.cpp:137（同上）
   └─ IsBetterCandidate(...)                                   SignatureTool.cpp:219（同上）
```

### 引擎里剩下的函数（上面没出现的）

```
ScanMemoryBufferForMultiplePatterns(内存, 大小, 起始相对地址,
                                    多条码, 条数, 出参:命中, 每条上限)
                                                              SignatureScan.cpp:191
   一次遍历同时扫多条特征码（按首字节分桶）。比逐条调 ScanMemoryBuffer 快得多。
   ★ 目前无人调用 —— 代码里有实现，但 15 条表还远不到需要它的规模。
     注意它不调用 ScanMemoryBuffer，而是自己内联跑 DoesPatternMatchAt。

GetResolveStatusText(状态)                                     SignatureDefinitionResolve.cpp:10
GetSignatureKindText(类型)                                     SignatureDefinitionResolve.cpp:29
   两个枚举 -> 中文字符串。报告用。

FindResolvedSignature(结果列表, "名字")                         SignatureDefinitionResolve.cpp:534
   在结果列表里按名找一条。★ 目前无人调用
   （要按名取值用 FindResolvedSignature 查列表，比重新解析一遍便宜）。

ParseAsMemoryLayout(数据, 大小)                                SignatureScan.cpp:296
   ★ 目前【没有调用方】—— 工具扫的是磁盘 exe（文件布局）。
     留着是因为它不长，而且「扫一个已经加载的映像」迟早会用到
     （例如想在 DLL 里自检 offset_update.h 的值和进程里的实际位置对不对）。
     真用不上就删掉 —— 连同 isMemoryLayout 和 GetExecutableCodeSection 里
     那两个分支一起删，别只删一半。

ConvertRelativeAddressToPointer(相对地址, 出参:指针)            SignatureScan.cpp:397
ConvertRelativeAddressToFileOffset(相对地址, 出参:文件偏移)      SignatureScan.cpp:430
ConvertFileOffsetToRelativeAddress(文件偏移)                   SignatureScan.cpp:441
   ★ 后两个目前无人调用，留着备用。

LooksLikePortableExecutable()                                  SignatureScan.h:151（内联）
   段表非空或 flat 布局。★ 目前无人调用。

TrimWhitespace(文本)                                           SignatureGameVersion.cpp:59
   去掉两头的空白。读版本号用，最下层。
```

### 层级总览

```
build_tool.bat ──▶ cl 编译 ──▶ signature_tool.exe ──▶ main ──┬── RunOffsetUpdateFlow
                                            （不带参数）     │   ├── ReadGameVersionFromLauncherConfig ── ReadWholeTextFile / ExtractXmlElementText
                                                             │   ├── ReadGameVersionFromGeneratedHeader ── ReadWholeTextFile / ExtractKeyValueOnLine
                                                             │   ├── IsSameGameVersion
                                                             │   ├── LoadGameImage ──┬── ReadEntireFileIntoBuffer
                                                             │   │                    ├── ParseAsFileLayout ── ReadUInt32 / ReadUInt16
                                                             │   │                    └── GetExecutableCodeSection
                                                             │   ├── ResolveEntireSignatureTable ── ResolveSignatureDefinition
                                                             │   │        ├── ParseSignaturePattern ── HexDigitValue
                                                             │   │        ├── ScanMemoryBuffer ── DoesPatternMatchAt
                                                             │   │        └── 三选一：
                                                             │   │            ResolveFunctionEntry
                                                             │   │            ResolveHookSpan
                                                             │   │            ResolveDwordValue ──┬── IsCallOpcode
                                                             │   │                                 ├── FindFunctionEndRelativeAddress ── ReadInt32At
                                                             │   │                                 ├── ScanMemoryBuffer ── DoesPatternMatchAt
                                                             │   │                                 └── ClassifyImmediateValueSite ── IsSupportedReferenceOpcode
                                                             │   ├── PrintResolveTable ── GetResolveStatusText / GetSignatureKindText
                                                             │   └── WriteOffsetUpdateHeader ── FormatCurrentLocalTime / GetConstantTrailingComment
                                                             │
                                                             ├── RunCrossVersionMatrix ──┬── LoadImageFromFile ── ParseAsFileLayout
                                                             │                            └── ResolveEntireSignatureTable（同上）
                                                             │
                                                             ├── GenerateSignatureForAddress ──┬── StabilizeWindow ── IsAddressInsideImage ── FindSectionContainingAddress
                                                             │                                  ├── BuildPatternText
                                                             │                                  └── IsBetterCandidate
                                                             │
                                                             └── GenerateGlobalVariableSignature ──┬── LooksLikeAbsoluteAddressingInstruction
                                                                                                   └──（其余同上）

DLL ──▶ #include "offset_update.h" ──▶ 一堆 constexpr uintptr_t ──▶ base + 常量
        ★ 没有函数调用。这是整个设计的目的。
```

---

## 三、使用方式（两个入口）

| | 独立工具 | DLL |
|---|---|---|
| 入口 | **双击 `build_tool.bat`** | `#include "offset_update.h"` |
| 拿到的东西 | 在磁盘 exe 上算出来的地址 | 编译期常量（工具写好的） |
| 何时用 | 游戏更新后 / 改表后 / 取码时 | 一直（跟着 DLL 编译进去） |
| 扫描开销 | 约 200 毫秒（开发期，无所谓） | **0**（不扫，只读常量） |
| 版本不对怎么办 | 工具报错，你去修表 | 编译期常量是旧的 → 重新跑工具重编 |

### A. 独立工具

**日常只有一步：双击 `build_tool.bat`。**

它做三件事：

```
[1/3] 编译 signature_tool.exe
[2/3] 单独编译一次引擎（这是 DLL 会编译的那两个文件）—— 编译检查，不链接
[3/3] 跑更新流程
```

第 2 步**不是多余的**：第 1 步是把引擎和 `SignatureTool.cpp` 混在一起编译的，
而头文件完全可能在这个顺序下编得过、在那个顺序下编不过。
单独编一遍验证的就是"DLL 那一侧真的编得过"，而不是等注入的时候才发现。

第 3 步的行为：

```
游戏版本 == offset_update.h 里记的版本  ->  "版本没变，跳过扫描"，一个字节都不动
游戏版本 != 那个版本 / 读不到           ->  读 300.exe，整表解析，覆盖 offset_update.h
```

**改了 `offsets_AI.h` 之后要加 `--force`**，否则版本没变它不动：

```bat
signature_tool.exe --force
```

命令行参数（开发期）：

```bat
:: 跨版本对比：同一张表在 N 个历史 dump 上各跑一遍
signature_tool.exe --matrix dump_8_13.exe dump_8_28.exe dump_9_11.exe dump_9_23.exe

:: 取码 —— 目标在【代码】里（函数序言、Hook 点）
signature_tool.exe --gen "300_dump_9_23.exe" 81D260

:: 取码 —— 目标在【数据】里（全局变量）
signature_tool.exe --gen-global "300_dump_9_23.exe" 1A6A4B8

:: 用法
signature_tool.exe --help
```

**`--gen` 和 `--gen-global` 的区别就是目标住哪儿**：

```
目标住在 .text（代码）里        -> --gen          从目标地址取码
目标住在 .data/.rdata（数据）里  -> --gen-global   找引用它的指令，在指令上取码
```

`--gen` 对全局变量会直接报错：`相对地址 0x1A6A4B8 不在 .text (...) 里`。
两条命令都会输出**表项片段**和备选候选，粘进 `offsets_AI.h` 再按下面的说明改。

**镜像用 dump 或磁盘上的 300.exe 都行** —— 两者都是文件布局，同一种字节。
更新流程本身用的是磁盘 exe（不需要 dump，省一步）。

### B. DLL

```cpp
#include "offset_update.h"     // 工具生成的。路径在 SignatureConfig.h 里配

// 模块基址 —— 主模块 = 游戏 exe，不是本 DLL
uintptr_t moduleBaseAddress = (uintptr_t)GetModuleHandleW(nullptr);

// 直接用。常量一律是【相对地址】，用的时候加基址。
void* castSkillFunction = (void*)(moduleBaseAddress + offset_func_castskill);
void* clockVariable     = (void*)(moduleBaseAddress + dword_Clock);
int   clockValue        = *(int*)clockVariable;

// Trampoline 是【成对】的：Start 和 End 都在头文件里
WriteJmp((void*)(moduleBaseAddress + Trampoline_HookSendPack_Start),
         (void*)(moduleBaseAddress + Trampoline_HookSendPack_End),
         Trampoline_YourHook);
```

**没有任何初始化步骤，没有日志回调，没有"解析失败"这一档。**
常量是编译期就定死的，失败只可能发生在编译期（少一个常量 -> 编译错误）。

**这就是选择"只做独立工具"换来的东西**：DLL 里不存在"运行时解析失败"
这个状态，也就不存在"要不要在被破坏的地址上装 Hook"这种危险决策。

**★ 别在 DLL 里自己再加个 0xA00000。** 生成物里记的 `ImageBase=0x00400000`
是**磁盘 exe** 的基址（那是文件里写的 PreferredBase）。
游戏跑起来时基址是**随机**的（实测 0xA00000 那一档），
永远用 `GetModuleHandleW(nullptr)` 拿，永远 `基址 + 相对地址`。

---

## 四、配置添加

### 4.0 表长什么样

`offsets_AI.h` 底部那张 `kSignatureTable[]`，一条就是 7 个字段，
顺序和你在文件里写的那份注释一致：

```cpp
{
    "offset_func_castskill",                  // 1. 函数名（和顶部 constexpr 同名，方便对账）
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
| **修正** | 读出来的 4 字节要加的数 | 默认 0。只有指令里的值本身不是你要的那个数时才用（见 4.3 的例子） |
| **二级** | `{特征码, 二级偏移}` 或 `{nullptr, 0}` | 只有"进 call 取"的 Dword 才填。空 = 直接取 |

**★ 偏移和边界都由工具自己再推一遍，填错了会报错** ——
这一条免费抓「偏移数错一位」。所以别去猜，先填个值跑一遍，它会告诉你对不对。

### 4.1 加一条 Function（要 call 进去的函数）

表里四条 `offset_func_*` 都是这一类。共同点：**特征码最后一个字节是那条 `call`**。

```cpp
{
    // 原 offsets.h: offset_func_getCDRecordObj = 0x81D260
    "offset_func_getCDRecordObj",
    "E8 ?? ?? ?? ?? 80 38 00 74 16",
    SignatureKind::Function, 0, 5, 0, { nullptr, 0 }
},
```

- **特征码**：`E8` + `?? ?? ?? ??`（rel32 必须通配掉，它随版本变）+ 后面几个字节凑唯一。
- **偏移**：`call` 在特征码里的下标。在末尾就是 `长度 - 1`；在开头就是 `0`。
  `offset_func_treeFindPlayerObj` 是 30 字节里**只有最后一个字节是 call** -> `29`。
- **边界**：恒为 `5`（`E8` + 4 字节 rel32）。填别的会报 `BadBoundaryLength`。
- **解出来的值**：`有效地址 + 5 + rel32` —— 即那个函数真正的地址。

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

- **偏移**：`Start` 在特征码里的下标。`0` 表示特征码命中处就是 `Start`。
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
| `mov ax, word ptr [esi+46F38h]` | 结构体成员偏移，**不是地址** | `{nullptr, 0}`，且【修正】可能要用上 |
| `call sub_7C890`，你要的是**它返回的** dword | 值只在运行时存在 | **填二级码**，进那个函数找 |
| `mov ecx, [ebx+0x46F38]`，但那是个死偏移 | 结构体偏移 | **别进表**，写成你自己 header 里的常量 |

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

**这个例子连修正都是 0**，因为读出来的 `0x46F38` 恰好就是你要的结构体偏移。
这就是"减不减 ImageBase 由操作码决定"的好处：

```
66 A3 <imm32>   moffs 绝对寻址  ->  imm32 是绝对地址   ->  自动减 ImageBase
8B 86 <disp32>  [寄存器+偏移]   ->  disp32 是结构体偏移 ->  自动【不】减
```

`66 A3` 是绝对寻址，但这条正好是**上一条指令** `mov ax, word ptr [esi+46F38h]`，
它是 `8B 86` 形式 —— 所以读出来的是纯偏移，不减。**同一个特征码、
两个偏移，出来两个语义完全不同的值**，靠的就是这个自动判断。

#### 情况 B：进 call 取（填二级）

`offsets_AI.h` 里你写的注释"首指令就是 call，进入 call 进行二级扫描"就是这个：

```cpp
{
    // 原 offsets.h: dword_HoverStruct = 0x1A6A76C
    "dword_HoverStruct",
    "E8 ?? ?? ?? ?? 8B C8 E8 ...",              // 主码：末尾/开头那条 call
    SignatureKind::Dword, 0, 5, 0,
    { "A1 ?? ?? ?? ?? 8B 4D F4 64 89 0D 00 00 00 00 59 5E 8B E5 5D C3", 0 }
    //  二级特征码                                         二级偏移
},
```

工具做的事，按顺序：

```
1. 主码命中、唯一
2. 有效地址处必须是 E8/E9  ->  函数入口 = 有效地址 + 5 + rel32
3. 从函数入口向下扫二级码，扫到【第一条 ret 为止】  <- ★ 关键，见「几个坑 8」
4. 取【第一个】命中
5. 值所在处 = 第一个命中 + 二级偏移
6. 看那条指令的操作码，决定 4 字节在哪儿、要不要减 ImageBase
```

- **二级特征码**通常**不是唯一的**（`A1 ?? ?? ?? ?? C3` 就是一句
  `mov eax, [全局]` + `ret`，全镜像到处都是）。靠"限制在函数体内 + 取第一个"定位。
- **二级偏移**一般是 `0`（二级命中处就是那条引用指令）。
- **同一处命中可以出两个元素**，靠偏移区分，互不冲突 ——
  `dword_CurrentProcessed_SkillID` 和 `dword_SkillTable_Slot_SkillIDOffset`
  就是共用同一条特征码（偏移 `0` 和 `-6`）。
  **同一条码只有一个命中处**，所以这不是"一条码挂两个元素"的重复劳动。

#### 修正：指令里的值不是你要的那个数

```cpp
{
    // 原 offsets.h: dword_CurrentProcessed_SkillID = 0x1E2D670
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
可以顺手读出来时才进表（那是白捡的）；否则一律写死在你自己的 header 里，
**不要为了"统一"把它塞进特征码表**。

### 4.4 加完之后

```bat
:: 1. 强制重扫（改了表必须加 --force，否则版本没变它不动）
signature_tool.exe --force

:: 2. 新条目在历史版本上【大面积 OK】才算稳
signature_tool.exe --matrix dump_8_13.exe dump_8_20.exe ... dump_9_23.exe
```

新条目在历史版本里如果大面积 `MISS`，说明码里混进了**随版本变的字节**
（最典型的是半截 `call` 位移，见「几个坑 6」）—— 重新取码，别急着用。

---

## 五、为什么只做独立工具

一开始是"工具 + DLL 运行时扫描"两个入口共用引擎。**现在 DLL 侧那一半删掉了。**
理由：

| | 独立工具 | （原来的）DLL 运行时扫描 |
|---|---|---|
| 一次迭代 | **约 200 毫秒** | 改代码→编译→注入→启游戏→进对局，几分钟 |
| 跨版本验证 | **能同时对比 8 个历史版本** | 做不到（不可能同时跑 8 个版本的游戏） |
| 运行时开销 | 0（不跑） | 扫 21MB 的 .text，每次注入都要 |
| 失败模式 | 报错，你改表 | **运行时才知道挂了**，而且是在别人机器上 |
| 依赖 | 只要 300.exe 在 | 要注入成功、要能读进程内存 |

**关键的一条是"失败模式"**：

运行时扫描失败的时候，你已经进了游戏。这时只有两个坏选择 ——
要么用一个算错的地址装 Hook（把 `CALL` 写进别的指令中间，
表现为"游戏在完全无关的地方崩溃"），要么放弃这次注入。

**而"只做独立工具"让这个状态根本不存在**：常量是编译期常量，
工具没算出某个值，生成物里就没有那个常量，DLL 编译不过。
**失败从"运行时静默"变成了"编译期响亮"。**

代价是：游戏更新后必须先跑一次工具、重新编译 DLL，才能进游戏。
这个代价是值得的 —— 更新本来就不是天天发生的事。

> 引擎（`SignatureScan` + `SignatureDefinitionResolve`）仍然是纯函数、零依赖。
> 留着这个分层不是为了"以后可能还要扫活进程"，而是因为它让
> 「工具里验证过的码，别的地方必然成立」这句话有结构性保证。

---

## 六、字段速查：三种类型

`offsets_AI.h` 里的特征码混了三类东西，分开是维护的前提：

```cpp
有效地址 = 命中处 + 偏移        // 先算这个
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
如果偏移只在 `Trampoline` 下可用，这一条就没法表达。

**为什么"边界"每种类型都要填**：它是**交叉校验**用的。
工具按操作码自己推一遍长度，和表里填的对不上就报 `BadBoundaryLength`。
免费抓「偏移数错一位」—— 数错一位，那边就不是一条完整指令了。

**为什么"减不减 ImageBase"不用填**：完全由 ModRM 决定，填是多余的、且是错的机会。

```
绝对寻址      A1 / 66 A3 / 8B 0D ...   读出来是绝对地址   -> 减
[寄存器+偏移] 8B 86 ...                 读出来是结构体偏移 -> 不减
```

---

## 七、日常用法

### 游戏更新后

```bat
:: 就这样。双击，看输出。
build_tool.bat
```

输出三种情况：

```
版本没变，跳过扫描。E:\repos\300DX9\AI\offset_update.h       -> 什么都没发生
已写出 offset_update.h   常量 15 个（Trampoline 各算两个）     -> 全部成功
★ 有 N 条没解析出来，对应的常量【没有生成】                   -> 去修那几条
```

第三种情况会列出**哪几条**失败了，还会在 `offset_update.h` 顶部留一个
醒目的失败块。**修的永远是 `offsets_AI.h` 里那条特征码，不是生成物。**

### 出了事怎么办

| 现象 | 含义 | 怎么办 |
|---|---|---|
| `NotFound`（一处都没命中） | 码指的地方没了 | 用 `--gen` 重新取码 |
| `NotUnique`（命中多处） | 码太短 | 加长特征码 |
| `NotACallInstruction` | 偏移指到的不是 `E8`/`E9` | 偏移数错了 |
| `NotAReferenceInstruction` | 偏移指到的不是引用指令 | 偏移数错了 |
| `BadBoundaryLength` | 边界和操作码推出来的长度不一致 | 偏移或边界数错了（**多半是偏移**） |
| `SecondaryNotFound` | 函数体里没有二级码 | 二级码要重新取，或函数变了 |
| `AddressOutOfCodeSection` | 算出来的地址跑到 `.text` 外面 | 偏移填错了 |
| `ImageHasNoCodeSection` | 这镜像没有代码段 | 文件传错了 |

**这些状态名都是失败，没有"疑似"这一档。** 因为全局扫 + 必须唯一，
不存在"地址看着像对但差几 KB"的中间状态 —— 要么唯一命中并算出来，要么明确报错。

### 新增一条特征码

已知某个东西在某版本的地址（比如从 `offsets.h` 抄来的）：

```bat
:: 目标在代码里
signature_tool.exe --gen "300_dump_9_23.exe" 81D260

:: 目标是全局变量
signature_tool.exe --gen-global "300_dump_9_23.exe" 1A6A4B8
```

工具会自动：

- 从该相对地址往后滑窗口，找一个**开头是实字节、通配位最少、全镜像唯一**的码
- 把看起来会随版本变的值（镜像内绝对地址、指回镜像的 rel32）**自动通配掉**
- 输出可直接粘贴的表项片段，包含它算出来的偏移
- 再附几个备选候选，并提示"换之前先用 `--matrix` 看看哪个在历史上最稳"

`--gen` 会跳过开头是 `??` 的窗口 —— 这种码毫无意义，命中地址会飘到任何满足
后几字节的地方，偏移也就无从谈起。

---

## 八、几个坑（都踩过）

### 1. 编码

- `.bat` **必须纯 ASCII**。cmd.exe 按 OEM 代码页（中文系统是 GBK）解析 `.bat`，
  UTF-8 中文注释会让脚本静默出错。
- C++ 源码是 UTF-8，编译要加 `/utf-8`，控制台要 `SetConsoleOutputCP(CP_UTF8)`，
  否则中文报告是乱码。
- **生成物 `offset_update.h` 要带 BOM。** 它是 UTF-8 的，
  但 DLL 那边编译时如果不带 `/utf-8`，编译器会按 GBK 去读它 ——
  带 BOM 才能让编译器认出"这是 UTF-8"。生成器里是主动写 BOM 的。
- **`offsets_AI.h` 也必须是 UTF-8。** 它原来是 GBK，配上 `/utf-8` 会报
  30 多条 `warning C4828`。已转成 UTF-8（无 BOM，和工程里其他源文件一致）。
- `.bat` 里的 `rem` 注释**不能含 `%` 和单引号** —— cmd.exe 会在 `rem` 行里
  展开变量、并把引号当成不平衡的。这个坑在写本文档时又踩了一次。

### 2. cmd.exe 的括号

- `%ProgramFiles(x86)%` 展开后含 `)`。写在 `for /f ... in (...)` 里会让
  块提前闭合，命令被截断；写在 `if (...)` 块内的 `echo` 里同理。
  解决：检测 VS 路径时走临时文件；块内输出路径用 `setlocal EnableDelayedExpansion`
  + `!VAR!`（延迟展开在分词之后，`(` `)` 不会被当语法）。
- 另一个变种：`cmd /c "cd /d E:\... & build_tool.bat"` 会报
  "不是内部或外部命令"，但 `cd /d` 本身是成功的。
  **构建脚本里一律用 `%~dp0` 拼全路径调自己人**，别依赖当前目录。

### 3. `vcvars32.bat` 会打印一行找不到 `vswhere.exe`

这是 **Visual Studio 自己的** `VsDevCmd.bat` 干的（它按裸名调用 vswhere，
假设 VS Installer 把它加进了 PATH）。**无害** —— vcvars 仍然正常设置好 `cl`。
实测 MSVC 14.36。不要在构建脚本里为这行加错误处理。

### 4. 文件偏移 vs 相对地址

磁盘文件/ dump 是**文件布局**（段数据在 `rawDataFileOffset` 处），
活进程是**内存布局**（段数据在 `moduleBaseAddress + relativeAddress` 处）。
用错不会崩，只会静默扫错地方。

本文档写作过程中就因为这个坑误判过一次：用 `相对地址 - 代码段相对地址` 当文件偏移
去读 rel32，算出个 `0x10443E7B`，其实是差了 0x400 字节读到了别的数据。
**用错布局不会报错，只会给你一个看起来很像地址的垃圾值。**

### 5. 别让参数名遮蔽同名成员

`ParseAsFileLayout(const uint8_t* imageData, size_t imageSize)` 会和成员
`imageData` / `imageSize` 互相遮蔽，`/W4` 报 C4458。参数名改成
`sourceImageData` / `sourceImageSize`。这类警告值得当成错误处理 ——
遮蔽会导致"我明明赋值了，怎么还是空的"。

### 6. ★ 特征码别在指令中间截断

**踩得最惨的一个。** 取码窗口的最后一两条指令可能正好跨在窗口边界上，
只按窗口长度去读，就会读到一个**残缺的 4 字节操作数**，
算出来的目标地址是垃圾，于是那 4 个字节被当成实字节留在特征码里 ——
可它们其实是随版本变的。

症状：`dword_HoverStruct` 的码末尾曾经是 `E8 48 CC` —— 一条 `call` 的操作码
加上它的 rel32 位移的**头两个字节**。`48 CC` 在 9_23 版是对的，
换个版本 call 点和目标的距离一变，这个码就再也命中不了：

```
名称                       8_13    8_15    8_20    8_28    9_3     9_11    9_18    9_23
dword_HoverStruct          MISS    MISS    MISS    MISS    MISS    MISS    MISS    1A6A76C   ← 修之前
dword_HoverStruct          1A4F64C 1A4F64C 1A5064C 1A5164C 1A4776C 1A6876C 1A6976C 1A6A76C   ← 修之后
```

修法在 `StabilizeWindow`：按 `窗口长度 + 3` 去**读**（这样跨界的那条指令能看完整，
它的 rel32 会被正确识别成"指回镜像的相对偏移"从而通配掉），
但只往窗口**内**的位置写通配。多出来的 3 字节纯粹是为了把边界上的指令看完整。

**自查办法**：新加的条目录完，跑 `--matrix`。如果只有取码那个版本成功、
其它版本全 `MISS`，几乎一定是这个病 —— 重新取码，别去调偏移
（调了也没用，偏移不是给这种情况准备的）。

### 7. `66` 前缀不改地址宽度

`--gen-global` 和解析层各有一张"操作码 → imm32 偏移"的白名单。
里面曾经把 `66 A1 <imm32>` 的偏移写成 **3**，正确的是 **2**：
`66` 只是操作数尺寸前缀（`eax` → `ax`），**不改变 moffs 地址的宽度**，
imm32 紧跟在 `A1` 后面，也就是 +2。

写错之后这个分支只在"命中处往前 3 字节碰巧是 `66 A1`"时才触发 ——
那其实是**上一条指令的尾巴**，等于把指令起点算错了一个字节。

真正让 imm32 顺延到 +3 的是**前缀 + opcode + ModRM** 三条字节的形式
（`66 8B 0D <imm32>`）—— 多一个前缀字节，所以顺延一位。

**同理，别把 `66 8B 86` 当成 6 字节的指令** —— 它是 7 字节
（`66` + `8B` + ModRM + disp32）。表里那条结构体偏移的例子是
**`8B 86`**（6 字节），`66` 是多余的。

**教训**：偏移表要照着**指令编码规则**推，不要照着"看起来像几个字节"数。
写完拿几条真实指令在 x64dbg 里对一遍。

### 8. ★★ 二级码必须限制在函数体内（靠第一条 `ret`）

**这是本工程最隐蔽的一个坑，因为它会静默出错。**

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

```cpp
FindFunctionEndRelativeAddress(代码段, 段起, 段末, 函数入口, 64KB上限)
    C3       -> 返回 offset + 1
    C2 iw    -> 返回 offset + 3
    找不到   -> 返回 0，退回到 64KB 上限
```

这些二级码指向的都是固定的底层小函数（只有一个 `ret`），所以 `ret` 就是函数体末尾。
有了这条边界，上面那个"隔壁的"落在第一条 `ret` 之后，自然被排除。
修完之后三条二级码全部报**命中 1 处**。

**为什么这个划法即使不精确也是安全的**：它只是"划一条边界"，不是完整反汇编。
万一 `C3` 出现在某条指令的立即数里，边界会**画早**。
画早的后果是 `SecondaryNotFound`（很吵，一眼能看见），
而不是选错一个地址（静默，极难查）—— **失败方向是安全的**。

**工具会把这件事打出来**：报告里如果出现
`函数体外还有 N 处同名，已按 ret 排除`，说明这条二级码本身不唯一，
全靠 `ret` 边界兜着。N 越大，越该考虑换一条更长的二级码。

### 9. 游戏版本号不在 exe 里

`300.exe` 的版本资源（`VS_VERSION_INFO`）是**死数据**：
永远是 `1.0.0.1` / 2012 年。拿它做版本判断会永远认为"没更新"。

真版本在这里：

```
D:\Game\JumpGame\300Hero\LauncherCfg\NewUpdateCfg.xml
    <ClientVersion>2026.09.24.2</ClientVersion>
    <InnerVersion>1274</InnerVersion>
```

**两个字段都要相等才算没变**，而且**任一侧读不到就当作"变了"**（去重扫）。
这里的取舍是刻意的：读不到就重扫只花 200 毫秒，
跳过却可能漏掉一次真正的更新 —— 两种错误的代价不对称。

---

## 九、实测数据

### 引擎在 8 个历史版本上的表现（旧表的记录，作为引擎能力的证据保留）

旧表（`SignatureTable.cpp`，已被 `offsets_AI.h` 取代）的 11 条特征码
在 8 个历史 dump 上**全部解析成功**（88 格全绿）：

```
名称                       8_13      8_15      8_20      8_28      9_3       9_11      9_18      9_23
Hook.CastNormalSkillRecvCD AF5481    AF5481    AF56F1    AF5BB1    AFCBB1    AFCEA1    AFCF21    AFDBD1
Hook.SendPack              1DD464    1DD464    1DD534    1DD534    1DF884    1DFA74    1DFA74    1DFAE4
Hook.SwitchSkill           81D299    81D299    81D559    81D679    824259    823EA9    823F09    824B59
Func.GetCDRecordObj        815990    815990    815C50    815D70    81C950    81C5B0    81C600    81D260
Func.CastSkill             AF6710    AF6710    AF6980    AF6E40    AFDE40    AFE130    AFE1B0    AFEE60
Func.CastSummonerSkill     76D960    76D960    76DBF0    76DD80    774770    773DF0    773E40    774460
Global.SkillTable          1A4F398   1A4F398   1A50398   1A51398   1A474B8   1A684B8   1A694B8   1A6A4B8
Global.Clock               1A55AEC   1A55AEC   1A56AEC   1A57AEC   1A4DC38   1A6EC40   1A6FC40   1A70C40
Global.HoverStruct         1A4F64C   1A4F64C   1A5064C   1A5164C   1A4776C   1A6876C   1A6976C   1A6A76C
Func.World2Screen          102D550   102D550   102D810   102DE40   1037770   1038D40   1039720   103A450
Func.TreeFindPlayerObj     601A0     601A0     601D0     601F0     601F0     601F0     601F0     601F0
```

这几列名字和现在 `offsets_AI.h` 里的常量一一对应
（`Hook.SendPack` -> `Trampoline_HookSendPack`，`Global.Clock` -> `dword_Clock`，……），
`9_23` 那一列就是现在表里那些值。**新表的数据用 `--matrix` 自己生成。**

`8_28` 那一列（`AF5BB1` / `81D679` / `1DD534`）与 `offsets.h` 里手写的
"8.28" 注释完全一致 —— 交叉验证通过。

三条全局变量的漂移量完全同步，也是交叉验证：
`Global.SkillTable` 从 `1A4F398` 到 `1A6A4B8`，`Global.HoverStruct` 从
`1A4F64C` 到 `1A6A76C`，**两条都漂了 `0x1B120`** —— 合理，它们在 .data 同一片区域。

`9_23` 单镜像报告：`OK=11`，其余全 0。

**正常版本位移是几 KB 量级**（8.13 → 9.23 六个版本累计才漂 30~40KB）。
所以"地址变了"不等于"码挂了" —— 这正是旧版表里 `STALE` / `SUSPECT` 分档的依据，
也是现在"全局扫 + 必须唯一"能成立的原因：唯一命中就够了，不需要期望地址。

---

## 十、还没做完的

- `offsets_AI.h` 里还有一批常量没进特征码表（`offset_SlotStruct`、
  `dword_MousePos`、`dword_SummonerSkillStruct`、`xRight`、`xBottom` 等）。
  真全局变量照 4.3 情况 A 加；**`Slot_SkillIDOffset` 是结构体偏移不是地址**，
  不该进表，写死在自己 header 里。
- `ParseAsMemoryLayout` 现在没有调用方（见「二、引擎里剩下的函数」）。
  留着当引擎能力，真用不上就删 —— 连同 `isMemoryLayout` 和
  `GetExecutableCodeSection` 里那两个分支一起删，别只删一半。
- `ScanMemoryBufferForMultiplePatterns` 已经实现但还没人调用。
  表大了之后（比如上百条）把它接进 `ResolveEntireSignatureTable` 会明显更快。
- 另有几个函数暂时无人调用（`ConvertRelativeAddressToFileOffset`、
  `ConvertFileOffsetToRelativeAddress`、`FindResolvedSignature`、
  `LooksLikePortableExecutable`），留着备用。
- 失败时只报"哪几条挂了"，还没做"挂的那条往前/往后挪几字节会命中什么"
  这类提示。表大了之后可能值得做。
- 英雄框架（`IHero` 接口 + 注册表 + `GameApi`）还没开始，属于"其他先放着"那一档。

