#pragma once
#include "300.h"

struct SlotsPanelInfo
{
	int skillid_T;
	int skillid_Q;
	int skillid_W;
	int skillid_E;
	int skillid_R;
	int skillid_D;
	int skillid_F;
	int skillid_G;
};

extern SlotsPanelInfo g_slotsPanelInfo;

void UpdateSlotsPanelInfo();