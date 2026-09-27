#include "cooldown.h"

void UpdateCooldowns()
{
	static auto lastTime = std::chrono::steady_clock::now();

	auto now = std::chrono::steady_clock::now();

	double deltaMs =
		std::chrono::duration<double, std::milli>(
			now - lastTime
		).count();

	lastTime = now;

	for (auto& row : gelei_skills)
	{
		for (auto& skill : row)
		{
			if (skill.cd > 0.0)
			{
				skill.start += deltaMs;

				if (skill.start > skill.cd)
				{
					skill.cd = 0.0;
					skill.start = 0.0;
				}

			}
		}
	}

	//ÕÙ»½Ê¦¼¼ÄÜ
	if (g_summonnerSkillSlotInfo.slot1_Hiden_D_CD > 0.0)
	{
		g_summonnerSkillSlotInfo.slot1_Hiden_D_Start += deltaMs;

		if (g_summonnerSkillSlotInfo.slot1_Hiden_D_Start > g_summonnerSkillSlotInfo.slot1_Hiden_D_CD)
		{
			g_summonnerSkillSlotInfo.slot1_Hiden_D_CD = 0.0;
			g_summonnerSkillSlotInfo.slot1_Hiden_D_Start = 0.0;
		}
	}
	if (g_summonnerSkillSlotInfo.slot2_Hiden_F_CD > 0.0)
	{
		g_summonnerSkillSlotInfo.slot2_Hiden_F_Start += deltaMs;

		if (g_summonnerSkillSlotInfo.slot2_Hiden_F_Start > g_summonnerSkillSlotInfo.slot2_Hiden_F_CD)
		{
			g_summonnerSkillSlotInfo.slot2_Hiden_F_CD = 0.0;
			g_summonnerSkillSlotInfo.slot2_Hiden_F_Start = 0.0;
		}
	}

	if (xiaomeiyan_WSkill_JiQiang_CD > 0.0)
	{
		xiaomeiyan_WSkill_Start += deltaMs;
		if (xiaomeiyan_WSkill_Start > xiaomeiyan_WSkill_JiQiang_CD)
		{
			xiaomeiyan_WSkill_JiQiang_CD = 0.0;
			xiaomeiyan_WSkill_Start = 0.0;
		}
	}

	if (bClock)
	{
		//clockTime += deltaMs;
	}
}