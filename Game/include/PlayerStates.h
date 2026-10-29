#pragma once
#include "Player.h"

// 玩家的各个状态。新增状态：在 PlayerStateId 加枚举、在这里写一个类，并在 Player::Initialize 里注册

// 待机：有移动输入时切到 Run
class PlayerIdleState : public Player::State
{
public:
    const char* Name() const override { return "Idle"; }
    void Enter(Player& player) override;
    void Update(Player& player, const UpdateArgs& args) override;
};

// 奔跑：朝输入方向转身并移动；没有输入时回到 Idle
class PlayerRunState : public Player::State
{
public:
    const char* Name() const override { return "Run"; }
    void Enter(Player& player) override;
    void Update(Player& player, const UpdateArgs& args) override;
};
