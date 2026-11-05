#pragma once

#include "ProcessManager.h"
#include "SystemMonitor.h"
#include <string>

class UIManager {
public:
    UIManager(ProcessManager* processManager, SystemMonitor* systemMonitor, ID3D11ShaderResourceView* appIcon = nullptr);
    ~UIManager();

    void Render();
    void SetAppIcon(ID3D11ShaderResourceView* appIcon) { m_appIcon = appIcon; }

    enum class Theme {
        LightBlue,
        LightPink
    };
    void SetTheme(Theme theme);

private:
    ProcessManager* m_processManager;
    SystemMonitor* m_systemMonitor;
    ID3D11ShaderResourceView* m_appIcon;
    std::string m_searchQuery;
    char m_searchBuffer[256];
    Theme m_currentTheme;
    bool m_themeChanged;
    
    // Config
    bool m_firstFrame;

    void RenderTopBar();
    void RenderSystemInfoPanel();
    void RenderProcessTable();
    
    // Helpers
    std::string WStringToString(const std::wstring& wstr);
    std::string FormatBytes(SIZE_T bytes);
};
