#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <memory>

using Microsoft::WRL::ComPtr;

struct RenderLayer {
    std::string id;
    std::string name;
    ComPtr<ID3D11ShaderResourceView> textureSRV;
    float x = 0.0f;
    float y = 0.0f;
    float width = 1920.0f;
    float height = 1080.0f;
    float opacity = 1.0f;
    bool visible = true;
};

class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    bool Initialize(HWND hWnd, uint32_t width = 1920, uint32_t height = 1080);
    void BeginFrame();
    void RenderLayers(const std::vector<RenderLayer>& layers);
    void EndFrame();
    void Resize(uint32_t width, uint32_t height);

    ID3D11Device* GetDevice() const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }
    ID3D11Texture2D* GetRenderTargetTexture() const { return m_renderTargetTexture.Get(); }

private:
    bool CreateDeviceAndSwapChain(HWND hWnd);
    bool CreateRenderTargets();
    bool CreateShadersAndQuad();

    uint32_t m_width = 1920;
    uint32_t m_height = 1080;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<ID3D11Texture2D> m_renderTargetTexture;

    // Quad rendering for scene layers
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11SamplerState> m_samplerState;
    ComPtr<ID3D11BlendState> m_blendState;
};
