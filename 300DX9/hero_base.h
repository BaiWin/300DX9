#pragma once

// 英雄请求施放技能的方式。
//enum class SkillRequestMode
//{
//    // 下一帧释放
//    Queued,
//
//    // 条件施放。
//    Condition
//};


// =============================================================================
//  英雄基类
//
//  只有 GetName() 是纯虚的 —— 新英雄最少只要实现这一个函数，
//  其余回调按需覆盖（基类里都是空实现）。
// =============================================================================
class HeroBase
{
public:
    virtual ~HeroBase() {}

    // 菜单上显示的名字。唯一必须实现的。
    virtual const char* GetName() const = 0;

    // 返回true，就放行，false就吞
    virtual bool OnKeyDown(int virtualKey) {}

    // 返回true，就放行，false就吞
    virtual bool OnKeyUp(int virtualKey) {}

    // 游戏切换了某个槽位的技能。
    //   selectSkillID = 这个槽位被换成了哪个技能
    //   slotID        = 哪个槽位（0/1/2）
    virtual void OnSwitchSkill(int selectSkillID, int slotID) {}

    // 收到技能 CD（技能刚进了冷却）。
    //   cooldownMilliseconds = CD 时长（毫秒），0 表示"冷却好了"
    //   slotIndex            = 哪个槽位
    virtual void OnReceiveSkillCooldown(int cooldownMilliseconds, int slotIndex) {}

    // -------------------------------------------------------------------------
    //  每帧
    // -------------------------------------------------------------------------

    // 每帧调一次（在游戏的 Present 里）。
    // 英雄自己的 CD 倒计时写在这里，参考 cooldown.cpp 里 D/F 的写法。
    virtual void OnFrameUpdate(double deltaMilliseconds) {}

    // -------------------------------------------------------------------------
    //  界面
    // -------------------------------------------------------------------------

    // 画在菜单窗口【内部】
    virtual void OnDrawMenu() {}

    // 画在屏幕叠加层（游戏画面之上）
    virtual void OnDrawOverlay() {}

    // -------------------------------------------------------------------------
    //  清理
    // -------------------------------------------------------------------------

    // 被激活、被取消激活时各调一次。
    // 把残留状态清干净：按键等待位、CD 表、缓存的技能 id。
    virtual void ResetState() {}
private:
    int keyflag = 0;
};


extern HeroBase* g_activeHero;
