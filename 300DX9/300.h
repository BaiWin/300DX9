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
#include "debug.h"

void GameMain();

uint32_t CallGameFunction();

void CastNormalSkillDirect(int slot_id);

void ClearKeyFlag();

void WriteJmp(void* pStartAddress, void* pEndAddress, void* pTarget);

void Trampoline_HookSendPack();

void Trampoline_CastNormalSkillRecvCD();

void Trampoline_SwitchSkill();

void Trampoline_SendSkill();

void HookSendPack(uintptr_t eax, uintptr_t esi);

void CastNormalSkillRecvCD(int ecx, int ebx);

void SwitchSkill(int esi, int edi);

void SendSkill(int ecx);

void ProcessSkillQueue();

int GetCDFromSkillTable(int skillID);

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

struct Skill
{
    int selectid;
    int skillid;
    double start;
    double cd;
};

extern Skill gelei_skills[3][3];

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

void UpdateCooldowns();

struct CastSkillTask
{
    int slot_id;
};

extern std::queue<CastSkillTask> castSkillTeskQueue;
