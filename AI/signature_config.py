# -*- coding: utf-8 -*-
"""
signature_config.py —— 全部路径只在这一个文件里改。

游戏那两条是绝对路径，装在哪就填哪。
工具侧那四条默认跟着这个脚本走（脚本在哪个目录，就在哪个目录读写），
所以整个 AI 文件夹搬到别的地方也不用改。
"""

import os

# =============================================================================
#  游戏侧 —— 绝对路径，必须按你自己的安装位置填
# =============================================================================

# 版本号从这里读。
# ★ 注意【不是】从 300.exe 的版本资源读 —— 那里面是死数据，
#   永远是 1.0.0.1 / 2012 年，拿它判断版本会永远认为没更新。
GAME_VERSION_FILE_PATH = r"D:\Game\JumpGame\300Hero\LauncherCfg\NewUpdateCfg.xml"

# 要扫描的游戏主程序。
# ★ 磁盘上的 exe 就够，不需要 dump —— 两者都是文件布局，字节是同一种。
GAME_EXECUTABLE_PATH = r"D:\Game\JumpGame\300Hero\300.exe"

# =============================================================================
#  工具侧 —— 默认跟着脚本走，一般不用改
# =============================================================================

_THIS_DIRECTORY = os.path.dirname(os.path.abspath(__file__))

# 特征码表。这是全工程唯一需要手工维护的文件，
# 一直放在 AI 目录里不动（工具只读它，从不写它）。
SIGNATURE_DEFINITION_FILE_PATH = os.path.join(_THIS_DIRECTORY, "offsets_AI.h")

# 生成物写到哪
OUTPUT_DIRECTORY = _THIS_DIRECTORY
OUTPUT_FILE_NAME = "offset_update.h"
