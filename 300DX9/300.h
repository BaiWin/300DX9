#pragma once
#include <Windows.h>
#include <string>
#include <chrono>
#include "hero_gelei.h"
#include "vectors.h"
#include "cooldown.h"
#include <queue>
//#include "offsets.h"
#include "offset_update.h"
#include "hero_xiaomeiyan.h"
#include "hero_base.h"
#include "debug.h"
#include "user32.h"
#include "game_slot.h"

void GameMain();

uint32_t CallGameFunction();

void ClearKeyFlag();

void WriteJmp(void* pStartAddress, void* pEndAddress, void* pTarget);

void Trampoline_HookSendPack();

void Trampoline_CastNormalSkillRecvCD();

void Trampoline_SwitchSkill();

void HookSendPack(uintptr_t eax, uintptr_t esi);

void CastNormalSkillRecvCD(int ecx, int ebx);

void SwitchSkill(int esi, int edi);

void ProcessSkillQueue();

int GetCDFromSkillTable(int skillID);

extern uintptr_t base;

extern int skill_ID;
extern int skill_CD;
extern int key_flags;
extern bool bSpace;
extern HWND g_hWnd;
extern int g_ParamControl;
extern int g_SendPackControl;
extern int g_CalledCount;
extern bool bClock;
extern double clockTime;
extern int test_GetCDFromSkillTable_Param;



struct SummonnerSkillSlotInfo
{
	double slot1_Hiden_D_Start;
	double slot1_Hiden_D_CD; // 170000
	const int slot1_Hiden_D_ID = 0x1F5D;  

	double slot2_Hiden_F_Start;
	double slot2_Hiden_F_CD; // 180000
	const int slot2_Hiden_F_ID = 0x1F63;

	int slot1_SkillID;
	uintptr_t slot1_Obj;
	int Slot1_SkillCD;

	int slot2_SkillID;
	uintptr_t slot2_Obj;
	int Slot2_SkillCD;
};

extern SummonnerSkillSlotInfo g_summonnerSkillSlotInfo;

double GetFrameDeltaMilliseconds();

void UpdateLocalCooldowns(double deltaMilliseconds);

struct CastSkillTask
{
    int slot_id;
};

extern std::queue<CastSkillTask> castSkillTeskQueue;
