#pragma once
#include <BufferHelpers.h>
#include "GameObject.h"
#include "Graphics.h"
#include "Scene.h"

struct GameOptions
{
    bool autoRun = false;           // 模拟一直按住 W
    float cameraYawDeg = 0.0f;      // 初始相机朝向
};

// 游戏逻辑：搭建场景（生成对象），并在每帧驱动 Scene；窗口、设备与输入由 Application 提供
class Game
{
public:
    bool Initialize(const InitArgs& args, const GameOptions& options);
    void Update(const UpdateArgs& args);
    void Draw(ID3D11DeviceContext* context, float aspect);   // 渲染目标已由 Application 清屏并绑定
    void Finalize();

    const float* ClearColor() const { return m_clearColor; }

private:
    Scene m_scene;
    ObjectHandle m_camera;
    DirectX::ConstantBuffer<FrameConstants> m_frameCB;
    DirectX::ConstantBuffer<ObjectConstants> m_objectCB;
    float m_clearColor[4] = { 0.55f, 0.62f, 0.72f, 1.0f };   // 与雾颜色一致
};
