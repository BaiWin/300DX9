#include "300.h"

using World2Screen_t = char(__thiscall*)(uintptr_t a1, int a2, DWORD* a3, DWORD* a4, int a5, int a6);
static World2Screen_t g_World2Screen = nullptr; //

using CastSkill_t = unsigned __int16* (__cdecl*)(int a1,unsigned __int8 a2,unsigned __int8 a3,int a4);
static CastSkill_t g_CastSkill = nullptr; // 

using CastSummonerSkillJJC_t = char(__thiscall*)(int a1, float a2, unsigned __int16 a3, unsigned __int16 a4);
static CastSummonerSkillJJC_t g_SummonerSkill = nullptr; //

using GetCDRecordObj_t = DWORD* (__thiscall*)(DWORD* thisptr, int a2);
static GetCDRecordObj_t g_GetCDRecordObj = nullptr;   // 参数(E8 ?? ?? ?? ?? 68 71 1E 00 00, id)

struct TreeFindResult          // out 的 12 字节
{
	uintptr_t node;            // +0x00  搜索最后停留的节点（一般不用）
	uint32_t  flag;            // +0x04  0/1：是否记过 lower_bound
	uintptr_t lowerBound;      // +0x08  ★ 这个才是要用的节点
};
using TreeFindPlayerResult_t = TreeFindResult * (__thiscall*)(void* thisMap, TreeFindResult* out, const uint32_t* key);
static TreeFindPlayerResult_t g_TreeFindPlayerResult = nullptr;

//using StartMoveTo 


//using CastSummonerSkill1_t = char(__thiscall*)(char** a1, unsigned __int16 a2, unsigned int a3, __int64 a4, float a5, int a6, char a7);
//static CastSummonerSkill1_t g_SummonerSkill = nullptr;

uintptr_t base;

// 8.28 dump




void GameMain()
{
	HMODULE gameBase = GetModuleHandleA(nullptr);
	
	if (!gameBase)
		return;

	base = reinterpret_cast<uintptr_t>(gameBase);

	InitUser32();

	g_CastSkill = reinterpret_cast<CastSkill_t>(base + offset_func_castskill);
	g_SummonerSkill = reinterpret_cast<CastSummonerSkillJJC_t>(base + offset_func_castSummonerSkill);
	g_TreeFindPlayerResult = reinterpret_cast<TreeFindPlayerResult_t>(base + offset_func_treeFindPlayerObj);
	g_GetCDRecordObj = reinterpret_cast<GetCDRecordObj_t>(base + offset_func_getCDRecordObj);

	//g_World2Screen = reinterpret_cast<World2Screen_t>(base + offset_func_world2Screen);
	//g_GetSkillID = reinterpret_cast<GetSkillID_t>(base + offset_func_getskillid); //0x8166A0

	WriteJmp((void*)(base + Trampoline_CastNormalSkillRecvCD_Start), (void*)(base + Trampoline_CastNormalSkillRecvCD_End), Trampoline_CastNormalSkillRecvCD); // 0xAF5BB1 to  0xAF5BB7
	WriteJmp((void*)(base + Trampoline_SwitchSkill_Start), (void*)(base + Trampoline_SwitchSkill_End), Trampoline_SwitchSkill);// 0x81D679 to  0x81D67F
	WriteJmp((void*)(base + Trampoline_HookSendPack_Start), (void*)(base + Trampoline_HookSendPack_End), Trampoline_HookSendPack);
	//WriteJmp((void*)(base + 0x7F30A1), (void*)(base + 0x7F30A8), Trampoline_SendSkill);
}

uint32_t CallGameFunction()
{
	if (g_CastSkill)
	{
		unsigned __int16* result = g_CastSkill(g_ParamControl, 0, 256, 1); // 注意参数3从0(1byte) 改为了25(4bytes)

		if (result)
			return static_cast<uint32_t>(
				reinterpret_cast<uintptr_t>(result));
	}

	//if (g_SummonerSkill)
	//{
	//	//1A6876C+ 300.exe]+44]
	//	uintptr_t temp = *reinterpret_cast<uintptr_t*>(base + 0x1A6876C);
	//	uintptr_t a1 = *reinterpret_cast<uintptr_t*>(temp + 0x44);
	//	if (a1)
	//	{
	//		char result = g_SummonerSkill(a1, 0xFFFFFFFF, g_ParamControl, 1); // 注意参数3从0(1byte) 改为了25(4bytes)

	//		if (result)
	//			return static_cast<uint32_t>(
	//				result);
	//	}
	//}
	return 0;
}


void CastNormalSkill(int slot_id)
{
	if (slot_id <= 4 && g_CastSkill)
	{
		auto result = g_CastSkill(slot_id, 0, 256, 1);
	}
}

void ClearKeyFlag()
{
	key_flags = 0;
}


void WriteJmp(void* pStartAddress, void* pEndAddress, void* pTarget)
{
	int length = (int)((char*)pEndAddress - (char*)pStartAddress);
	if (length < 5)
	{
		// 至少需要5字节才能写JMP，否则报错或处理
		return;
	}

	DWORD oldProtect;

	// 1. 修改内存保护属性为可写
	VirtualProtect(pStartAddress, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

	// 2. 写入 CALL 指令
	unsigned char* pBytes = (unsigned char*)pStartAddress;
	pBytes[0] = 0xE8;  // CALL 操作码

	// 3. 计算相对偏移：目标地址 - (当前地址 + 5)
	int offset = (int)((char*)pTarget - ((char*)pStartAddress + 5));
	*(int*)(pBytes + 1) = offset;

	// 4. 填充 NOP
	// 如果原指令是11字节，CALL占5字节，剩6字节填充NOP
	for (int i = 5; i < length; i++)
	{
		pBytes[i] = 0x90;  // nop
	}

	FlushInstructionCache(GetCurrentProcess(), pStartAddress, length);
	// 5. 恢复内存保护属性
	VirtualProtect(pStartAddress, length, oldProtect, &oldProtect);
}


//.text:001DFA74                 movzx   eax, word ptr[esi + 8]
//.text : 001DFA78                 mov[ebp + var_E4], eax
//.text : 001DFA7E                 call    sub_D70140
__declspec(naked) void Trampoline_HookSendPack()
{
	__asm {
		movzx eax, word ptr[esi + 8]
		mov [ebp - 0xE4], eax

		pushad
		pushfd

		push esi
		push eax
		call HookSendPack
		add esp, 8

		popfd
		popad

		ret
	}
}

void HookSendPack(uintptr_t eax, uintptr_t esi)   // 2AFA 技能发包cmd
{
	uint16_t pack = static_cast<uint16_t>(eax);
	if (pack == static_cast<uint16_t>(g_SendPackControl))
	{
		g_CalledCount++;
	}
}


//.text:00AF5BB1                 mov     ecx, [eax + 8]
//.text : 00AF5BB4                 mov     eax, [eax + 4]
//.text:00AF5BB7                 mov[ebp + 44Ch + var_470], ecx

__declspec(naked) void Trampoline_CastNormalSkillRecvCD()
{
	__asm {
		mov ecx, [eax + 0x8]
		mov eax, [eax + 0x4]

		pushad
		pushfd

		push ebx
		push ecx
		call CastNormalSkillRecvCD
		add esp, 8

		popfd
		popad

		ret
	}
}

int skill_CD;
void CastNormalSkillRecvCD(int ecx, int ebx)    //注意自动转好的技能服务器不会下发，但手动重置技能会下发，并且cd = 0
{
	skill_CD = ecx;
	int skill_cd = ecx;
	int slot_index = ebx;

	if (g_activeHero)
	{
		g_activeHero->OnReceiveSkillCooldown(skill_cd, slot_index);
	}
	#if 0
	#endif // 0
}

//.text:00AFD11A                 mov     ecx, [eax + 4]
//.text : 00AFD11D                 add     ecx, [eax + 8]
//.text : 00AFD120                 mov     dword ptr[esp + 40h + var_8], ecx

__declspec(naked) void Trampoline_CastSummonerSkillRecvCD()
{
	__asm {
		mov ecx, [eax + 0x4]
		add eax, [eax + 0x8]

		pushad
		pushfd

		push ebx
		push ecx
		call CastNormalSkillRecvCD
		add esp, 8

		popfd
		popad

		ret
	}
}


//.text:0081D679                 mov     eax, [esp+10h+arg_4] esp + 0x18
//.text:0081D67D                 mov[eax], esi
//.text:0081D67F                 call    sub_7C890

__declspec(naked) void Trampoline_SwitchSkill()
{
	__asm {
		mov eax, [esp + 0x1C]
		mov [eax], esi

		pushad
		pushfd

		push edi
		push esi
		call SwitchSkill
		add esp, 8

		popfd
		popad

		ret
	}
}

int key_flags = 0;
int skill_ID = 0;
void SwitchSkill(int esi, int edi)
{
	int select_id = esi;
	int slot_id = edi;

	if (g_activeHero)
	{
		g_activeHero->OnSwitchSkill(select_id, slot_id);
	}
	#if 0
	#endif // 0
}



std::queue<CastSkillTask> castSkillTeskQueue;
void ProcessSkillQueue()
{
	while (!castSkillTeskQueue.empty())
	{
		CastSkillTask task = castSkillTeskQueue.front();
		castSkillTeskQueue.pop();

		if (g_CastSkill)
		{
			/*OutputDebugStringA(
				"Sendddd!\n"
			);*/
			int slotid = task.slot_id;
			unsigned __int16* result = g_CastSkill(slotid, 0, 256, 1);  // 第三个参数 不是0(1byte) 而是 256(4bytes)
		}
	}
}


bool bSpace = false;
/*
void CastSummonerSkill1()
{
	if (bSpace)
	{
		bSpace = false;

		if (g_SummonerSkill && g_World2Screen)
		{
			uintptr_t hoverStruct = *reinterpret_cast<uintptr_t*>(base + dword_HoverStruct);
			uintptr_t this_ptr = *reinterpret_cast<uintptr_t*>(hoverStruct + 0x44);

			float playerPosX = *reinterpret_cast<float*>(this_ptr + 0x84);
			float playerPosY = *reinterpret_cast<float*>(this_ptr + 0x88);
			float playerPosZ = 0; //*reinterpret_cast<float*>(this_ptr + 0x8C);

			char buffer[128];
			sprintf_s(
				buffer,
				sizeof(buffer),
				"playerPosX = %f, playerPosY = %f, playerPosZ = %f\n",
				playerPosX,
				playerPosY,
				playerPosZ
			);
			OutputDebugStringA(buffer);
			vec3_t pt{ playerPosX , playerPosY , playerPosZ };
			uintptr_t param1Up = *reinterpret_cast<uintptr_t*>(base + dword_castSummnerSkillParam1Up);
			uintptr_t param1 = *reinterpret_cast<uintptr_t*>(param1Up + dword_castSummnerSkillParam1);

			DWORD outputX = 0, outputY = 0;

			char bRet = g_World2Screen(param1, (int)&pt, &outputX, &outputY,
				*reinterpret_cast<int*>(base + dword_castSummnerSkillxRight),
				*reinterpret_cast<int*>(base + dword_castSummnerSkillxBottom)
			);
			//现在是屏幕坐标了

			uintptr_t pos_Struct = *reinterpret_cast<uintptr_t*>(base + dword_MousePos);
			if (!pos_Struct) return;
			int* pX = reinterpret_cast<int*>(pos_Struct + 0x264);
			int* pY = reinterpret_cast<int*>(pos_Struct + 0x268);

			
			sprintf_s(
				buffer,
				sizeof(buffer),
				"Before Write: mousePosX = %d, mousePosY = %d | outPutX=%d, outPutY=%d\n",
				*pX,
				*pY,
				outputX,
				outputY
			);
			OutputDebugStringA(buffer);

			*reinterpret_cast<int*>(pos_Struct + 0x264) = static_cast<int>(outputX); // 写入 X
			*reinterpret_cast<int*>(pos_Struct + 0x268) = static_cast<int>(outputY); // 写入 Y

			sprintf_s(
				buffer,
				sizeof(buffer),
				"After Write:  mousePosX = %d, mousePosY = %d\n",
				*pX,
				*pY
			);
			OutputDebugStringA(buffer);

			if (this_ptr)
			{
				char result = g_SummonerSkill(this_ptr, 0xFFFFFFFF, 0x1F63, 1); // 0x1F63=闪现

				if (result)
					return;
			}
		}
	}
}
*/


SummonnerSkillSlotInfo g_summonnerSkillSlotInfo;
//void GetSummonnerSkillSlotInfo()
//{
//	//9.11
//	//80 38 00 74 12 8B 48 08 8B 40 04 89 86 10 04 00 00 89 8E 14 04 00 00
//	// 24F8418 + 300.exe + 0xAED8 + 690] + 448 + 414
//
//	uintptr_t slotStruct = *reinterpret_cast<uintptr_t*>(base + offset_SlotStruct + 0xAED8 + 0x690);
//	uintptr_t slot1 = *reinterpret_cast<uintptr_t*>(slotStruct + 0x448 * 0);
//	uintptr_t slot2 = *reinterpret_cast<uintptr_t*>(slotStruct + 0x448 * 1);
//	int slot1_CD = *reinterpret_cast<int*>(slotStruct + 0x448 * 0 + 0x414);
//	int slot2_CD = *reinterpret_cast<int*>(slotStruct + 0x448 * 1 + 0x414);
//
//	uintptr_t summonerSkillStruct = *reinterpret_cast<uintptr_t*>(base + dword_SummonerSkillStruct);
//	WORD slot1SkillID = *reinterpret_cast<WORD*>(summonerSkillStruct + 4 * (0x11BCE + 0x0) + 0x0 + 0x3);  // 0x0就是slot0
//	WORD slot2SkillID = *reinterpret_cast<WORD*>(summonerSkillStruct + 4 * (0x11BCE + 0x1) + 0x1 + 0x3);  // 0x1就是slot1
//
//	g_summonnerSkillSlotInfo.slot1_Obj = slot1;
//	g_summonnerSkillSlotInfo.slot2_Obj = slot2;
//	g_summonnerSkillSlotInfo.Slot1_SkillCD = slot1_CD;
//	g_summonnerSkillSlotInfo.Slot2_SkillCD = slot2_CD;
//	g_summonnerSkillSlotInfo.slot1_SkillID = static_cast<int>(slot1SkillID);
//	g_summonnerSkillSlotInfo.slot1_SkillID = static_cast<int>(slot2SkillID);
//
//	char buffer[128];
//	sprintf_s(
//		buffer,
//		sizeof(buffer),
//		"g_summonnerSkillSlotInfo.Slot2_SkillCD = %d\n",
//		g_summonnerSkillSlotInfo.Slot2_SkillCD
//	);
//	OutputDebugStringA(buffer);
//}

void CastSummonerSkill()
{
	uintptr_t hoverStruct = *reinterpret_cast<uintptr_t*>(base + dword_HoverStruct);
	uintptr_t player_self = *reinterpret_cast<uintptr_t*>(hoverStruct + 0x44);

	if (!player_self) return;

	//仅执行一次
	if (key_flags & (1 << 5))
	{
		key_flags &= ~(1 << 5);  //先置回

		char result = g_SummonerSkill(player_self, 0xFFFFFFFF, 0x1F5D, 1); // 0x1F5D=治疗

		g_summonnerSkillSlotInfo.slot1_Hiden_D_CD = 170000;
	}

	if (key_flags & (1 << 6))
	{
		key_flags &= ~(1 << 6);

		char result = g_SummonerSkill(player_self, 0xFFFFFFFF, 0x1F63, 1); // 0x1F63=闪现

		g_summonnerSkillSlotInfo.slot2_Hiden_F_CD = 180000;
	}
}


bool IsEnemyHero(uintptr_t obj)
{
	uintptr_t hoverStruct = *reinterpret_cast<uintptr_t*>(base + dword_HoverStruct);
	uintptr_t me = *reinterpret_cast<uintptr_t*>(hoverStruct + 0x44);
	uint8_t myCamp = *(uint8_t*)(me + 0x4306);

	uint8_t  camp = *(uint8_t*)(obj + 0x4306);   // 阵营
	uint16_t type = *(uint16_t*)(obj + 0x4582);   // 类型字
	uint32_t flag = *(uint32_t*)(obj + 0x4580);   // = 类型<<16 | 标志位
	if (camp == 0)          return false;         // ① 排除未同步/泉水占位
	if (camp == myCamp)     return false;         // ② 排除自己人
	if (type != 0xFFFF)     return false;         // ③ 只要英雄
	if ((flag & 0x40) == 0) return false;         // ④ 可选：目标过滤位（见下）
	//if (*(int*)(obj + 0x534) <= 0) return false;  // ⑤ 可选：+0x534=当前血，死了跳过
	return true;
}

uintptr_t GetCurrentHoverEnemyObj()
{
	static DWORD lastHoverID = 0x7FFFFFFF;
	uintptr_t obj = 0;
	uintptr_t hoverStruct = *reinterpret_cast<uintptr_t*>(base + dword_HoverStruct);
	DWORD hoverID = *reinterpret_cast<DWORD*>(hoverStruct + 0x7c);

	//static DWORD  lastTick = 0;
	//DWORD now = GetTickCount64();
	//if (now - lastTick < 50) return obj;    // ① 节流：悬停不需要每帧
	//lastTick = now;


	if (hoverID == lastHoverID)
	{
		return obj;
	}
	lastHoverID = hoverID;

	if (hoverID != 0x7FFFFFFF)
	{
		if (g_TreeFindPlayerResult)
		{
			TreeFindResult res = {};      // 7AB964 9.23  // 这里不要解引用 
			g_TreeFindPlayerResult(reinterpret_cast<void*>(hoverStruct + 0x94), &res, reinterpret_cast<uint32_t*>(&hoverID));
			uintptr_t node = res.lowerBound;

			/*char buffer[128];
	sprintf_s(
		buffer,
		sizeof(buffer),
		"hoverID = %d \n",
		hoverID
	);
	OutputDebugStringA(buffer);*/

		obj = *reinterpret_cast<uintptr_t*>(node + 0x14);
		//if (node
		//	&& *reinterpret_cast<uint8_t*>(node + 0x0D) == 0            // !_Isnil
		//	&& *reinterpret_cast<uint32_t*>(node + 0x10) <= hoverID      // 与游戏同款判据
		//	&& node != *reinterpret_cast<uintptr_t*>(hoverStruct + 0x94))          // != head
		//	obj = *reinterpret_cast<uintptr_t*>(node + 0x14);            // value = Entity*
		//else
		//	obj = *reinterpret_cast<uintptr_t*>(hoverStruct + 0x100);              // 兜底（和游戏一致）

		//if (obj && (*reinterpret_cast<uint8_t*>(obj + 0x4580) & 0x40) == 0)
		//	obj = 0;                                        // 游戏自己的"可选中"判据，可选


		/*if (!IsEnemyHero(obj))
		{
			obj = 0;
		}*/

		return obj;
		}
	}
	return obj;
}



int test_GetCDFromSkillTable_Param;
int GetCDFromSkillTable(int skillID)
{
	int cd = 0;
	uintptr_t skillTable = *reinterpret_cast<uintptr_t*>(base + dword_SkillTable);
	if (g_GetCDRecordObj)
	{
		uintptr_t cdRecordObj = reinterpret_cast<uintptr_t>(g_GetCDRecordObj(reinterpret_cast<DWORD*>(skillTable), skillID));

		//CDRecord(12 字节)
		//	+ 0x00  u8  valid                // 过期也不清零，别只看它
		//	+ 0x01  pad[3]
		//	+ 0x04  u32 开始ms                       // AFDA7C 9.23
		//	+ 0x08  u32 时长ms               // ← "CD在+8" 指的就是它   AFDA79

		// AFDA7F 	call 时钟 0x1C11B0
		int start_ms = *reinterpret_cast<int*>(cdRecordObj + 0x4);
		int duration_ms = *reinterpret_cast<int*>(cdRecordObj + 0x8);

		int now = *reinterpret_cast<int*>(base + dword_Clock);

		cd = start_ms + duration_ms - now;


		char buffer[128];
	sprintf_s(
		buffer,
		sizeof(buffer),
		"skillid = %d , start_ms = %d, duration_ms = %d , duration_ms = %d\n",
		skillID,
		start_ms,
		duration_ms,
		now
	);
	OutputDebugStringA(buffer);
	}
	return cd;
}


float GetHoverEnemyHeroDistance()
{
	uintptr_t obj = GetCurrentHoverEnemyObj();
	if (obj == 0) return 0;
	uintptr_t hoverStruct = *reinterpret_cast<uintptr_t*>(base + dword_HoverStruct);
	uintptr_t m_player = *reinterpret_cast<uintptr_t*>(hoverStruct + 0x44);

	vec3_t v3_Enemy = *reinterpret_cast<vec3_t*>(obj + 0x84);
	vec3_t v3_Me = *reinterpret_cast<vec3_t*>(m_player + 0x84);
	float dist = v3_Me.distance_to(v3_Enemy) * 1000.0;

	char buffer[128];
	sprintf_s(
		buffer,
		sizeof(buffer),
		"dist = %f\n",
		dist
	);
	OutputDebugStringA(buffer);

	return dist;
}

void LogicUpdate()
{
	// CD相关更新
	double deltaMilliseconds = GetFrameDeltaMilliseconds();

	UpdateLocalCooldowns(deltaMilliseconds);

	if (g_activeHero)
	{
		g_activeHero->OnFrameUpdate(deltaMilliseconds);
	}

	// 英雄在 OnFrameUpdate 里排的技能，这一帧就放出去，不拖到下一帧
	ProcessSkillQueue();

	// 召唤师技能释放相关
	CastSummonerSkill();

	// 技能面板的槽位技能id
	UpdateSlotsPanelInfo();


	//GetSummonnerSkillSlotInfo();

	//CastSummonerSkill1();

	//GetHoverEnemyHeroDistance();
}
