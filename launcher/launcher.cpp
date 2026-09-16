#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        return 1;
    }

    std::wstring path(exePath);
    size_t pos = path.find_last_of(L"\\/");
    std::wstring baseDir = (pos != std::wstring::npos) ? path.substr(0, pos) : L".";
    std::wstring workDir = baseDir + L"\\bin\\64bit";
    std::wstring targetExe = workDir + L"\\VeloraStudio.exe";

    // Fallback if VeloraStudio.exe not found
    DWORD attribs = GetFileAttributesW(targetExe.c_str());
    if (attribs == INVALID_FILE_ATTRIBUTES) {
        targetExe = workDir + L"\\PRISMLiveStudio.exe";
        attribs = GetFileAttributesW(targetExe.c_str());
        if (attribs == INVALID_FILE_ATTRIBUTES) {
            targetExe = workDir + L"\\obs64.exe";
        }
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Launch with correct working directory (bin/64bit)
    if (CreateProcessW(targetExe.c_str(), GetCommandLineW(), NULL, NULL, FALSE, 0, NULL, workDir.c_str(), &si, &pi)) {
        // Wait up to 2 seconds to see if it immediately crashes or fails initialization
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 2000);
        if (waitResult == WAIT_OBJECT_0) {
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            if (exitCode != 0) {
                std::wstring msg = L"Velora Studio engine exited with code: 0x" + std::to_wstring(exitCode) +
                                   L"\n\nPlease verify that graphics drivers and DirectX 11+ runtime are up to date.";
                MessageBoxW(NULL, msg.c_str(), L"Velora Studio - Startup Error", MB_OK | MB_ICONERROR);
            }
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    }

    DWORD err = GetLastError();
    std::wstring errMsg = L"Failed to start Velora Studio engine from bin\\64bit (Error: " + std::to_wstring(err) +
                          L").\nPlease ensure all package files are fully extracted.";
    MessageBoxW(NULL, errMsg.c_str(), L"Velora Studio", MB_OK | MB_ICONERROR);
    return 1;
}
