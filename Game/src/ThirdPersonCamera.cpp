#include "ThirdPersonCamera.h"

#include <algorithm>
#include <cmath>
#include "Player.h"
#include "Scene.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

bool ThirdPersonCamera::Initialize(const InitArgs&)
{
    // 没有玩家时停在原点
    if (auto* player = GetScene().Find<Player>())
        m_target = player->Handle();
    m_hasPivot = false;
    return true;
}

void ThirdPersonCamera::Update(const UpdateArgs& args)
{
    const Mouse::State& mouse = args.mouse;
    if (mouse.positionMode == Mouse::MODE_RELATIVE)
    {
        m_yaw -= static_cast<float>(mouse.x) * sensitivity;
        m_pitch = std::clamp(m_pitch + static_cast<float>(mouse.y) * sensitivity, minPitch, maxPitch);
    }
}

// 在碰撞修正之后跟随，拿到的是玩家本帧的最终位置
void ThirdPersonCamera::LateUpdate(const UpdateArgs& args)
{
    const auto* player = GetScene().Get<Player>(m_target);
    const Vector3 target = player ? player->Position() : Vector3::Zero;

    Vector3 desired = target + Vector3(0.0f, pivotHeight, 0.0f);
    if (!m_hasPivot)
    {
        m_pivot = desired;
        m_hasPivot = true;
    }
    m_pivot = Vector3::Lerp(m_pivot, desired, 1.0f - std::exp(-20.0f * args.dt));

    Vector3 forward(std::sin(m_yaw) * std::cos(m_pitch), -std::sin(m_pitch), std::cos(m_yaw) * std::cos(m_pitch));
    m_eye = m_pivot - forward * distance;
    m_eye.y = std::max(m_eye.y, 0.1f);
}

// 相机本身不描画，视图矩阵由 Game 写入帧常量
void ThirdPersonCamera::Draw(const DrawArgs&)
{
}

void ThirdPersonCamera::Finalize()
{
    m_target = {};
    m_hasPivot = false;
}

Matrix ThirdPersonCamera::View() const
{
    return Matrix::CreateLookAt(m_eye, m_pivot, Vector3::Up);
}
