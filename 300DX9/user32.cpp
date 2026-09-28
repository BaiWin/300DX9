#include "user32.h"

using ScreenToClient_t = BOOL(__stdcall*)(HWND hWnd, LPPOINT lpPoint);
static ScreenToClient_t g_ScreenToClient = nullptr;
using ClientToScreen_t = BOOL(__stdcall*)(HWND hWnd, LPPOINT lpPoint);
static ClientToScreen_t g_ClientToScreen = nullptr;


void InitUser32()
{
	HMODULE hUser32 = GetModuleHandleA("user32.dll");
	if (hUser32)
	{
		g_ClientToScreen = (ClientToScreen_t)GetProcAddress(hUser32, "ClientToScreen");
		g_ScreenToClient = (ScreenToClient_t)GetProcAddress(hUser32, "ScreenToClient");
	}
}