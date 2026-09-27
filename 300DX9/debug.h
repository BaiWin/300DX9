#pragma once
#include <Windows.h>

void DebugPrint(const char* msg);


//#include <windows.h>
//#include <cstdio>
//
//template<typename T>
//void Assert(T val, const char* paramName, const char* file, int line)
//{
//    bool isZero = false;
//    if constexpr (std::is_floating_point_v<T>)
//    {
//        // float 浮点数判断：严格等于0，如需epsilon接近0可自行修改
//        isZero = (val == 0.0f);
//    }
//    else
//    {
//        // 整数类型 uintptr_t / int / WORD / DWORD / BYTE
//        isZero = (val == 0);
//    }
//
//    if (isZero)
//    {
//        char buf[512];
//        if constexpr (std::is_floating_point_v<T>)
//        {
//            sprintf_s(buf, "ASSERT ERROR: param %s == 0.0f, value=%f, file:%s, line:%d", paramName, (double)val, file, line);
//        }
//        else if constexpr (std::is_same_v<T, uintptr_t>)
//        {
//            sprintf_s(buf, "ASSERT ERROR: param %s == 0, value=0x%llX, file:%s, line:%d", paramName, (unsigned long long)val, file, line);
//        }
//        else
//        {
//            sprintf_s(buf, "ASSERT ERROR: param %s == 0, value=%d(0x%X), file:%s, line:%d", paramName, (int)val, (unsigned int)val, file, line);
//        }
//        OutputDebugStringA(buf);
//        // 可选：触发断点，调试器下中断，Release可注释掉
//        DebugBreak();
//    }
//}
//
//// 宏封装，自动带入文件名、行号
//#define CHECK_ZERO_ASSERT(val) Assert(val, #val, __FILE__, __LINE__)