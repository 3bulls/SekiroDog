#pragma once
#include <SimpleMath.h>
#include "GameObject.h"

// 第三人称环绕相机：Update 里用鼠标（锁定时）控制视角，LateUpdate 里跟随玩家本帧的最终位置
class ThirdPersonCamera : public GameObject
{
public:
    void SetYaw(float yaw) { m_yaw = yaw; }

    bool Initialize(const InitArgs& args) override;
    void Update(const UpdateArgs& args) override;
    void LateUpdate(const UpdateArgs& args) override;
    void Draw(const DrawArgs& args) override;
    void Finalize() override;

    DirectX::SimpleMath::Matrix View() const;
    const DirectX::SimpleMath::Vector3& Position() const { return m_eye; }
    float Yaw() const { return m_yaw; }

    float distance = 2.5f;
    float pivotHeight = 0.6f;       // 注视点相对脚底的高度
    float sensitivity = 0.003f;     // 弧度 / 像素
    float minPitch = -0.5f;
    float maxPitch = 1.2f;

private:
    ObjectHandle m_target;
    float m_yaw = 0.0f;             // 与 Player 相同：0 = 看向 +Z
    float m_pitch = 0.25f;
    bool m_hasPivot = false;
    DirectX::SimpleMath::Vector3 m_pivot;
    DirectX::SimpleMath::Vector3 m_eye;
};
