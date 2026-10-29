#pragma once
#include <string>
#include <vector>
#include <BufferHelpers.h>
#include <SimpleMath.h>
#include "Graphics.h"

// 测试场景：棋盘格地面 + 随机柱子作为参照物
class Environment
{
public:
    bool Initialize(ID3D11Device* device, const std::wstring& shaderPath);
    void Draw(ID3D11DeviceContext* context, DirectX::ConstantBuffer<ObjectConstants>& objectCB);

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
};
