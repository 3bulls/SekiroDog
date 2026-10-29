#include "Environment.h"

#include <algorithm>
#include <random>
#include <GeometricPrimitive.h>
#include <VertexTypes.h>
#include "Log.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

bool Environment::Initialize(ID3D11Device* device, const std::wstring& shaderPath)
{
    // 所有物体共用一个单位立方体
    GeometricPrimitive::VertexCollection vertices;
    GeometricPrimitive::IndexCollection indices;
    GeometricPrimitive::CreateBox(vertices, indices, XMFLOAT3(1.0f, 1.0f, 1.0f));
    if (FAILED(CreateStaticBuffer(device, vertices, D3D11_BIND_VERTEX_BUFFER, &m_vertexBuffer)) ||
        FAILED(CreateStaticBuffer(device, indices, D3D11_BIND_INDEX_BUFFER, &m_indexBuffer)))
        return false;
    m_indexCount = static_cast<UINT>(indices.size());

    auto vsCode = CompileShader(shaderPath, "VSMain", "vs_5_0");
    auto psCode = CompileShader(shaderPath, "PSMain", "ps_5_0");
    if (!vsCode || !psCode)
        return false;
    device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &m_vs);
    device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &m_ps);
    device->CreateInputLayout(VertexPositionNormalTexture::InputElements, VertexPositionNormalTexture::InputElementCount,
                              vsCode->GetBufferPointer(), vsCode->GetBufferSize(), &m_layout);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&rd, &m_rasterizer);

    m_instances.push_back({ Matrix::CreateScale(200.0f, 0.1f, 200.0f) * Matrix::CreateTranslation(0.0f, -0.05f, 0.0f),
                            Vector4(0.30f, 0.30f, 0.30f, 1.0f), true });

    std::mt19937 rng(7);
    std::uniform_real_distribution<float> pos(-20.0f, 20.0f), height(0.5f, 3.0f);
    for (int i = 0; i < 24; ++i)
    {
        Vector2 p(pos(rng), pos(rng));
        if (p.Length() < 3.0f)
            p = p * (3.0f / std::max(p.Length(), 0.01f));
        float h = height(rng);
        m_instances.push_back({ Matrix::CreateScale(0.6f, h, 0.6f) * Matrix::CreateTranslation(p.x, h * 0.5f, p.y),
                                Vector4(0.35f, 0.28f, 0.22f, 1.0f), false });
    }
    return true;
}

void Environment::Draw(ID3D11DeviceContext* context, ConstantBuffer<ObjectConstants>& objectCB)
{
    const UINT stride = sizeof(VertexPositionNormalTexture), offset = 0;
    context->IASetInputLayout(m_layout.Get());
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->RSSetState(m_rasterizer.Get());
    context->VSSetShader(m_vs.Get(), nullptr, 0);
    context->PSSetShader(m_ps.Get(), nullptr, 0);

    ID3D11Buffer* cb = objectCB.GetBuffer();
    context->VSSetConstantBuffers(1, 1, &cb);
    context->PSSetConstantBuffers(1, 1, &cb);

    for (const Instance& inst : m_instances)
    {
        ObjectConstants object = {};
        XMStoreFloat4x4(&object.world, XMMatrixTranspose(inst.world));
        object.color = inst.color;
        object.params = { inst.checker ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
        objectCB.SetData(context, object);
        context->DrawIndexed(m_indexCount, 0, 0);
    }
}
