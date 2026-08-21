#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <d3d11.h>

struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    std::wstring username;
    std::wstring fullPath;
    
    bool isNotResponding;
    bool hasMainWindow;
    
    double cpuUsage;
    SIZE_T memoryUsageBytes;
    
    ID3D11ShaderResourceView* iconTexture; // Weak ptr, managed by ProcessManager
    
    // Internal timing
    ULARGE_INTEGER lastSystemTime;
    ULARGE_INTEGER lastProcessTime;
    bool validTime;
};

class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

    void Init(ID3D11Device* device);
    void RefreshProcesses();
    const std::vector<ProcessInfo>& GetProcesses() const;
    
    static void KillProcess(DWORD pid);
    static void BringToFront(DWORD pid);
    static void OpenFileLocation(const std::wstring& path);

private:
    std::vector<ProcessInfo> m_processList;
    std::map<DWORD, ProcessInfo> m_processMap;
    std::map<std::wstring, ID3D11ShaderResourceView*> m_iconCache;
    
    ID3D11Device* m_d3dDevice;
    
    ID3D11ShaderResourceView* GetOrLoadIcon(const std::wstring& path);
    std::wstring GetProcessUsername(HANDLE hProcess);
    bool CheckNotResponding(DWORD pid);
    void CleanupIcons();
};
