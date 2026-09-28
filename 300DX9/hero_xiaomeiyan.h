#pragma once
#include "300.h"



#pragma once
#include "hero_base.h"

// =============================================================================
//  ★★★ 新英雄模板 ★★★
// =============================================================================
class HeroXiaoMeiYan : public HeroBase
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
private:
	bool HasJiQiang();

	bool HasPaoDan();
};


// 全局唯一实例。注册表里登记的就是它的地址。
extern HeroXiaoMeiYan g_hero_xiaomeiyan;

extern double xiaomeiyan_WSkill_JiQiang_CD;
extern double xiaomeiyan_WSkill_Start;
extern WORD xiaomeiyan_WSkill_ID;



// 设计
// Q技能炮弹
// W技能机枪
// E技能机枪

// w skill  (int)
// 3710 机枪
// 3985 炮弹
// 3695 炮弹？
// 3709 地雷
// 3708 闪光弹

