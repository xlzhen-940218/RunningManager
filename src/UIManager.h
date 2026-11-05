#pragma once

#include "ProcessManager.h"
#include "SystemMonitor.h"

#include <string>

class UIManager {
public:
    UIManager(ProcessManager* processManager, SystemMonitor* systemMonitor,
              ImTextureID appIcon = (ImTextureID)0);
    ~UIManager();

    void Render();
    void SetAppIcon(ImTextureID appIcon) { m_appIcon = appIcon; }

    enum class Theme {
        LightBlue,
        LightPink
    };
    void SetTheme(Theme theme);

private:
    ProcessManager* m_processManager;
    SystemMonitor*  m_systemMonitor;
    ImTextureID     m_appIcon;
    std::string     m_searchQuery;
    char            m_searchBuffer[256] = {};
    Theme           m_currentTheme;
    bool            m_themeChanged;

    bool m_firstFrame;

    void RenderTopBar();
    void RenderSystemInfoPanel();
    void RenderProcessTable();

    std::string FormatBytes(uint64_t bytes) const;
    static const char* StatusText(const ProcessInfo& proc, ImVec4& color);
    static void DrawCenteredText(const char* text);
    static void DrawCenteredTextColored(const ImVec4& color, const char* text);
    void DrawAppIconFallback(float size) const;
    void DrawProcessIconFallback(const ProcessInfo& proc, float size) const;
};
