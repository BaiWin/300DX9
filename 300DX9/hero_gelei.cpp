#include "hero_gelei.h"
#include "300.h"
#include "imgui.h"

HeroGeLei g_hero_gelei;


const char* HeroGeLei::GetName() const
{
    return "格蕾";
}

bool HeroGeLei::OnKeyDown(int virtualKey)
{
    if (virtualKey == 'Q')
    {
        if (key_flags & (1 << 0))
            return 0;
    }
    else if (virtualKey == 'W')
    {
        if (key_flags & (1 << 1))
            return 0;
    }
    else if (virtualKey == 'E')
    {
        if (key_flags & (1 << 2))
            return 0;
    }
    else if (virtualKey == 'T')
    {
        bool spaceDown =
            (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

        if (spaceDown) // space + T
        {
            key_flags |= 1 << 4;
        }
    }
    return 1;
}


bool HeroGeLei::OnKeyUp(int virtualKey)
{
    if (virtualKey == 'T')
    {
        key_flags &= ~(1 << 4);
    }
    return 1;
}

void HeroGeLei::OnSwitchSkill(int selectSkillID, int slotID)
{
    int select_id = selectSkillID;
    int slot_id = slotID;
    if (key_flags & (1 << 4))
    {
        return;
    }

    if (select_id && slot_id < 3)
    {
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                GeLeiSkill& skill = gelei_skills[i][j];

                if (skill.skillid == select_id)
                {
                    key_flags &= ~(1 << slot_id); //收到包，代表已经选择了，这个时候解锁一下键盘，以防bug
                    return;               //这个函数的调用处就是服务器下发对槽位的设置，切技能会设置槽位，选择技能也会设置槽位。
                }                         //而我只需要切技能时候的id，所以屏蔽调选择技能时的id

                if (skill.selectid == select_id)
                {
                    /*char buffer[128];

                    sprintf_s(
                        buffer,
                        sizeof(buffer),
                        "seletcID = (%u)\n",
                        select_id
                    );*/

                    //OutputDebugStringA(buffer);

                    // 选择要切换的技能
                    if (skill.cd == 0 && slot_id < 3)  // 技能不在cd，就准备调用切技能，并且启用键盘
                    {
                        key_flags &= ~(1 << slot_id);
                        castSkillTeskQueue.push({ slot_id }); // 下一帧统一进行，因为hook的方法里是解包赋值的方法，后面还有UI处理
                    }
                    else if (skill.cd > 0 && slot_id < 3) // 根据我自己保存的cd列表，如果当前切的技能在cd，就禁用键盘
                    {
                        key_flags |= 1 << slot_id;
                    }

                    return;
                }
            }
        }
    }
}

void HeroGeLei::OnReceiveSkillCooldown(int cooldownMilliseconds, int slotIndex)
{
    int skill_cd = cooldownMilliseconds;
    int slot_id = slotIndex;

    WORD process_SkillID = *reinterpret_cast<WORD*>(base + dword_CurrentProcessed_SkillID + 0x2); // 当前要处理的技能id,也是其中一个slot的id  // 0x1E12B28  
    //和上面一行一样的，来源于 .text:00816704    mov     word ptr dword_1E12B28+2, ax

    if (process_SkillID)
    {
        bool isSlot1_InCD, isSlot2_InCD, isSlot3_InCD;

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                GeLeiSkill& skill = gelei_skills[i][j];

                if (skill.skillid == process_SkillID)
                {
                    skill.cd = static_cast<double>(skill_cd);
                    //return;
                }

                if (skill.skillid == g_slotsPanelInfo.skillid_Q && skill.cd > 0)
                {
                    isSlot1_InCD = true;
                }
                if (skill.skillid == g_slotsPanelInfo.skillid_W && skill.cd > 0)
                {
                    isSlot2_InCD = true;
                }
                if (skill.skillid == g_slotsPanelInfo.skillid_E && skill.cd > 0)
                {
                    isSlot3_InCD = true;
                }
            }
        }

        if (isSlot1_InCD && isSlot1_InCD && isSlot1_InCD)
        {
            castSkillTeskQueue.push({ 4 });
        }
    }
}

void HeroGeLei::OnFrameUpdate(double deltaMilliseconds)
{
    for (auto& row : gelei_skills)
    {
        for (auto& skill : row)
        {
            if (skill.cd > 0.0)
            {
                skill.start += deltaMilliseconds;

                if (skill.start > skill.cd)
                {
                    skill.cd = 0.0;
                    skill.start = 0.0;
                }

            }
        }
    }
}


// =============================================================================
//  菜单里自己那部分 UI，画在菜单窗口【内部】。
// =============================================================================
void HeroGeLei::OnDrawMenu()
{
    ImVec4 colors[3] =
    {
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
        ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
        ImVec4(0.0f, 0.0f, 1.0f, 1.0f)
    };

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            GeLeiSkill& skill = gelei_skills[i][j];

            // 1. 获取当前布局光标位置
            ImVec2 cursor_pos = ImGui::GetCursorScreenPos();

            // 2. 计算圆的位置（垂直居中于文字）
            // 获取当前字体的大致高度
            float font_height = ImGui::GetFontSize();  // 当前字体大小
            float radius = font_height * 0.20f;        // 圆半径 = 字体高度的 1/5

            // 圆心的 Y 坐标 = 光标 Y + 字体高度的一半（让圆垂直居中）
            ImVec2 center = ImVec2(
                cursor_pos.x + radius + 2.0f,           // X：留一点左边距
                cursor_pos.y + font_height * 0.5f       // Y：垂直居中
            );

            // 3. 绘制半透明实心圆
            ImGui::GetWindowDrawList()->AddCircleFilled(
                center,
                radius,
                ImGui::ColorConvertFloat4ToU32(colors[i]),  // 已包含透明度 0.75
                16
            );

            // 4. 手动调整布局光标位置（跳过圆 + 间距）
            float circle_width = radius * 2.0f + 8.0f;
            ImGui::SetCursorScreenPos(ImVec2(cursor_pos.x + circle_width, cursor_pos.y - 1.0f));

            // 5. 绘制文字
            ImGui::Text(" %.2f", (skill.cd - skill.start) / 1000.0);
        }
        if (i < 2)
        {
            ImGui::Spacing();     // 添加小空行
            ImGui::Spacing();     // 可以加两个 Spacing 让空行更大
        }
    }
}


// =============================================================================
//  屏幕叠加层里自己那部分 UI，画在游戏画面之上。
//
//  参考 menu.cpp 里的 DrawDFSummonerSkill() 是怎么画的。
// =============================================================================
void HeroGeLei::OnDrawOverlay()
{
    // 例子：
    // ImDrawList* drawList = ImGui::GetForegroundDrawList();
    // drawList->AddCircle(...);
}


// =============================================================================
//  被激活 / 被取消激活时各调一次。把残留状态清干净。
// =============================================================================
void HeroGeLei::ResetState()
{
    // template_keyWaitingFlags = 0;
    // 所有 CD 清零 ...
}



