#pragma once
#include <string>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;

// 与 shaders/Common.hlsli 中的常量缓冲布局保持一致（矩阵上传前需转置）
struct FrameConstants
{
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;
    DirectX::XMFLOAT3 lightDir;
    float pad0;
    DirectX::XMFLOAT3 cameraPos;
    float pad1;
};

struct ObjectConstants
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT4 params;   // x: 1 = 棋盘格地面
};

// D3D11 设备、交换链与深度缓冲
class Graphics
{
public:
    bool Initialize(HWND hwnd, int width, int height);
    void Resize(int width, int height);
    void BeginFrame(const float clearColor[4]);
    void Present();

    ID3D11Device* Device() const { return m_device.Get(); }
    ID3D11DeviceContext* Context() const { return m_context.Get(); }
    ID3D11Texture2D* BackBuffer() const { return m_backBuffer.Get(); }
    int Width() const { return m_width; }
    int Height() const { return m_height; }

private:
    void CreateSizeDependentResources();

    HWND m_hwnd = nullptr;
    int m_width = 1;
    int m_height = 1;
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID3D11Texture2D> m_backBuffer;
    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11DepthStencilView> m_dsv;
};

// 运行时编译 HLSL，失败时返回 nullptr 并写日志
ComPtr<ID3DBlob> CompileShader(const std::wstring& file, const char* entry, const char* target);
