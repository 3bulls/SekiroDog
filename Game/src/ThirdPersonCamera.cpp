#include "ThirdPersonCamera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace DirectX::SimpleMath;

void ThirdPersonCamera::Update(float dt, const Mouse::State& mouse, const Vector3& target)
{
    if (mouse.positionMode == Mouse::MODE_RELATIVE)
    {
        m_yaw -= static_cast<float>(mouse.x) * sensitivity;
        m_pitch = std::clamp(m_pitch + static_cast<float>(mouse.y) * sensitivity, minPitch, maxPitch);
    }

    Vector3 desired = target + Vector3(0.0f, pivotHeight, 0.0f);
    if (!m_hasPivot)
    {
        m_pivot = desired;
        m_hasPivot = true;
    }
    m_pivot = Vector3::Lerp(m_pivot, desired, 1.0f - std::exp(-20.0f * dt));

    Vector3 forward(std::sin(m_yaw) * std::cos(m_pitch), -std::sin(m_pitch), std::cos(m_yaw) * std::cos(m_pitch));
    m_eye = m_pivot - forward * distance;
    m_eye.y = std::max(m_eye.y, 0.1f);
}

Matrix ThirdPersonCamera::View() const
{
    return Matrix::CreateLookAt(m_eye, m_pivot, Vector3::Up);
}
