// =============================================================================
//  ★ 这个文件已经停用。
//
//  这里是之前的「函数指针结构体」版英雄分叉方案：
//      HeroFuncs 结构体 + g_allHeroes[] 数组 + g_activeHero 指针。
//
//  已经被 hero_base.h / hero_registry.h 的 class 版取代了 ——
//  用虚函数而不是给每个回调都填一遍函数指针。
//
//  整个文件用 #if 0 关掉，留着备查。要恢复就把 #if 0 改成 #if 1。
//  （用 #if 0 而不是块注释，因为这个文件里本来就有块注释，套起来会提前闭合。）
// =============================================================================
#if 0

#pragma once
#include "300.h"

struct HeroFuncs
{
    const char* name;                                   // 菜单上显示的名字

    bool (*OnKeyDown)(int virtualKey);                  // 返回 true = 吞掉这次按键
    void (*OnSwitchSkill)(int selectSkillID, int slotID);
    void (*OnReceiveSkillCooldown)(int cooldownMilliseconds, int slotIndex);
    void (*OnFrameUpdate)(double deltaMilliseconds);
    void (*ResetState)();
};

extern const HeroFuncs* g_activeHero;

#endif // 0
