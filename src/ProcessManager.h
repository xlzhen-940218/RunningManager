#pragma once

#include "imgui.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <d3d11.h>
#endif

// All strings are UTF-8 on every platform.
struct ProcessInfo {
    uint32_t    pid = 0;
    std::string name;
    std::string username;
    std::string fullPath;

    bool isNotResponding = false;
    bool hasMainWindow  = false;
    bool isAdmin        = false;

    double   cpuUsage         = 0.0;
    uint64_t memoryUsageBytes = 0;

    // Native process state ('R', 'S', 'D', 'Z', 'T', ...). 0 = not available
    // (Windows reports its own Running/Not Responding state instead).
    char state = 0;

    // Weak reference; owned by ProcessManager. May be 0 when the platform has
    // no executable icon (Linux).
    ImTextureID iconTexture = (ImTextureID)0;

    // Internal CPU sampling data.
    uint64_t lastSystemTime  = 0;
    uint64_t lastProcessTime = 0;
    bool     validTime       = false;
};

class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

#ifdef _WIN32
    void Init(ID3D11Device* device);
    static ImTextureID CreateTextureFromIcon(ID3D11Device* device, HICON hIcon);
#else
    void Init();
#endif

    void RefreshProcesses();
    const std::vector<ProcessInfo>& GetProcesses() const;

    static void KillProcess(uint32_t pid);
    static void BringToFront(uint32_t pid);
    static void OpenFileLocation(const std::string& path);

private:
    std::vector<ProcessInfo>        m_processList;
    std::map<uint32_t, ProcessInfo> m_processMap;

#ifdef _WIN32
    std::map<std::wstring, ImTextureID> m_iconCache;
    ID3D11Device*                       m_d3dDevice = nullptr;

    ImTextureID  GetOrLoadIcon(const std::wstring& path);
    std::string  GetProcessUsername(HANDLE hProcess);
    bool         CheckIsAdmin(HANDLE hProcess);
    static void  EnableDebugPrivilege();
    bool         CheckNotResponding(uint32_t pid);
    void         CleanupIcons();
#else
    // Total jiffies from the previous /proc/stat sample, used for CPU deltas.
    uint64_t m_prevTotalJiffies = 0;
#endif
};
