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

void DrawHeroSelector()
{
    ImGui::Text("Hero");

    if (ImGui::RadioButton("gelei", g_activeHero == &g_hero_gelei))
    {
        g_activeHero = &g_hero_gelei;
    }

    if (ImGui::RadioButton("xiaomeiyan", g_activeHero == &g_hero_xiaomeiyan))
    {
        g_activeHero = &g_hero_xiaomeiyan;
    }
    if (g_activeHero)
    {
        g_activeHero->OnDrawMenu();
        g_activeHero->OnDrawOverlay();
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
    //ImGui::Text("PresentFPS: %d", PresentFPS);
    ImGui::Begin(
        "##Overlay",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize
    );
    
    DrawHeroSelector();

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

    if (bold_font)
    {
        ImGui::PopFont();
    }

    ImGui::End();


    // ============================================================
    // 独立于 UI 窗口，绘制在屏幕正下方的文字
    // ============================================================
    DrawDFSummonerSkill();
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


void DrawMenuTest()
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

void DrawMenu()
{
    //DrawMenuTest();

    DrawMenu2();
}
