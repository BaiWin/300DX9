#include "hero_template.h"
#include "300.h"
#include "imgui.h"


// =============================================================================
//  这个英雄自己的状态，全部写成这个文件里的 static 变量。
//
//  ★ 好处：别的文件碰不到它们了。
//    原来 gelei_skills / key_flags / xiaomeiyan_WSkill_ID 都是全局的，
//    任何文件都能改，出了问题很难查。
//
//  例子：
//      static int   template_keyWaitingFlags = 0;   // 哪个键在等技能切过去
//      static Skill template_skills[3][3] = { ... };
// =============================================================================


HeroTemplate g_heroTemplate;


const char* HeroTemplate::GetName() const
{
    return "模板英雄";
}


// =============================================================================
//  按键按下
//
//  virtualKey 是 Windows 虚拟键码，直接和字符比较即可：
//      virtualKey == 'Q' / 'W' / 'E' / 'R' / 'T' / 'D' / 'F' / 'G'
//      virtualKey == VK_SPACE
//
//  两个动作：
//      request.RequestCastSkill(槽位)   放技能
//      request.SwallowKey()             这次按键不给游戏（原来 WndProc 里的 return 0）
// =============================================================================
bool HeroTemplate::OnKeyDown(int virtualKey)
{
    // 例子 1：按 Q 放 1 号位的技能，并且不让游戏收到这次 Q
    //
    // if (virtualKey == 'Q')
    // {
    //     request.RequestCastSkill(1);
    //     request.SwallowKey();
    //     return;
    // }

    // 例子 2：某个键先记下来，等技能切过去再放（原来是 key_flags 那套）
    //
    // if (virtualKey == 'W')
    // {
    //     template_keyWaitingFlags |= 1 << 1;
    //     request.SwallowKey();
    // }
    return 1;
}


bool HeroTemplate::OnKeyUp(int virtualKey)
{
    // 例子：松开某个键就清掉等待位
    //
    // if (virtualKey == 'W')
    //     template_keyWaitingFlags &= ~(1 << 1);
    return 1;
}


// =============================================================================
//  游戏切换了某个槽位的技能
//
//  selectSkillID = 这个槽位被换成了哪个技能
//  slotID        = 哪个槽位（0/1/2）
//
//  ★ 这里【只能】用 request.RequestCastSkill()，它是排队模式。
//    这个回调是从游戏函数内部 call 进来的，当场施法会重入游戏的技能系统。
// =============================================================================
void HeroTemplate::OnSwitchSkill(int selectSkillID, int slotID)
{
    // 例子：把游戏换给我们的技能 id 记下来
    //
    // if (slotID == 1)
    //     template_skillIDInSlotW = selectSkillID;

    // 例子：新换上的技能不在 CD 就立刻放，在 CD 就挂个等待位
    //
    // if (槽位里这个技能不在 CD)
    //     request.RequestCastSkill(slotID);
    // else
    //     template_keyWaitingFlags |= 1 << slotID;
}


// =============================================================================
//  收到技能 CD（技能刚进了冷却）
//
//  cooldownMilliseconds = CD 时长（毫秒），0 表示"冷却好了"
//  slotIndex            = 哪个槽位
//
//  ★ 同样是排队模式，理由同上。
//
//  常读的两个游戏状态：
//
//    // 当前正在处理的技能 id（也可能就是下一个要放的）
//    WORD process_SkillID = *reinterpret_cast<WORD*>(base + dword_CurrentProcessed_SkillID + 0x2);
//
//    // 三个槽位里分别是什么技能
//    uintptr_t skillTable = *reinterpret_cast<uintptr_t*>(base + dword_SkillTable);
//    WORD slot1_SkillID = *reinterpret_cast<WORD*>(skillTable + dword_SkillTable_Slot_SkillIDOffset + 0x0);
//    WORD slot2_SkillID = *reinterpret_cast<WORD*>(skillTable + dword_SkillTable_Slot_SkillIDOffset + 0x4);
//    WORD slot3_SkillID = *reinterpret_cast<WORD*>(skillTable + dword_SkillTable_Slot_SkillIDOffset + 0x8);
// =============================================================================
void HeroTemplate::OnReceiveSkillCooldown(int cooldownMilliseconds, int slotIndex)
{
    // 例子：把这次的 CD 记到自己的表里
    //
    // if (process_SkillID == 某个技能id)
    //     template_someCooldown = static_cast<double>(cooldownMilliseconds);
}


// =============================================================================
//  每帧
//
//  deltaMilliseconds = 上一帧到现在过了多少毫秒。
//
//  自己的 CD 倒计时写在这里，照着 cooldown.cpp 里 UpdateLocalCooldowns()
//  对召唤师技能 D/F 的写法抄一份就行。
//
//  这个回调不在游戏逻辑函数内部，所以 request 是立刻模式，可以当场施法。
// =============================================================================
void HeroTemplate::OnFrameUpdate(double deltaMilliseconds)
{
    // 例子：CD 倒计时
    //
    // if (template_someCooldown > 0.0)
    // {
    //     template_someCooldownStart += deltaMilliseconds;
    //
    //     if (template_someCooldownStart > template_someCooldown)
    //     {
    //         template_someCooldown = 0.0;
    //         template_someCooldownStart = 0.0;
    //     }
    // }
}


// =============================================================================
//  菜单里自己那部分 UI，画在菜单窗口【内部】。
// =============================================================================
void HeroTemplate::OnDrawMenu()
{
    // 例子：
    // ImGui::Text("技能 CD %.2f", template_someCooldown / 1000.0);
}


// =============================================================================
//  屏幕叠加层里自己那部分 UI，画在游戏画面之上。
//
//  参考 menu.cpp 里的 DrawDFSummonerSkill() 是怎么画的。
// =============================================================================
void HeroTemplate::OnDrawOverlay()
{
    // 例子：
    // ImDrawList* drawList = ImGui::GetForegroundDrawList();
    // drawList->AddCircle(...);
}


// =============================================================================
//  被激活 / 被取消激活时各调一次。把残留状态清干净。
// =============================================================================
void HeroTemplate::ResetState()
{
    // template_keyWaitingFlags = 0;
    // 所有 CD 清零 ...
}
