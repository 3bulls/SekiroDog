#include "Graphics.h"

#include <algorithm>
#include <d3dcompiler.h>
#include "Log.h"

bool Graphics::Initialize(HWND hwnd, int width, int height)
{
    m_hwnd = hwnd;
    m_width = std::max(width, 1);
    m_height = std::max(height, 1);

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, _countof(levels),
                                   D3D11_SDK_VERSION, &m_device, nullptr, &m_context);
    if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG))
    {
        // 未安装调试层（Graphics Tools）时退回普通设备
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, _countof(levels),
                               D3D11_SDK_VERSION, &m_device, nullptr, &m_context);
    }
    if (FAILED(hr))
    {
        Log("D3D11CreateDevice failed: 0x%08X", static_cast<unsigned>(hr));
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    m_device.As(&dxgiDevice);
    dxgiDevice->GetAdapter(&adapter);
    adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    hr = factory->CreateSwapChainForHwnd(m_device.Get(), hwnd, &desc, nullptr, nullptr, &m_swapChain);
    if (FAILED(hr))
    {
        Log("CreateSwapChainForHwnd failed: 0x%08X", static_cast<unsigned>(hr));
        return false;
    }
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    CreateSizeDependentResources();
    return true;
}

void Graphics::Finalize()
{
    if (m_context)
    {
        m_context->ClearState();
        m_context->Flush();
    }
    m_dsv.Reset();
    m_rtv.Reset();
    m_backBuffer.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
    m_hwnd = nullptr;
}

void Graphics::Resize(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (!m_swapChain || (width == m_width && height == m_height))
        return;

    m_width = width;
    m_height = height;
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv.Reset();
    m_dsv.Reset();
    m_backBuffer.Reset();
    m_context->Flush();
    m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
    CreateSizeDependentResources();
}

void Graphics::CreateSizeDependentResources()
{
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&m_backBuffer));

    // 以 sRGB 视图写入，着色器里全部按线性空间计算
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    m_device->CreateRenderTargetView(m_backBuffer.Get(), &rtvDesc, &m_rtv);

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = m_width;
    depthDesc.Height = m_height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;
    m_device->CreateTexture2D(&depthDesc, nullptr, &depth);
    m_device->CreateDepthStencilView(depth.Get(), nullptr, &m_dsv);
}

void Graphics::BeginFrame(const float clearColor[4])
{
    m_context->ClearRenderTargetView(m_rtv.Get(), clearColor);
    m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());

    D3D11_VIEWPORT vp = { 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
    m_context->RSSetViewports(1, &vp);
}

void Graphics::Present()
{
    m_swapChain->Present(1, 0);
}

ComPtr<ID3DBlob> CompileShader(const std::wstring& file, const char* entry, const char* target)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    ComPtr<ID3DBlob> code, errors;
    HRESULT hr = D3DCompileFromFile(file.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                    entry, target, flags, 0, &code, &errors);
    if (FAILED(hr))
    {
        Log("Shader compile failed (%ls, %s): %s", file.c_str(), entry,
            errors ? static_cast<const char*>(errors->GetBufferPointer()) : "file not found");
        return nullptr;
    }
    return code;
}
