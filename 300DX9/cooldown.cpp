#include "cooldown.h"

// =============================================================================
//  上一帧到这一帧过了多少毫秒。
//
//  每帧只调一次（在 LogicUpdate 里），结果传给所有需要它的地方 ——
//  这样大家用的才是同一个 delta。
// =============================================================================
double GetFrameDeltaMilliseconds()
{
	static auto lastTime = std::chrono::steady_clock::now();

	auto now = std::chrono::steady_clock::now();

	double deltaMilliseconds =
		std::chrono::duration<double, std::milli>(
			now - lastTime
		).count();

	lastTime = now;

	return deltaMilliseconds;
}


// =============================================================================
//  本地推算的 CD 递减。
//
//  ★ 召唤师技能 D / F 在这里，因为它俩所有英雄共用，不属于任何英雄。
//    英雄自己的 CD 归英雄自己的 OnFrameUpdate() 管。
// =============================================================================
void UpdateLocalCooldowns(double deltaMilliseconds)
{
	// 召唤师技能
	if (g_summonnerSkillSlotInfo.slot1_Hiden_D_CD > 0.0)
	{
		g_summonnerSkillSlotInfo.slot1_Hiden_D_Start += deltaMilliseconds;

		if (g_summonnerSkillSlotInfo.slot1_Hiden_D_Start > g_summonnerSkillSlotInfo.slot1_Hiden_D_CD)
		{
			g_summonnerSkillSlotInfo.slot1_Hiden_D_CD = 0.0;
			g_summonnerSkillSlotInfo.slot1_Hiden_D_Start = 0.0;
		}
	}
	if (g_summonnerSkillSlotInfo.slot2_Hiden_F_CD > 0.0)
	{
		g_summonnerSkillSlotInfo.slot2_Hiden_F_Start += deltaMilliseconds;

		if (g_summonnerSkillSlotInfo.slot2_Hiden_F_Start > g_summonnerSkillSlotInfo.slot2_Hiden_F_CD)
		{
			g_summonnerSkillSlotInfo.slot2_Hiden_F_CD = 0.0;
			g_summonnerSkillSlotInfo.slot2_Hiden_F_Start = 0.0;
		}
	}

	if (bClock)
	{
		//clockTime += deltaMilliseconds;
	}
}
