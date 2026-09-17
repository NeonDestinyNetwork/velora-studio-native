#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <functional>

using Microsoft::WRL::ComPtr;

struct CaptureDevice {
    std::string id;
    std::string name;
    bool isCamera;
};

class CaptureEngine {
public:
    CaptureEngine(ID3D11Device* device, ID3D11DeviceContext* context);
    ~CaptureEngine();

    bool InitializeDisplayCapture(uint32_t outputIndex = 0);
    bool InitializeCameraCapture(const std::string& deviceSymbolicLink);
    
    // Acquire latest frame into a D3D11 Texture
    bool AcquireNextFrame(ID3D11Texture2D** outTexture, uint32_t timeoutMs = 16);
    void ReleaseFrame();

    std::vector<CaptureDevice> EnumerateDevices();

private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGIOutputDuplication> m_deskDupl;
    ComPtr<ID3D11Texture2D> m_acquiredDesktopImage;
    bool m_hasAcquiredFrame = false;
};
