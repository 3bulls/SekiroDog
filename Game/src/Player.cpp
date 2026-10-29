#include "Player.h"

#include <algorithm>
#include <cmath>
#include "GameEvents.h"
#include "Log.h"
#include "PlayerStates.h"
#include "Scene.h"
#include "ThirdPersonCamera.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
    float WrapAngle(float a)
    {
        a = std::fmod(a + XM_PI, XM_2PI);
        if (a < 0.0f) a += XM_2PI;
        return a - XM_PI;
    }
}

bool Player::Initialize(const InitArgs& args)
{
    const auto dir = args.assetDir / "Player";
    SkinnedModelDesc desc;
    desc.fbxPath = (dir / "Models" / "DogWarrior.fbx").string();
    desc.baseColorPath = (dir / "Textures" / "DogWarrior_BaseColor.png").wstring();
    desc.normalPath = (dir / "Textures" / "DogWarrior_Normal.png").wstring();
    desc.metallicSmoothnessPath = (dir / "Textures" / "DogWarrior_MetallicSmoothness.png").wstring();
    desc.shaderPath = (args.shaderDir / "Skinned.hlsl").wstring();
    if (!m_model.Initialize(args.device, args.context, desc))
        return false;

    m_clips[static_cast<int>(Anim::Idle)] = m_model.FindClip("Idle");
    m_clips[static_cast<int>(Anim::Run)] = m_model.FindClip("Run");
    Log("Idle clip %d, Run clip %d", m_clips[0], m_clips[1]);
    m_animator.Initialize(&m_model);

    // 相机不存在时按世界方向移动
    if (auto* camera = GetScene().Find<ThirdPersonCamera>())
        m_camera = camera->Handle();

    Collider collider;
    collider.owner = Handle();
    collider.shape = CapsuleShape{};
    collider.layer = CollisionLayer::Player;
    collider.mask = CollisionLayer::Static | CollisionLayer::Enemy;
    m_collider = GetScene().Collisions().Add(collider);
    SyncCollider();

    m_stateMachine.Add(PlayerStateId::Idle, std::make_unique<PlayerIdleState>());
    m_stateMachine.Add(PlayerStateId::Run, std::make_unique<PlayerRunState>());
    m_stateMachine.Start(*this, PlayerStateId::Idle);
    return true;
}

void Player::Update(const UpdateArgs& args)
{
    if (m_stateMachine.Update(*this, args))
        GetScene().Events().Publish(PlayerStateChangedEvent{ Handle(), m_stateMachine.PreviousName(), m_stateMachine.CurrentName() });
    m_animator.Update(args.dt);
}

void Player::Draw(const DrawArgs& args)
{
    m_model.Draw(args.context, World(), args.objectCB);
}

void Player::Finalize()
{
    m_stateMachine.Clear();
    if (m_collider)
    {
        GetScene().Collisions().Remove(m_collider);
        m_collider = nullptr;
    }
    m_animator.Finalize();
    m_model.Finalize();
}

// 撞到静态物体时沿法线水平推出，实现贴墙滑动
void Player::OnCollision(GameObject&, const Contact& contact)
{
    if (contact.trigger || !(contact.other->layer & CollisionLayer::Static))
        return;
    Vector3 push = contact.normal * contact.depth;
    push.y = 0.0f;
    Move(push);
}

Vector3 Player::MoveDirection(const UpdateArgs& args) const
{
    Vector2 input;
    if (args.keys.W || autoRun) input.y += 1.0f;
    if (args.keys.S) input.y -= 1.0f;
    if (args.keys.D) input.x += 1.0f;
    if (args.keys.A) input.x -= 1.0f;

    // 右手坐标系，Y 朝上：相机水平前方与右方（autoRun 时沿世界 +Z，方便从任意相机角度截图）
    const auto* camera = GetScene().Get<ThirdPersonCamera>(m_camera);
    const float cameraYaw = autoRun || !camera ? 0.0f : camera->Yaw();
    Vector3 forward(std::sin(cameraYaw), 0.0f, std::cos(cameraYaw));
    Vector3 right = forward.Cross(Vector3::Up);
    Vector3 dir = forward * input.y + right * input.x;
    if (dir.LengthSquared() < 0.01f)
        return Vector3::Zero;
    dir.Normalize();
    return dir;
}

void Player::TurnTowards(const Vector3& dir, float dt)
{
    const float delta = WrapAngle(std::atan2(dir.x, dir.z) - m_yaw);
    const float step = turnSpeed * dt;
    m_yaw = WrapAngle(m_yaw + std::clamp(delta, -step, step));
}

void Player::Move(const Vector3& delta)
{
    m_position += delta;
    SyncCollider();
}

void Player::PlayAnimation(Anim anim)
{
    m_animator.Play(m_clips[static_cast<int>(anim)], blendTime);
}

void Player::SyncCollider()
{
    if (!m_collider)
        return;
    // 胶囊中轴从脚底上方 radius 处到头顶下方 radius 处
    m_collider->shape = CapsuleShape{ m_position + Vector3(0.0f, radius, 0.0f), height - 2.0f * radius, radius };
}

Matrix Player::World() const
{
    return Matrix::CreateRotationY(m_yaw) * Matrix::CreateTranslation(m_position);
}
