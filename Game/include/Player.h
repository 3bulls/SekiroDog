#pragma once
#include <Keyboard.h>
#include <SimpleMath.h>

// 玩家移动：W/A/S/D 相对相机方向奔跑，松开回到待机。
// 跑步动画是原地动画（In Place），位移由代码驱动。
class Player
{
public:
    void Update(float dt, const DirectX::Keyboard::State& keys, float cameraYaw, bool forceRun);

    DirectX::SimpleMath::Matrix World() const;
    const DirectX::SimpleMath::Vector3& Position() const { return m_position; }
    float Yaw() const { return m_yaw; }

    float RunBlend() const { return m_runBlend; }    // 0 = 待机，1 = 奔跑
    float IdleTime() const { return m_idleTime; }
    float RunTime() const { return m_runTime; }

    float runSpeed = 2.5f;                  // 与跑步动画步幅匹配（米/秒）
    float turnSpeed = DirectX::XM_2PI * 2;  // 720 度/秒
    float blendTime = 0.15f;                // 待机 <-> 奔跑 过渡时间

private:
    DirectX::SimpleMath::Vector3 m_position;
    float m_yaw = 0.0f;                     // 0 = 面朝 +Z
    float m_runBlend = 0.0f;
    float m_idleTime = 0.0f;
    float m_runTime = 0.0f;
};
