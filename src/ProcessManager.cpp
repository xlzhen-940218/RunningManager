#include "ProcessManager.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <sddl.h>
#include <iostream>
#include <set>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wtsapi32.lib")

// Helper function to create ID3D11ShaderResourceView from HICON
ID3D11ShaderResourceView* ProcessManager::CreateTextureFromIcon(ID3D11Device* device, HICON hIcon) {
    if (!hIcon || !device) return nullptr;

    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return nullptr;

    BITMAP bmp;
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;
    if (width == 0 || height == 0) {
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        return nullptr;
    }

    HDC hDC = GetDC(nullptr);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint32_t> pixels(width * height, 0);
    GetDIBits(hDC, iconInfo.hbmColor, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);

    // Check if alpha channel is all zeros
    bool hasAlpha = false;
    for (int i = 0; i < width * height; ++i) {
        if ((pixels[i] & 0xFF000000) != 0) {
            hasAlpha = true;
            break;
        }
    }

    // If no alpha, get mask and apply it
    if (!hasAlpha && iconInfo.hbmMask) {
        std::vector<uint32_t> maskPixels(width * height, 0);
        GetDIBits(hDC, iconInfo.hbmMask, 0, height, maskPixels.data(), &bmi, DIB_RGB_COLORS);
        for (int i = 0; i < width * height; ++i) {
            if (maskPixels[i] == 0) {
                pixels[i] |= 0xFF000000;
            }
        }
    }

    ReleaseDC(nullptr, hDC);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = pixels.data();
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;

    ID3D11Texture2D* pTexture = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, &subResource, &pTexture);
    if (FAILED(hr)) return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* pSRV = nullptr;
    device->CreateShaderResourceView(pTexture, &srvDesc, &pSRV);
    pTexture->Release();

    return pSRV;
}

static uint64_t FileTimeToUInt64(const FILETIME& ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

ProcessManager::ProcessManager() : m_d3dDevice(nullptr) {}

ProcessManager::~ProcessManager() {
    CleanupIcons();
}

void ProcessManager::Init(ID3D11Device* device) {
    m_d3dDevice = device;
    EnableDebugPrivilege();
}

void ProcessManager::CleanupIcons() {
    for (auto& pair : m_iconCache) {
        if (pair.second) {
            pair.second->Release();
        }
    }
    m_iconCache.clear();
}

ID3D11ShaderResourceView* ProcessManager::GetOrLoadIcon(const std::wstring& path) {
    if (path.empty()) return nullptr;
    auto it = m_iconCache.find(path);
    if (it != m_iconCache.end()) {
        return it->second;
    }

    HICON hIcon = nullptr;
    ExtractIconExW(path.c_str(), 0, nullptr, &hIcon, 1);
    if (!hIcon) {
        SHFILEINFOW sfi = { 0 };
        SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON);
        hIcon = sfi.hIcon;
    }

    ID3D11ShaderResourceView* srv = nullptr;
    if (hIcon) {
        srv = CreateTextureFromIcon(m_d3dDevice, hIcon);
        DestroyIcon(hIcon);
    }
    
    m_iconCache[path] = srv;
    return srv;
}

std::wstring ProcessManager::GetProcessUsername(HANDLE hProcess) {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        return L"";
    }

    DWORD dwSize = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &dwSize);
    if (dwSize == 0) {
        CloseHandle(hToken);
        return L"";
    }

    std::vector<BYTE> buffer(dwSize);
    if (!GetTokenInformation(hToken, TokenUser, buffer.data(), dwSize, &dwSize)) {
        CloseHandle(hToken);
        return L"";
    }

    PTOKEN_USER pTokenUser = (PTOKEN_USER)buffer.data();
    SID_NAME_USE SidType;
    wchar_t name[256];
    DWORD cchName = 256;
    wchar_t domain[256];
    DWORD cchDomain = 256;

    if (LookupAccountSidW(nullptr, pTokenUser->User.Sid, name, &cchName, domain, &cchDomain, &SidType)) {
        CloseHandle(hToken);
        return std::wstring(name);
    }

    CloseHandle(hToken);
    return L"";
}

bool ProcessManager::CheckIsAdmin(HANDLE hProcess) {
    if (!hProcess) return false;
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        return false;
    }

    TOKEN_ELEVATION elevation = {};
    DWORD dwSize = sizeof(TOKEN_ELEVATION);
    bool isElevated = false;
    if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
        isElevated = (elevation.TokenIsElevated != 0);
    }

    CloseHandle(hToken);
    return isElevated;
}

void ProcessManager::EnableDebugPrivilege() {
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        TOKEN_PRIVILEGES tp = {};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &tp.Privileges[0].Luid)) {
            AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        }
        CloseHandle(hToken);
    }
}

bool ProcessManager::CheckNotResponding(DWORD pid) {
    // This is a simplified check. A full implementation would find the main window
    // and use SendMessageTimeout with WM_NULL to see if it responds.
    return false; 
}

static BOOL CALLBACK EnumWindowsCollectProc(HWND hWnd, LPARAM lParam) {
    if (IsWindowVisible(hWnd) && GetWindow(hWnd, GW_OWNER) == nullptr) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hWnd, &pid);
        if (pid != 0) {
            std::set<DWORD>* pidSet = (std::set<DWORD>*)lParam;
            pidSet->insert(pid);
        }
    }
    return TRUE;
}

void ProcessManager::RefreshProcesses() {
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return;

    std::set<DWORD> pidsWithWindows;
    EnumWindows(EnumWindowsCollectProc, (LPARAM)&pidsWithWindows);

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return;
    }

    std::map<DWORD, ProcessInfo> newProcessMap;

    do {
        ProcessInfo info = {};
        info.pid = pe32.th32ProcessID;
        info.name = pe32.szExeFile;
        info.validTime = false;
        info.cpuUsage = 0.0;
        info.memoryUsageBytes = 0;
        info.hasMainWindow = (pidsWithWindows.find(info.pid) != pidsWithWindows.end());

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, info.pid);
        if (!hProcess) {
            hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, info.pid);
        }

        if (hProcess) {
            // Get Path
            wchar_t path[MAX_PATH];
            DWORD pathLen = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, path, &pathLen)) {
                info.fullPath = path;
            }

            // Get Memory
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                info.memoryUsageBytes = pmc.WorkingSetSize;
            }

            // Get Username
            info.username = GetProcessUsername(hProcess);

            // Get Admin Status
            info.isAdmin = CheckIsAdmin(hProcess);

            // CPU Usage calculation
            FILETIME ftCreation, ftExit, ftKernel, ftUser;
            FILETIME sysIdle, sysKernel, sysUser;
            if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser) && 
                GetSystemTimes(&sysIdle, &sysKernel, &sysUser)) {
                
                info.lastSystemTime.QuadPart = FileTimeToUInt64(sysKernel) + FileTimeToUInt64(sysUser);
                info.lastProcessTime.QuadPart = FileTimeToUInt64(ftKernel) + FileTimeToUInt64(ftUser);
                info.validTime = true;

                // Check if we have previous data
                auto it = m_processMap.find(info.pid);
                if (it != m_processMap.end() && it->second.validTime) {
                    uint64_t sysDiff = info.lastSystemTime.QuadPart - it->second.lastSystemTime.QuadPart;
                    uint64_t procDiff = info.lastProcessTime.QuadPart - it->second.lastProcessTime.QuadPart;
                    if (sysDiff > 0) {
                        info.cpuUsage = (double)procDiff / (double)sysDiff * 100.0;
                        // Adjust for number of logical processors (or not, if we want overall CPU %)
                        // We'll leave it as overall % for all cores.
                    }
                }
            }
            
            CloseHandle(hProcess);
        }

        // Icon
        info.iconTexture = GetOrLoadIcon(info.fullPath);
        
        info.isNotResponding = CheckNotResponding(info.pid);

        newProcessMap[info.pid] = info;

    } while (Process32NextW(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    m_processMap = std::move(newProcessMap);
    
    // Build list
    m_processList.clear();
    m_processList.reserve(m_processMap.size());
    for (const auto& pair : m_processMap) {
        m_processList.push_back(pair.second);
    }
}

const std::vector<ProcessInfo>& ProcessManager::GetProcesses() const {
    return m_processList;
}

void ProcessManager::KillProcess(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) {
        TerminateProcess(hProcess, 1);
        CloseHandle(hProcess);
    }
}

struct EnumWindowsData {
    DWORD pid;
    HWND hWnd;
};

static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    EnumWindowsData* data = (EnumWindowsData*)lParam;
    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);
    if (pid == data->pid && IsWindowVisible(hWnd)) {
        if (GetWindow(hWnd, GW_OWNER) == nullptr) {
            data->hWnd = hWnd;
            return FALSE; // Stop enumerating, found best window
        }
        if (data->hWnd == nullptr) {
            data->hWnd = hWnd; // Fallback
        }
    }
    return TRUE;
}

void ProcessManager::BringToFront(DWORD pid) {
    EnumWindowsData data = { pid, nullptr };
    EnumWindows(EnumWindowsProc, (LPARAM)&data);
    
    if (data.hWnd) {
        if (IsIconic(data.hWnd)) {
            ShowWindow(data.hWnd, SW_RESTORE);
        } else {
            ShowWindow(data.hWnd, SW_SHOW);
        }
        
        // Force window to foreground using AttachThreadInput trick
        HWND hCurWnd = GetForegroundWindow();
        DWORD dwMyID = GetCurrentThreadId();
        DWORD dwCurID = GetWindowThreadProcessId(hCurWnd, NULL);
        
        if (dwMyID != dwCurID) {
            AttachThreadInput(dwCurID, dwMyID, TRUE);
            SetWindowPos(data.hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
            SetWindowPos(data.hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
            SetForegroundWindow(data.hWnd);
            SetFocus(data.hWnd);
            SetActiveWindow(data.hWnd);
            AttachThreadInput(dwCurID, dwMyID, FALSE);
        } else {
            SetForegroundWindow(data.hWnd);
            SetFocus(data.hWnd);
            SetActiveWindow(data.hWnd);
        }
    }
}

void ProcessManager::OpenFileLocation(const std::wstring& path) {
    if (path.empty()) return;
    std::wstring cmd = L"/select,\"" + path + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd.c_str(), nullptr, SW_SHOWNORMAL);
}
