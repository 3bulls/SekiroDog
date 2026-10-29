#include "PlayerStates.h"

using namespace DirectX::SimpleMath;

void PlayerIdleState::Enter(Player& player)
{
    player.PlayAnimation(Player::Anim::Idle);
}

void PlayerIdleState::Update(Player& player, const UpdateArgs& args)
{
    if (player.MoveDirection(args) != Vector3::Zero)
        player.ChangeState(PlayerStateId::Run);
}

void PlayerRunState::Enter(Player& player)
{
    player.PlayAnimation(Player::Anim::Run);    // 每次起跑从动画开头播放
}

void PlayerRunState::Update(Player& player, const UpdateArgs& args)
{
    const Vector3 dir = player.MoveDirection(args);
    if (dir == Vector3::Zero)
    {
        player.ChangeState(PlayerStateId::Idle);
        return;
    }
    player.TurnTowards(dir, args.dt);
    player.Move(dir * player.runSpeed * args.dt);
}
