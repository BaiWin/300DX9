#pragma once
#include "hero_base.h"

class HeroGeLei : public HeroBase
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
	struct GeLeiSkill
	{
		int selectid;
		int skillid;
		double start;
		double cd;
	};

	GeLeiSkill gelei_skills[3][3] =
	{
		{
			{11202, 11211, 0.0, 0.0},
			{11205, 11218, 0.0, 0.0},
			{11208, 11223, 0.0, 0.0}
		},
		{
			{11203, 11215, 0.0, 0.0},
			{11206, 11219, 0.0, 0.0},
			{11209, 11225, 0.0, 0.0}
		},
		{
			{11204, 11217, 0.0, 0.0},
			{11207, 11221, 0.0, 0.0},
			{11210, 11227, 0.0, 0.0}
		}
	};
};

extern HeroGeLei g_hero_gelei;



