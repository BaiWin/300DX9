#include "menu.h"
#include "300.h"

void MenuStyle()
{
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;

    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;

    // 窗口背景透明度
    style.Colors[ImGuiCol_WindowBg].w = 0.45f;

    // 输入框、按钮等
    style.Colors[ImGuiCol_FrameBg].w = 1.00f;
    style.Colors[ImGuiCol_PopupBg].w = 1.00f;
}


void DrawCenterCircle()
{
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 center(
        io.DisplaySize.x * 0.5f,
        io.DisplaySize.y * 0.5f
    );

    ImDrawList* drawList =
        ImGui::GetBackgroundDrawList();

    drawList->AddCircle(
        center,
        50.0f,
        IM_COL32(255, 255, 255, 180),
        64,
        2.0f
    );
}

void DrawSkillCDShapeInfo()
{
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            Skill& skill = gelei_skills[i][j];

            ImGui::Text("Skill_ID: %u, Skill_CD: %.1f", skill.skillid, skill.cd - skill.start);
        }
    }
}


void DrawDFSummonerSkill()
{
    static ImFont* bold_font = nullptr;
    if (!bold_font)
    {
        ImGuiIO& io = ImGui::GetIO();
        bold_font = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\Arial.ttf",
            18.0f
        );
    }

    if (bold_font)
    {
        ImGui::PushFont(bold_font);
    }

    // 用前景 drawlist，保证画在所有窗口上面；想被窗口挡住就用 Background
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    char buf1[64];
    char buf2[64];
    snprintf(buf1, sizeof(buf1), "%.1f",
        (g_summonnerSkillSlotInfo.slot1_Hiden_D_CD - g_summonnerSkillSlotInfo.slot1_Hiden_D_Start) / 1000.0);
    snprintf(buf2, sizeof(buf2), "%.1f",
        (g_summonnerSkillSlotInfo.slot2_Hiden_F_CD - g_summonnerSkillSlotInfo.slot2_Hiden_F_Start) / 1000.0);

    // ---- 样式参数 ----
    const float gap = 7.0f;  // 两个框之间 20px 空隙
    const float pad_x = 4.0f;   // 框内左右内边距
    const float pad_y = 2.0f;   // 框内上下内边距

    const ImU32 bg_Green = IM_COL32(0, 255, 0, 50);      // 半透明绿色
    const ImU32 bg_Red = IM_COL32(255, 0, 0, 50);      // 半透明绿色
    const ImU32 text_color = IM_COL32(255, 255, 255, 255);  // 白色文字

    // ---- 计算每个文字尺寸 ----
    ImVec2 size1 = ImGui::CalcTextSize(buf1);
    ImVec2 size2 = ImGui::CalcTextSize(buf2);

    // 每个背景框的尺寸
    ImVec2 box1_size = ImVec2(size1.x + pad_x * 2.0f, size1.y + pad_y * 2.0f);
    ImVec2 box2_size = ImVec2(size2.x + pad_x * 2.0f, size2.y + pad_y * 2.0f);

    // 两个框 + 中间 gap 的总宽度
    float total_width = box1_size.x + gap + box2_size.x;

    // 屏幕尺寸
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screen_size = io.DisplaySize;

    // 整体起始 X（水平居中），Y（贴屏幕底部）
    float margin_bottom = 60.0f + 20.0f;
    float margin_right = 170.0f;
    float start_x = (screen_size.x - total_width) * 0.5f + margin_right;
    float start_y = screen_size.y - box1_size.y - margin_bottom;

    // ---- 绘制第一个框 ----
    ImVec2 box1_min = ImVec2(start_x, start_y);
    ImVec2 box1_max = ImVec2(start_x + box1_size.x, start_y + box1_size.y);
    dl->AddRectFilled(box1_min, box1_max,
        g_summonnerSkillSlotInfo.slot1_Hiden_D_CD == 0 ? bg_Green : bg_Red,
        4.0f); // 4.0f 是圆角，不需要可改 0.0f

    ImVec2 text1_pos = ImVec2(box1_min.x + pad_x, box1_min.y + pad_y);
    dl->AddText(text1_pos, text_color, buf1);

    // ---- 绘制第二个框 ----
    float box2_x = start_x + box1_size.x + gap;
    ImVec2 box2_min = ImVec2(box2_x, start_y);
    ImVec2 box2_max = ImVec2(box2_x + box2_size.x, start_y + box2_size.y);
    dl->AddRectFilled(box2_min, box2_max,
        g_summonnerSkillSlotInfo.slot2_Hiden_F_CD == 0 ? bg_Green : bg_Red,
        4.0f);

    ImVec2 text2_pos = ImVec2(box2_min.x + pad_x, box2_min.y + pad_y);
    dl->AddText(text2_pos, text_color, buf2);

    if (bold_font)
    {
        ImGui::PopFont();
    }
}

void DrawMenu2()
{
    ImVec4 colors[3] =
    {
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
        ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
        ImVec4(0.0f, 0.0f, 1.0f, 1.0f)
    };

    //ImGui::Text("PresentFPS: %d", PresentFPS);
    ImGui::Begin(
        "##Overlay",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize
    );

    if (ImGui::Checkbox("gelei", &b_gelei))
    {
        if (b_gelei && b_xiaomeiyan) b_xiaomeiyan = false;
        ClearKeyFlag();
    }

    if (ImGui::Checkbox("xiaomeiyan", &b_xiaomeiyan))
    {
        if (b_xiaomeiyan && b_gelei) b_gelei = false;
        ClearKeyFlag();
    }

    if (b_gelei)
    {
        b_xiaomeiyan = false;
    }

    if (b_xiaomeiyan)
    {
        b_gelei = false;
    }

    static ImFont* bold_font = nullptr;

    if (!bold_font)
    {
        ImGuiIO& io = ImGui::GetIO();
        bold_font = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\Arial.ttf",
            18.0f
        );
    }

    if (bold_font)
    {
        ImGui::PushFont(bold_font);
    }

    if(b_gelei)
    {
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                Skill& skill = gelei_skills[i][j];

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

        ImGui::Text(" D %.2f", (g_summonnerSkillSlotInfo.slot1_Hiden_D_CD - g_summonnerSkillSlotInfo.slot1_Hiden_D_Start) / 1000.0);
        ImGui::Text(" F %.2f", (g_summonnerSkillSlotInfo.slot2_Hiden_F_CD - g_summonnerSkillSlotInfo.slot2_Hiden_F_Start) / 1000.0);
    }
    else if(b_xiaomeiyan)
    {
        ImGui::Text(" W %.2f", (xiaomeiyan_WSkill_JiQiang_CD - xiaomeiyan_WSkill_Start) / 1000.0);
        ImGui::Text(" ID %d", xiaomeiyan_WSkill_ID);
    }

    if (bold_font)
    {
        ImGui::PopFont();
    }

    ImGui::End();


    // ============================================================
    // 独立于 UI 窗口，绘制在屏幕正下方的文字
    // ============================================================
    DrawDFSummonerSkill();

    // ============================================================
    // 独立于 UI 窗口，绘制在屏幕正下方的文字
    // ============================================================
    if (b_xiaomeiyan)
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
}

int g_SendPackControl = 0;
int g_CalledCount = 0;
void DrawSendPack()
{
    static char input[8] = "";
    ImGui::InputText("##Input2", input, sizeof(input));

    char* endptr = nullptr;
    long num = strtol(input, &endptr, 16);
    g_SendPackControl = static_cast<int>(num);

    ImGui::SameLine();
    if (ImGui::Button("ResetCount", ImVec2(100, 30)))
    {
        g_CalledCount = 0;
    }
    ImGui::Text("CalledCount: %d", g_CalledCount);
}


void DrawGetSkillCDFromTable()
{
    static char input[8] = "";
    ImGui::InputText("##Input3", input, sizeof(input));

    char* endptr = nullptr;
    long num = strtol(input, &endptr, 16);
    int skillid = static_cast<int>(num);

    static int result = 0;

    ImGui::SameLine();
    if (ImGui::Button("TryGetCD", ImVec2(100, 30)))
    {
        result = GetCDFromSkillTable(skillid);
    }
    ImGui::Text("TryGetSkillCD: %d", result);
}


bool bClock = false;
double clockTime = 0;
int g_ParamControl = 0;
void DrawMenu()
{
    ImGui::Begin("##Menu", nullptr, ImGuiWindowFlags_NoCollapse);

    // -----------------------------
    static char input[8] = "";
    static uint32_t ret = 0;
    static bool hexMode = false;

    ImGui::InputText("##Input", input, sizeof(input));

    char* endptr = nullptr;
    long num = strtol(input, &endptr, 16);
    g_ParamControl = static_cast<int>(num);

    ImGui::SameLine();

    if (hexMode)
    {
        ImGui::Text("Result: 0x%08X", ret);
    }
    else
    {
        ImGui::Text("Result: %u", ret);
    }

    if (ImGui::Button("Call", ImVec2(100, 30)))
    {
        ret = CallGameFunction();
        bClock = true;
    }

    ImGui::SameLine();

    if (ImGui::Button(hexMode ? "DEC" : "HEX", ImVec2(70, 30)))
    {
        hexMode = !hexMode;
    }

    // ---------------------
    
    DrawSendPack();

    DrawGetSkillCDFromTable();

    // ----------------------
    ImGui::Text("Skill_CD: %u", skill_CD);
    double tmp = static_cast<double>(skill_CD / 1000);
    ImGui::Text("Skill_CD: %f", tmp);
    ImGui::Text("Skill_CD: %.2f", tmp);

    ImGui::Text("CLOCK: %.2f", clockTime / 1000.0);

    ImGui::End();
}