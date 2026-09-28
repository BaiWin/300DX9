#pragma once
#include <chrono>
#include "300.h"

// 上一帧到这一帧过了多少毫秒。每帧只调一次（在 LogicUpdate 里），
// 结果传给所有需要它的地方 —— 这样大家用的才是同一个 delta。
double GetFrameDeltaMilliseconds();

// 本地推算的 CD 递减。英雄自己的 CD 归英雄自己的 OnFrameUpdate() 管，
// 这里只剩所有英雄共用的召唤师技能 D / F。
void UpdateLocalCooldowns(double deltaMilliseconds);