#pragma once
#include "hero_base.h"

// =============================================================================
//  ★★★ 新英雄模板 ★★★
// =============================================================================
class HeroTemplate : public HeroBase
{
public:
    const char* GetName() const override;

    bool OnKeyDown(int virtualKey) override;
    bool OnKeyUp(int virtualKey) override;
    void OnSwitchSkill(int selectSkillID, int slotID) override;
    void OnReceiveSkillCooldown(int cooldownMilliseconds, int slotIndex) override;
    void OnFrameUpdate(double deltaMilliseconds) override;

    void OnDrawMenu() override;
    void OnDrawOverlay() override;

    void ResetState() override;
};


// 全局唯一实例。注册表里登记的就是它的地址。
extern HeroTemplate g_heroTemplate;
