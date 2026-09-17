#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "D3D11Renderer.h"
#include "CaptureEngine.h"
#include "AudioMixer.h"
#include "HardwareEncoder.h"
#include "OutputRouter.h"

// Forward declaration of Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. Register Studio Window Class
    const wchar_t CLASS_NAME[] = L"VeloraStudioMainWindowClass";

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszClassName = CLASS_NAME;

    RegisterClassExW(&wcex);

    // 2. Create Window with Velora Obsidian style
    HWND hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        CLASS_NAME,
        L"Velora Studio Native — Broadcast Suite",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 3. Initialize Engine Components
    D3D11Renderer renderer;
    if (renderer.Initialize(hWnd, 1920, 1080)) {
        CaptureEngine capture(renderer.GetDevice(), renderer.GetContext());
        capture.InitializeDisplayCapture(0);

        AudioMixer audio;
        audio.Initialize(48000, 2);
        audio.Start();

        HardwareEncoder encoder;
        EncoderConfig encCfg;
        encCfg.bitrateKbps = 6000;
        encCfg.keyframeIntervalSec = 2; // Strict 2.0s GOP
        encCfg.bFrames = 0;             // 0 B-frames
        encoder.Initialize(renderer.GetDevice(), encCfg);

        // 4. Initialize OutputRouter with RTMP fan-out & local recording
        OutputRouter router;
        router.SetKeyframeRequestCallback([&encoder]() {
            encoder.RequestKeyframe();
        });

        auto veloraRTMP = std::make_shared<RTMPOutput>("velora-rtmp", "Velora RTMP", "rtmp://ingest.velora.tv/live", "live_key");
        veloraRTMP->Connect();
        router.RegisterOutput(veloraRTMP);

        // 5. Launch Studio UI Shell
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring path(exePath);
        size_t pos = path.find_last_of(L"\\/");
        std::wstring baseDir = (pos != std::wstring::npos) ? path.substr(0, pos) : L".";
        std::wstring htmlPath = baseDir + L"\\index.html";

        ShellExecuteW(NULL, L"open", htmlPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    // Main Message Loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
