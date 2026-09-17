#include "CaptureEngine.h"
#include <iostream>

CaptureEngine::CaptureEngine(ID3D11Device* device, ID3D11DeviceContext* context)
    : m_device(device), m_context(context) {}

CaptureEngine::~CaptureEngine() {
    ReleaseFrame();
    m_deskDupl.Reset();
}

bool CaptureEngine::InitializeDisplayCapture(uint32_t outputIndex) {
    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIOutput> dxgiOutput;
    hr = dxgiAdapter->EnumOutputs(outputIndex, &dxgiOutput);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIOutput1> dxgiOutput1;
    hr = dxgiOutput.As(&dxgiOutput1);
    if (FAILED(hr)) return false;

    // Initialize Desktop Duplication API
    hr = dxgiOutput1->DuplicateOutput(m_device.Get(), &m_deskDupl);
    return SUCCEEDED(hr);
}

bool CaptureEngine::AcquireNextFrame(ID3D11Texture2D** outTexture, uint32_t timeoutMs) {
    if (!m_deskDupl) return false;

    ReleaseFrame();

    DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
    ComPtr<IDXGIResource> desktopResource;

    HRESULT hr = m_deskDupl->AcquireNextFrame(timeoutMs, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // No new frame rendered yet, reuse previous texture if available
            if (m_acquiredDesktopImage) {
                *outTexture = m_acquiredDesktopImage.Get();
                (*outTexture)->AddRef();
                return true;
            }
        }
        return false;
    }

    m_hasAcquiredFrame = true;

    hr = desktopResource.As(&m_acquiredDesktopImage);
    if (FAILED(hr)) {
        m_deskDupl->ReleaseFrame();
        m_hasAcquiredFrame = false;
        return false;
    }

    *outTexture = m_acquiredDesktopImage.Get();
    (*outTexture)->AddRef();
    return true;
}

void CaptureEngine::ReleaseFrame() {
    if (m_hasAcquiredFrame && m_deskDupl) {
        m_deskDupl->ReleaseFrame();
        m_hasAcquiredFrame = false;
    }
}

std::vector<CaptureDevice> CaptureEngine::EnumerateDevices() {
    std::vector<CaptureDevice> devices;
    // Primary display output
    devices.push_back({ "display-0", "Primary Display (Display 1)", false });
    return devices;
}
