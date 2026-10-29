#pragma once
#include "GameObject.h"

// 通过 EventBus 广播的游戏事件（只放数据，不放逻辑）

// 玩家状态切换，例如 Idle -> Run
struct PlayerStateChangedEvent
{
    ObjectHandle player;
    const char* from;
    const char* to;
};
