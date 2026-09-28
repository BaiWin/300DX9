#include "game_slot.h"

SlotsPanelInfo g_slotsPanelInfo;

void UpdateSlotsPanelInfo()
{
    uintptr_t skillTable = *reinterpret_cast<uintptr_t*>(base + dword_SkillTable);  // 0x1A51398
    WORD slot1_SKillID = *reinterpret_cast<WORD*>(skillTable + dword_SkillTable_Slot_SkillIDOffset + 0x0);
    WORD slot2_SKillID = *reinterpret_cast<WORD*>(skillTable + dword_SkillTable_Slot_SkillIDOffset + 0x4);
    WORD slot3_SKillID = *reinterpret_cast<WORD*>(skillTable + dword_SkillTable_Slot_SkillIDOffset + 0x8); // .text:00816704    mov     word ptr dword_1E12B28+2, ax
    g_slotsPanelInfo.skillid_Q = static_cast<int>(slot1_SKillID);
    g_slotsPanelInfo.skillid_W = static_cast<int>(slot1_SKillID);
    g_slotsPanelInfo.skillid_E = static_cast<int>(slot1_SKillID);
}