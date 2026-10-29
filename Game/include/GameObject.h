#pragma once
#include <cstdint>
#include <filesystem>
#include <BufferHelpers.h>
#include <Keyboard.h>
#include <Mouse.h>
#include "Graphics.h"

class Scene;
struct Contact;

// 对象句柄：槽位下标 + 代数。对象销毁后代数递增，旧句柄自动失效，不会变成野指针
struct ObjectHandle
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;

    bool IsValid() const { return index != UINT32_MAX; }
    bool operator==(const ObjectHandle&) const = default;
};

// 初始化时可用的资源
struct InitArgs
{
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    std::filesystem::path shaderDir;    // Game/shaders
    std::filesystem::path assetDir;     // Assets
};

// 每帧更新的输入
struct UpdateArgs
{
    float dt;
    const DirectX::Keyboard::State& keys;
    const DirectX::Mouse::State& mouse;
};

// 描画时可用的资源（帧常量 b0 已由 Game 绑定）
struct DrawArgs
{
    ID3D11DeviceContext* context;
    DirectX::ConstantBuffer<ObjectConstants>& objectCB;
};

// 游戏对象的统一生命周期，由 Scene 驱动。每帧顺序：
//   Update → 碰撞检测（OnCollision）→ 事件分发 → LateUpdate → 增删对象 → Draw
class GameObject
{
public:
    virtual ~GameObject() = default;

    virtual bool Initialize(const InitArgs& args) = 0;
    virtual void Update(const UpdateArgs& args) = 0;
    virtual void LateUpdate(const UpdateArgs&) {}       // 依赖其他对象本帧最终状态的逻辑（如相机跟随）
    virtual void Draw(const DrawArgs& args) = 0;
    virtual void Finalize() = 0;

    // 碰撞回调：contact.normal 指向把自己推离 other 的方向
    virtual void OnCollision(GameObject& /*other*/, const Contact& /*contact*/) {}

    ObjectHandle Handle() const { return m_handle; }

protected:
    Scene& GetScene() const { return *m_scene; }

private:
    friend class Scene;
    Scene* m_scene = nullptr;
    ObjectHandle m_handle;
};
