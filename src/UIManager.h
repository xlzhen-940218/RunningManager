#pragma once

#include "ProcessManager.h"
#include "SystemMonitor.h"

#include <string>

// Fonts loaded once at start-up and shared by the UI. Any entry may be null,
// in which case the corresponding text falls back to the default font.
struct UIFonts {
    ImFont* body = nullptr;
    ImFont* title = nullptr;
    // Named "caption" rather than "small": <rpcndr.h> defines "small" as a
    // macro expanding to char on Windows, which breaks this declaration.
    ImFont* caption = nullptr;
};

class UIManager {
public:
    UIManager(ProcessManager* processManager, SystemMonitor* systemMonitor,
              ImTextureID appIcon, const UIFonts& fonts, float uiScale);
    ~UIManager();

    void Render();
    void SetAppIcon(ImTextureID appIcon) { m_appIcon = appIcon; }

    enum class Theme {
        LightBlue,
        LightPink,
        Dark
    };
    void SetTheme(Theme theme);

    // User settings (sidebar width, process sort order and theme) are stored as
    // KEY=VALUE lines in ~/.runningmanager/.env so they survive restarts.
    void LoadSettings();
    void SaveSettings();

    // Loads body/title/small font sizes (and CJK coverage) from fonts available
    // on the host. Safe to call before the platform backend is initialised.
    static UIFonts LoadFonts(ImGuiIO& io, float uiScale);

    // Colour used behind the whole window (also used to clear the framebuffer).
    ImVec4 GetBackgroundColor() const;

private:
    ProcessManager* m_processManager;
    SystemMonitor*  m_systemMonitor;
    ImTextureID     m_appIcon;
    UIFonts         m_fonts;
    float           m_scale;
    std::string     m_searchQuery;
    char            m_searchBuffer[256] = {};
    Theme           m_currentTheme;
    bool            m_themeChanged;
    bool            m_isDark;
    int             m_selectedPid;
    float           m_sidebarWidth;
    // Persisted process sort order (column index + direction), applied to the
    // table through its default-sort flags on start-up.
    int             m_sortColumn;
    bool            m_sortDescending;

    float MinSidebarWidth() const;
    float MaxSidebarWidth(float availableWidth) const;

    void ApplyTheme();

    void RenderBanner();
    void DrawThemeSwatches();
    void RenderSidebar();
    void RenderContent();
    void RenderTopBar();
    void RenderProcessTable();

    void DrawGauge(const char* label, double percentage, const ImVec4& color,
                   const std::string& detail, bool available, float cellWidth);
    void DrawWrappedCenteredText(float left, float width, const std::string& text,
                                 const ImVec4& color);
    void DrawStatusPill(const char* text, const ImVec4& color);
    void DrawCountChip(int count);

    std::string FormatBytes(uint64_t bytes) const;
    std::string Ellipsize(const std::string& text, float maxWidth) const;
    const char* StatusText(const ProcessInfo& proc, ImVec4& color) const;
    bool MatchesSearch(const ProcessInfo& proc) const;

    static void DrawCenteredText(const char* text);
    static void DrawCenteredTextColored(const ImVec4& color, const char* text);
    static void DrawRightAlignedText(const char* text);
    static void DrawRightAlignedTextColored(const ImVec4& color, const char* text);
    void DrawAppIconFallback(float size) const;
    void DrawProcessIconFallback(const ProcessInfo& proc, float size) const;
};
