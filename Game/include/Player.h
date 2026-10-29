#pragma once
#include <SimpleMath.h>
#include "Animator.h"
#include "CollisionSystem.h"
#include "GameObject.h"
#include "SkinnedModel.h"
#include "StateMachine.h"

enum class PlayerStateId
{
    Idle,
    Run,
};

// 玩家：行为由状态机驱动（见 PlayerStates.h），Player 本身只提供状态可用的“动作”。
// 跑步动画是原地动画（In Place），位移由代码驱动；用胶囊体与场景碰撞。
class Player : public GameObject
{
public:
    using State = StateMachine<Player, PlayerStateId>::State;
    enum class Anim { Idle, Run };

    bool Initialize(const InitArgs& args) override;
    void Update(const UpdateArgs& args) override;
    void Draw(const DrawArgs& args) override;
    void Finalize() override;
    void OnCollision(GameObject& other, const Contact& contact) override;

    // ---- 供状态调用的动作 ----
    // 相对相机的水平移动方向（已归一化）；没有输入时返回零向量
    DirectX::SimpleMath::Vector3 MoveDirection(const UpdateArgs& args) const;
    void TurnTowards(const DirectX::SimpleMath::Vector3& dir, float dt);
    void Move(const DirectX::SimpleMath::Vector3& delta);
    void PlayAnimation(Anim anim);
    void ChangeState(PlayerStateId id) { m_stateMachine.ChangeState(id); }

    DirectX::SimpleMath::Matrix World() const;
    const DirectX::SimpleMath::Vector3& Position() const { return m_position; }
    float Yaw() const { return m_yaw; }
    PlayerStateId CurrentState() const { return m_stateMachine.Current(); }

    float runSpeed = 2.5f;                  // 与跑步动画步幅匹配（米/秒）
    float turnSpeed = DirectX::XM_2PI * 2;  // 720 度/秒
    float blendTime = 0.15f;                // 动画切换过渡时间
    float radius = 0.25f;                   // 碰撞胶囊半径
    float height = 0.9f;                    // 碰撞胶囊总高
    bool autoRun = false;                   // 调试：一直沿世界 +Z 奔跑

private:
    void SyncCollider();

    SkinnedModel m_model;
    Animator m_animator;
    int m_clips[2] = { -1, -1 };            // 按 Anim 索引
    StateMachine<Player, PlayerStateId> m_stateMachine;
    ObjectHandle m_camera;
    Collider* m_collider = nullptr;

    DirectX::SimpleMath::Vector3 m_position;
    float m_yaw = 0.0f;                     // 0 = 面朝 +Z
};
