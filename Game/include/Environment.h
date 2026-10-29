#pragma once
#include <vector>
#include <SimpleMath.h>
#include "GameObject.h"

struct Collider;

// 测试场景：棋盘格地面 + 随机柱子作为参照物
class Environment : public GameObject
{
public:
    bool Initialize(const InitArgs& args) override;
    void Update(const UpdateArgs& args) override;
    void Draw(const DrawArgs& args) override;
    void Finalize() override;

private:
    struct Instance
    {
        DirectX::SimpleMath::Matrix world;
        DirectX::SimpleMath::Vector4 color;
        bool checker;
    };

    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    UINT m_indexCount = 0;
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11InputLayout> m_layout;
    ComPtr<ID3D11RasterizerState> m_rasterizer;
    std::vector<Instance> m_instances;
    std::vector<Collider*> m_colliders;    // 柱子的静态碰撞体
};
