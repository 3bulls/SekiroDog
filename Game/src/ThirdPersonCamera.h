#pragma once
#include <Mouse.h>
#include <SimpleMath.h>

// 第三人称环绕相机：鼠标锁定（相对模式）时控制视角
class ThirdPersonCamera
{
public:
    void Update(float dt, const DirectX::Mouse::State& mouse, const DirectX::SimpleMath::Vector3& target);
    void SetYaw(float yaw) { m_yaw = yaw; }

    DirectX::SimpleMath::Matrix View() const;
    const DirectX::SimpleMath::Vector3& Position() const { return m_eye; }
    float Yaw() const { return m_yaw; }

    float distance = 2.5f;
    float pivotHeight = 0.6f;       // 注视点相对脚底的高度
    float sensitivity = 0.003f;     // 弧度 / 像素
    float minPitch = -0.5f;
    float maxPitch = 1.2f;

private:
    float m_yaw = 0.0f;             // 与 Player 相同：0 = 看向 +Z
    float m_pitch = 0.25f;
    bool m_hasPivot = false;
    DirectX::SimpleMath::Vector3 m_pivot;
    DirectX::SimpleMath::Vector3 m_eye;
};
