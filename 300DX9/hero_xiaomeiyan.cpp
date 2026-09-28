#include "hero_xiaomeiyan.h"
#include "300.h"
#include "imgui.h"

HeroXiaoMeiYan g_hero_xiaomeiyan;

const char* HeroXiaoMeiYan::GetName() const
{
    return "晓美焰";
}

bool HeroXiaoMeiYan::OnKeyDown(int virtualKey)
{
    if (virtualKey == 'Q')
    {
        if (HasPaoDan())
        {
            castSkillTeskQueue.push({ 1 });
        }
        else // 设置触发，等待技能切换，即swtich
        {
            key_flags |= 1 << 0; // QW
        }
    }
    else if (virtualKey == 'W')
    {
        if (HasPaoDan())
        {
            castSkillTeskQueue.push({ 1 });
            return 0;
        }

    }
    else if (virtualKey == 'E')
    {
        if (HasPaoDan())
        {
            castSkillTeskQueue.push({ 1 });
            return 0;

            if (xiaomeiyan_WSkill_JiQiang_CD <= 0.0)  // 机枪不在cd
            {
                key_flags |= 1 << 2;  //设置触发，之后放W，等待技能切换，即swtich
                return 0;  // 不放E
            }
            else // 机枪在cd
            {
                key_flags |= 1 << 2; //设置触发，等待技能切换，即switch
                //放E
            }
        }
        else //没有炮弹
        {
            if (xiaomeiyan_WSkill_JiQiang_CD <= 0.0)  // 机枪不在cd
            {
                //CastNormalSkillDirect(1); // 直接放机枪
                return 0;  // 不放E
            }
            else // 机枪在cd
            {
                key_flags |= 1 << 2; //设置触发，等待技能cd，即recvCD
                //放E
            }
        }
    }
    return 1;
}


bool HeroXiaoMeiYan::OnKeyUp(int virtualKey)
{
    if (virtualKey == 'Q')
    {
        key_flags &= ~(1 << 0);
    }
    else if (virtualKey == 'E')
    {
        key_flags &= ~(1 << 2);
    }
    
    return 1;
}

void HeroXiaoMeiYan::OnSwitchSkill(int selectSkillID, int slotID)
{
    int select_id = selectSkillID;
    int slot_id = slotID;

    if (slot_id == 1)
    {
        xiaomeiyan_WSkill_ID = select_id; // 每次切技能都更新一下W的id
    }
    if (key_flags & (1 << 0) && xiaomeiyan_WSkill_ID != 3710)  // QW 连招 // 按下Q一定会切技能 // 并且一定是炮弹
    {
        key_flags &= ~(1 << 0);

        castSkillTeskQueue.push({ 1 });
    }
    if (key_flags & (1 << 2) && xiaomeiyan_WSkill_ID == 3710)  // E
    {
        key_flags &= ~(1 << 2);

        castSkillTeskQueue.push({ 1 });
    }
}

void HeroXiaoMeiYan::OnReceiveSkillCooldown(int cooldownMilliseconds, int slotIndex)
{
    int skill_cd = cooldownMilliseconds;
    int slot_id = slotIndex;

    if (slot_id == 1)     // 仅释放E会触发这里
    {
        xiaomeiyan_WSkill_JiQiang_CD = static_cast<double>(skill_CD);
        // E和Q都会触发recv cd
        WORD process_SkillID = *reinterpret_cast<WORD*>(base + dword_CurrentProcessed_SkillID + 0x2);
        xiaomeiyan_WSkill_ID = process_SkillID;

        if (key_flags & (1 << 2) && process_SkillID == 3710 && skill_CD == 0) //是否按下E 且收到的是机枪
        {
            key_flags &= ~(1 << 2);

            castSkillTeskQueue.push({ 1 });
        }
    }
}

void HeroXiaoMeiYan::OnFrameUpdate(double deltaMilliseconds)
{
    if (xiaomeiyan_WSkill_JiQiang_CD > 0.0)
    {
        xiaomeiyan_WSkill_Start += deltaMilliseconds;
        if (xiaomeiyan_WSkill_Start > xiaomeiyan_WSkill_JiQiang_CD)
        {
            xiaomeiyan_WSkill_JiQiang_CD = 0.0;
            xiaomeiyan_WSkill_Start = 0.0;
        }
    }
}


// =============================================================================
//  菜单里自己那部分 UI，画在菜单窗口【内部】。
// =============================================================================
void HeroXiaoMeiYan::OnDrawMenu()
{
    ImGui::Text(" W %.2f", (xiaomeiyan_WSkill_JiQiang_CD - xiaomeiyan_WSkill_Start) / 1000.0);
    ImGui::Text(" ID %d", xiaomeiyan_WSkill_ID);
}


// =============================================================================
//  屏幕叠加层里自己那部分 UI，画在游戏画面之上。
//
//  参考 menu.cpp 里的 DrawDFSummonerSkill() 是怎么画的。
// =============================================================================
void HeroXiaoMeiYan::OnDrawOverlay()
{
    // 亮灰色（比 #808080 更亮，空心框用这个更醒目）
    ImVec4 colors[4] =
    {
        ImVec4(0.75f, 0.75f, 0.75f, 1.0f), // 亮灰色
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
        ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
        ImVec4(0.0f, 0.0f, 1.0f, 1.0f)
    };

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // 屏幕尺寸
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screen_size = io.DisplaySize;

    // 可调节参数
    float box_size = 52.5f; // 正方形边长
    float thickness = 4.8f;   // 框的粗细
    float margin_bottom = 26.0f;// 距离屏幕底端的间距
    float margin_right = -5.7f;
    float rounding = 3.0f;
    const float gap = 1.0f;  // 两个框之间 20px 空隙


    // 正方形中心 X = 屏幕水平居中
    float center_x = screen_size.x * 0.5f;

    // 正方形的 top-left 和 bottom-right
    ImVec2 p_min(center_x - box_size * 0.5f + margin_right,
        screen_size.y - margin_bottom - box_size);
    ImVec2 p_max(center_x + box_size * 0.5f + margin_right,
        screen_size.y - margin_bottom);

    ImVec2 p_min_QSkill(p_min.x - box_size - gap, p_min.y);
    ImVec2 p_max_QSkill(p_max.x - box_size - gap, p_max.y);

    ImVec2 p_min_ESkill(p_min.x + box_size + gap, p_min.y);
    ImVec2 p_max_ESkill(p_max.x + box_size + gap, p_max.y);

    // 画空心框
    if (xiaomeiyan_WSkill_ID == 3710) //机枪
    {
        if (xiaomeiyan_WSkill_JiQiang_CD <= 0) //机枪已经转好
        {
            dl->AddRect(p_min, p_max, ImGui::GetColorU32(colors[2]), rounding, 0, thickness);
        }
        else
        {
            dl->AddRect(p_min_ESkill, p_max_ESkill, ImGui::GetColorU32(colors[2]), rounding, 0, thickness);
        }
    }
    else // 炮弹
    {
        dl->AddRect(p_min_QSkill, p_max_QSkill, ImGui::GetColorU32(colors[1]), rounding, 0, thickness);
    }
}


// =============================================================================
//  被激活 / 被取消激活时各调一次。把残留状态清干净。
// =============================================================================
void HeroXiaoMeiYan::ResetState()
{
    // template_keyWaitingFlags = 0;
    // 所有 CD 清零 ...
}