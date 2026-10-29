#include "Player.h"

#include <algorithm>
#include <cmath>

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

void Player::Update(float dt, const Keyboard::State& keys, float cameraYaw, bool forceRun)
{
    Vector2 input;
    if (keys.W || forceRun) input.y += 1.0f;
    if (keys.S) input.y -= 1.0f;
    if (keys.D) input.x += 1.0f;
    if (keys.A) input.x -= 1.0f;

    // 右手坐标系，Y 朝上：相机水平前方与右方
    Vector3 forward(std::sin(cameraYaw), 0.0f, std::cos(cameraYaw));
    Vector3 right = forward.Cross(Vector3::Up);
    Vector3 dir = forward * input.y + right * input.x;
    const bool running = dir.LengthSquared() > 0.01f;

    if (running)
    {
        dir.Normalize();
        float delta = WrapAngle(std::atan2(dir.x, dir.z) - m_yaw);
        float step = turnSpeed * dt;
        m_yaw = WrapAngle(m_yaw + std::clamp(delta, -step, step));
        m_position += dir * runSpeed * dt;

        if (m_runBlend <= 0.0f)
            m_runTime = 0.0f;   // 每次起跑从动画开头播放
    }

    float target = running ? 1.0f : 0.0f;
    float rate = dt / blendTime;
    m_runBlend = std::clamp(m_runBlend + std::clamp(target - m_runBlend, -rate, rate), 0.0f, 1.0f);
    m_idleTime += dt;
    m_runTime += dt;
}

Matrix Player::World() const
{
    return Matrix::CreateRotationY(m_yaw) * Matrix::CreateTranslation(m_position);
}
