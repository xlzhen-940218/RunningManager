#include "UIManager.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct FontFile {
    const char* path;
    int         index;
};

#ifdef _WIN32
const FontFile kBodyFonts[] = {
    {"C:\\Windows\\Fonts\\segoeui.ttf", 0},
    {"C:\\Windows\\Fonts\\tahoma.ttf", 0},
};
const FontFile kTitleFonts[] = {
    {"C:\\Windows\\Fonts\\segoeuib.ttf", 0},
    {"C:\\Windows\\Fonts\\segoeui.ttf", 0},
};
#else
const FontFile kBodyFonts[] = {
    {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 2},
    {"/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf", 0},
    {"/usr/share/fonts/truetype/wqy/wqy-microhei.ttc", 0},
    {"/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc", 0},
    {"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf", 0},
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0},
};
const FontFile kTitleFonts[] = {
    {"/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc", 2},
    {"/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc", 2},
    {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 2},
    {"/usr/share/fonts/truetype/wqy/wqy-microhei.ttc", 0},
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0},
};
#endif

bool FileExists(const char* path) {
    if (!path || !*path) return false;
    std::ifstream file(path);
    return file.good();
}

// ---------------------------------------------------------------------------
// Colours
// ---------------------------------------------------------------------------

struct Palette {
    ImVec4 bg;
    ImVec4 card;
    ImVec4 cardHover;
    ImVec4 border;
    ImVec4 borderSoft;
    ImVec4 text;
    ImVec4 textDim;
    ImVec4 accent;
    ImVec4 accentHover;
    ImVec4 accentActive;
    ImVec4 inputBg;
    ImVec4 headerBg;
    ImVec4 rowAlt;
    ImVec4 track;
};

ImVec4 Rgba(int r, int g, int b, float a = 1.0f) {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

Palette PaletteFor(UIManager::Theme theme) {
    switch (theme) {
        case UIManager::Theme::LightPink: {
            Palette p;
            p.bg           = Rgba(251, 241, 244);
            p.card         = Rgba(255, 255, 255);
            p.cardHover    = Rgba(255, 248, 250);
            p.border       = Rgba(241, 225, 230);
            p.borderSoft   = Rgba(247, 236, 239);
            p.text         = Rgba(43, 32, 36);
            p.textDim      = Rgba(147, 128, 138);
            p.accent       = Rgba(226, 96, 127);
            p.accentHover  = Rgba(212, 81, 115);
            p.accentActive = Rgba(194, 68, 103);
            p.inputBg      = Rgba(253, 246, 248);
            p.headerBg     = Rgba(252, 243, 246);
            p.rowAlt       = Rgba(255, 249, 250);
            p.track        = Rgba(243, 229, 233);
            return p;
        }
        case UIManager::Theme::Dark: {
            Palette p;
            p.bg           = Rgba(14, 17, 23);
            p.card         = Rgba(22, 26, 34);
            p.cardHover    = Rgba(26, 31, 41);
            p.border       = Rgba(36, 42, 54);
            p.borderSoft   = Rgba(30, 36, 46);
            p.text         = Rgba(231, 235, 243);
            p.textDim      = Rgba(139, 149, 167);
            p.accent       = Rgba(91, 157, 255);
            p.accentHover  = Rgba(111, 169, 255);
            p.accentActive = Rgba(74, 140, 240);
            p.inputBg      = Rgba(27, 32, 42);
            p.headerBg     = Rgba(26, 31, 40);
            p.rowAlt       = Rgba(25, 30, 39);
            p.track        = Rgba(38, 45, 58);
            return p;
        }
        case UIManager::Theme::LightBlue:
        default: {
            Palette p;
            p.bg           = Rgba(239, 243, 250);
            p.card         = Rgba(255, 255, 255);
            p.cardHover    = Rgba(246, 249, 255);
            p.border       = Rgba(227, 232, 241);
            p.borderSoft   = Rgba(237, 241, 247);
            p.text         = Rgba(27, 35, 51);
            p.textDim      = Rgba(123, 133, 152);
            p.accent       = Rgba(59, 130, 246);
            p.accentHover  = Rgba(46, 116, 232);
            p.accentActive = Rgba(36, 99, 204);
            p.inputBg      = Rgba(245, 247, 251);
            p.headerBg     = Rgba(244, 247, 252);
            p.rowAlt       = Rgba(248, 250, 254);
            p.track        = Rgba(233, 238, 246);
            return p;
        }
    }
}

ImVec4 Col(ImGuiCol index) {
    return ImGui::GetStyle().Colors[index];
}

ImU32 ToU32(const ImVec4& color) {
    return ImGui::GetColorU32(color);
}

ImVec4 WithAlpha(const ImVec4& color, float alpha) {
    ImVec4 result = color;
    result.w = alpha;
    return result;
}

ImVec4 Brighten(const ImVec4& color, float amount) {
    return ImVec4(color.x + (1.0f - color.x) * amount, color.y + (1.0f - color.y) * amount,
                  color.z + (1.0f - color.z) * amount, color.w);
}

ImVec4 StatusColor(float r, float g, float b, bool dark) {
    ImVec4 color(r, g, b, 1.0f);
    return dark ? Brighten(color, 0.38f) : color;
}

} // namespace

UIManager::UIManager(ProcessManager* processManager, SystemMonitor* systemMonitor,
                     ImTextureID appIcon, const UIFonts& fonts, float uiScale)
    : m_processManager(processManager),
      m_systemMonitor(systemMonitor),
      m_appIcon(appIcon),
      m_fonts(fonts),
      m_scale(uiScale > 0.0f ? uiScale : 1.0f),
      m_currentTheme(Theme::LightBlue),
      m_themeChanged(true),
      m_isDark(false),
      m_selectedPid(-1) {
    std::memset(m_searchBuffer, 0, sizeof(m_searchBuffer));
}

UIManager::~UIManager() {}

UIFonts UIManager::LoadFonts(ImGuiIO& io, float uiScale) {
    UIFonts fonts;

    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    config.PixelSnapH = true;

#ifdef _WIN32
    const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseFull();
#else
    const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
#endif

    const float bodySize = 16.0f * uiScale;
    const float smallSize = 12.0f * uiScale;
    const float titleSize = 21.0f * uiScale;

    for (const FontFile& candidate : kBodyFonts) {
        if (!FileExists(candidate.path)) continue;

        config.FontNo = candidate.index;
        ImFont* body = io.Fonts->AddFontFromFileTTF(candidate.path, bodySize, &config, ranges);
        if (!body) continue;

        fonts.body = body;
        fonts.caption = io.Fonts->AddFontFromFileTTF(candidate.path, smallSize, &config, ranges);
        if (!fonts.caption) fonts.caption = body;

        for (const FontFile& titleCandidate : kTitleFonts) {
            if (!FileExists(titleCandidate.path)) continue;
            ImFontConfig titleConfig = config;
            titleConfig.FontNo = titleCandidate.index;
            ImFont* title = io.Fonts->AddFontFromFileTTF(titleCandidate.path, titleSize,
                                                         &titleConfig, ranges);
            if (title) {
                fonts.title = title;
                break;
            }
        }
        if (!fonts.title) fonts.title = body;
        break;
    }

    if (!fonts.body) {
        fonts.body = io.Fonts->AddFontDefault();
        fonts.title = fonts.body;
        fonts.caption = fonts.body;
    }

    return fonts;
}

void UIManager::SetTheme(Theme theme) {
    m_currentTheme = theme;
    m_themeChanged = true;
}

ImVec4 UIManager::GetBackgroundColor() const {
    return Col(ImGuiCol_WindowBg);
}

void UIManager::ApplyTheme() {
    const Palette p = PaletteFor(m_currentTheme);
    const float s = m_scale;
    ImGuiStyle& style = ImGui::GetStyle();

    m_isDark = (m_currentTheme == Theme::Dark);

    // Metrics -----------------------------------------------------------------
    style.WindowPadding    = ImVec2(14.0f * s, 14.0f * s);
    style.FramePadding     = ImVec2(12.0f * s, 6.0f * s);
    style.CellPadding      = ImVec2(10.0f * s, 6.0f * s);
    style.ItemSpacing      = ImVec2(10.0f * s, 8.0f * s);
    style.ItemInnerSpacing = ImVec2(8.0f * s, 6.0f * s);
    style.WindowRounding   = 0.0f;
    style.ChildRounding    = 16.0f * s;
    style.FrameRounding    = 9.0f * s;
    style.PopupRounding    = 10.0f * s;
    style.ScrollbarRounding = 9.0f * s;
    style.GrabRounding     = 9.0f * s;
    style.TabRounding      = 8.0f * s;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize  = 1.0f;
    style.PopupBorderSize  = 1.0f;
    style.FrameBorderSize  = 0.0f;
    style.ScrollbarSize    = 10.0f * s;
    style.GrabMinSize      = 10.0f * s;

    // Colours -----------------------------------------------------------------
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                 = p.text;
    colors[ImGuiCol_TextDisabled]         = p.textDim;
    colors[ImGuiCol_WindowBg]             = p.bg;
    colors[ImGuiCol_ChildBg]              = p.card;
    colors[ImGuiCol_PopupBg]              = p.card;
    colors[ImGuiCol_Border]               = p.border;
    colors[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg]              = p.inputBg;
    colors[ImGuiCol_FrameBgHovered]       = p.cardHover;
    colors[ImGuiCol_FrameBgActive]        = p.borderSoft;
    colors[ImGuiCol_TitleBg]              = p.card;
    colors[ImGuiCol_TitleBgActive]        = p.card;
    colors[ImGuiCol_TitleBgCollapsed]     = p.card;
    colors[ImGuiCol_MenuBarBg]            = p.card;
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrab]        = WithAlpha(p.textDim, 0.35f);
    colors[ImGuiCol_ScrollbarGrabHovered] = WithAlpha(p.textDim, 0.55f);
    colors[ImGuiCol_ScrollbarGrabActive]  = WithAlpha(p.textDim, 0.75f);
    colors[ImGuiCol_CheckMark]            = p.accent;
    colors[ImGuiCol_SliderGrab]           = p.accent;
    colors[ImGuiCol_SliderGrabActive]     = p.accentActive;
    colors[ImGuiCol_Button]               = p.accent;
    colors[ImGuiCol_ButtonHovered]        = p.accentHover;
    colors[ImGuiCol_ButtonActive]         = p.accentActive;
    colors[ImGuiCol_Header]               = WithAlpha(p.accent, 0.20f);
    colors[ImGuiCol_HeaderHovered]        = WithAlpha(p.accent, 0.18f);
    colors[ImGuiCol_HeaderActive]         = WithAlpha(p.accent, 0.26f);
    colors[ImGuiCol_Separator]            = p.borderSoft;
    colors[ImGuiCol_SeparatorHovered]     = p.accent;
    colors[ImGuiCol_SeparatorActive]      = p.accentActive;
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ResizeGripHovered]    = WithAlpha(p.accent, 0.35f);
    colors[ImGuiCol_ResizeGripActive]     = WithAlpha(p.accent, 0.55f);
    colors[ImGuiCol_TableHeaderBg]        = p.headerBg;
    colors[ImGuiCol_TableBorderStrong]    = p.border;
    colors[ImGuiCol_TableBorderLight]     = p.borderSoft;
    colors[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt]        = p.rowAlt;
    colors[ImGuiCol_TextSelectedBg]       = WithAlpha(p.accent, 0.28f);
    colors[ImGuiCol_NavHighlight]         = p.accent;
    colors[ImGuiCol_DragDropTarget]       = p.accent;
    colors[ImGuiCol_PlotLines]            = p.accent;
    colors[ImGuiCol_PlotLinesHovered]     = p.accentHover;
    colors[ImGuiCol_PlotHistogram]        = p.accent;
    colors[ImGuiCol_PlotHistogramHovered] = p.accentHover;

    m_themeChanged = false;
}

std::string UIManager::FormatBytes(uint64_t bytes) const {
    const double kb = static_cast<double>(bytes) / 1024.0;
    const double mb = kb / 1024.0;
    const double gb = mb / 1024.0;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (gb >= 1.0)
        ss << gb << " GB";
    else if (mb >= 1.0)
        ss << mb << " MB";
    else if (kb >= 1.0)
        ss << kb << " KB";
    else
        ss << bytes << " B";
    return ss.str();
}

std::string UIManager::Ellipsize(const std::string& text, float maxWidth) const {
    if (text.empty()) return text;
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;

    static const char kEllipsis[] = "...";
    std::string result = text;
    while (!result.empty()) {
        result.pop_back();
        // Do not leave half a UTF-8 sequence behind.
        while (!result.empty() && (static_cast<unsigned char>(result.back()) & 0xC0) == 0x80) {
            result.pop_back();
        }
        if (ImGui::CalcTextSize((result + kEllipsis).c_str()).x <= maxWidth) break;
    }
    return result + kEllipsis;
}

const char* UIManager::StatusText(const ProcessInfo& proc, ImVec4& color) const {
    if (proc.isNotResponding) {
        color = StatusColor(0.83f, 0.28f, 0.28f, m_isDark);
        return "Not Responding";
    }

    switch (proc.state) {
        case 'R':
            color = StatusColor(0.10f, 0.62f, 0.42f, m_isDark);
            return "Running";
        case 'S':
            color = StatusColor(0.42f, 0.55f, 0.68f, m_isDark);
            return "Sleeping";
        case 'D':
            color = StatusColor(0.85f, 0.55f, 0.13f, m_isDark);
            return "Disk Wait";
        case 'Z':
            color = StatusColor(0.80f, 0.24f, 0.28f, m_isDark);
            return "Zombie";
        case 'T':
        case 't':
            color = StatusColor(0.88f, 0.48f, 0.15f, m_isDark);
            return "Stopped";
        case 'I':
            color = StatusColor(0.55f, 0.57f, 0.62f, m_isDark);
            return "Idle";
        default:
            color = StatusColor(0.10f, 0.62f, 0.42f, m_isDark);
            return "Running";
    }
}

bool UIManager::MatchesSearch(const ProcessInfo& proc) const {
    if (m_searchQuery.empty()) return true;

    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    };

    if (lower(proc.name).find(m_searchQuery) != std::string::npos) return true;
    if (lower(proc.username).find(m_searchQuery) != std::string::npos) return true;
    if (lower(proc.fullPath).find(m_searchQuery) != std::string::npos) return true;

    ImVec4 unusedColor;
    if (lower(StatusText(proc, unusedColor)).find(m_searchQuery) != std::string::npos) return true;
    if (std::to_string(proc.pid).find(m_searchQuery) != std::string::npos) return true;

    const bool matchesAdmin = proc.isAdmin && (m_searchQuery == "admin" ||
                                               m_searchQuery == "isadmin" ||
                                               m_searchQuery == "yes" ||
                                               m_searchQuery == "root");
    return matchesAdmin;
}

void UIManager::DrawCenteredText(const char* text) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float width = ImGui::CalcTextSize(text).x;
    if (avail > width)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
    ImGui::TextUnformatted(text);
}

void UIManager::DrawCenteredTextColored(const ImVec4& color, const char* text) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float width = ImGui::CalcTextSize(text).x;
    if (avail > width)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
    ImGui::TextColored(color, "%s", text);
}

void UIManager::DrawRightAlignedText(const char* text) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float width = ImGui::CalcTextSize(text).x;
    if (avail > width)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width));
    ImGui::TextUnformatted(text);
}

void UIManager::DrawRightAlignedTextColored(const ImVec4& color, const char* text) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float width = ImGui::CalcTextSize(text).x;
    if (avail > width)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width));
    ImGui::TextColored(color, "%s", text);
}

void UIManager::DrawAppIconFallback(float size) const {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    const ImU32 bg = ImGui::GetColorU32(Col(ImGuiCol_Button));
    draw->AddRectFilled(p, ImVec2(p.x + size, p.y + size), bg, size * 0.24f);

    // Three ascending bars: a small "running processes" glyph.
    const float pad = size * 0.24f;
    const float barW = size * 0.14f;
    const float gap = size * 0.12f;
    const float bottom = p.y + size - pad;
    const float heights[3] = {size * 0.26f, size * 0.40f, size * 0.54f};
    for (int i = 0; i < 3; ++i) {
        const float x = p.x + pad + i * (barW + gap);
        const float y = bottom - heights[i];
        draw->AddRectFilled(ImVec2(x, y), ImVec2(x + barW, bottom),
                            IM_COL32(255, 255, 255, 255), barW * 0.35f);
    }

    ImGui::Dummy(ImVec2(size, size));
}

void UIManager::DrawProcessIconFallback(const ProcessInfo& proc, float size) const {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Stable, pleasant color derived from the executable name + pid.
    uint32_t hash = 2166136261u;
    for (unsigned char c : proc.name) {
        hash ^= c;
        hash *= 16777619u;
    }
    hash ^= proc.pid;
    const float hue = static_cast<float>(hash % 360) / 360.0f;
    ImVec4 color;
    ImGui::ColorConvertHSVtoRGB(hue, 0.42f, m_isDark ? 0.72f : 0.86f, color.x, color.y, color.z);
    color.w = 1.0f;
    draw->AddRectFilled(p, ImVec2(p.x + size, p.y + size), ToU32(color), size * 0.28f);

    char letter[8] = {'?', '\0'};
    bool haveLetter = false;
    for (size_t i = 0; i < proc.name.size();) {
        const unsigned char c = static_cast<unsigned char>(proc.name[i]);
        if (c < 0x80) {
            if (std::isalnum(c)) {
                letter[0] = static_cast<char>(std::toupper(c));
                letter[1] = '\0';
                haveLetter = true;
                break;
            }
            ++i;
            continue;
        }

        // Keep the first UTF-8 sequence so CJK names get a readable badge too.
        int length = 0;
        if ((c & 0xE0) == 0xC0)
            length = 2;
        else if ((c & 0xF0) == 0xE0)
            length = 3;
        else if ((c & 0xF8) == 0xF0)
            length = 4;
        if (length > 0 && i + static_cast<size_t>(length) <= proc.name.size()) {
            std::memcpy(letter, proc.name.data() + i, static_cast<size_t>(length));
            letter[length] = '\0';
            haveLetter = true;
        }
        break;
    }
    if (!haveLetter) std::strcpy(letter, "?");
    const ImVec2 textSize = ImGui::CalcTextSize(letter);
    const ImVec2 textPos(p.x + (size - textSize.x) * 0.5f, p.y + (size - textSize.y) * 0.5f);
    draw->AddText(textPos, IM_COL32(255, 255, 255, 255), letter);

    ImGui::Dummy(ImVec2(size, size));
}

void UIManager::DrawGauge(const char* label, double percentage, const ImVec4& color,
                          const std::string& detail, bool available) {
    const float s = m_scale;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float diameter = std::min(84.0f * s, avail);
    const float thickness = 8.0f * s;
    const float radius = diameter * 0.5f - thickness * 0.5f;

    const float offsetX = (avail - diameter) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(offsetX, 0.0f));

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 center(origin.x + diameter * 0.5f, origin.y + diameter * 0.5f);
    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddCircle(center, radius, ToU32(Col(ImGuiCol_TableBorderLight)), 64, thickness);

    if (available && percentage > 0.5) {
        const float clamped = std::clamp(static_cast<float>(percentage), 0.0f, 100.0f);
        const float start = -kPi * 0.5f;
        const float end = start + clamped / 100.0f * 2.0f * kPi;
        const ImU32 arcColor = ToU32(color);
        draw->PathArcTo(center, radius, start, end, 64);
        draw->PathStroke(arcColor, ImDrawFlags_None, thickness);
        draw->AddCircleFilled(ImVec2(center.x + std::cos(start) * radius,
                                     center.y + std::sin(start) * radius),
                              thickness * 0.5f, arcColor, 16);
        draw->AddCircleFilled(ImVec2(center.x + std::cos(end) * radius,
                                     center.y + std::sin(end) * radius),
                              thickness * 0.5f, arcColor, 16);
    }

    char value[32];
    if (available)
        std::snprintf(value, sizeof(value), "%.0f%%", percentage);
    else
        std::snprintf(value, sizeof(value), "N/A");

    ImFont* valueFont = m_fonts.title ? m_fonts.title : ImGui::GetFont();
    const ImVec2 valueSize = valueFont->CalcTextSizeA(valueFont->FontSize, FLT_MAX, 0.0f, value);
    draw->AddText(valueFont, valueFont->FontSize,
                  ImVec2(center.x - valueSize.x * 0.5f, center.y - valueSize.y * 0.5f),
                  ToU32(Col(ImGuiCol_Text)), value);

    ImGui::Dummy(ImVec2(diameter, diameter));

    DrawCenteredTextColored(Col(ImGuiCol_Text), Ellipsize(label, avail - 2.0f * s).c_str());

    ImGui::PushFont(m_fonts.caption ? m_fonts.caption : ImGui::GetFont());
    const std::string plainDetail = Ellipsize(detail, avail - 2.0f * s);
    DrawCenteredTextColored(Col(ImGuiCol_TextDisabled), plainDetail.c_str());
    ImGui::PopFont();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f * s);
}

void UIManager::DrawStatusPill(const char* text, const ImVec4& color) {
    const float s = m_scale;
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const float padX = 8.0f * s;
    const float padY = 3.0f * s;
    const float pillW = textSize.x + padX * 2.0f;
    const float pillH = textSize.y + padY * 2.0f;

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float avail = ImGui::GetContentRegionAvail().x;
    const float offset = std::max((avail - pillW) * 0.5f, 0.0f);
    const ImVec2 p(cursor.x + offset, cursor.y);
    const ImVec2 q(p.x + pillW, p.y + pillH);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p, q, ToU32(WithAlpha(color, 0.14f)), pillH * 0.5f);
    draw->AddText(ImVec2(p.x + padX, p.y + padY), ToU32(color), text);

    ImGui::Dummy(ImVec2(pillW, pillH));
}

void UIManager::DrawCountChip(int count) {
    const float s = m_scale;
    char text[32];
    std::snprintf(text, sizeof(text), "%d", count);

    ImGui::PushFont(m_fonts.caption ? m_fonts.caption : ImGui::GetFont());
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const float height = textSize.y + 6.0f * s;
    const float width = textSize.x + 16.0f * s;

    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p, ImVec2(p.x + width, p.y + height), ToU32(Col(ImGuiCol_Header)),
                        height * 0.5f);
    draw->AddText(ImVec2(p.x + 8.0f * s, p.y + 3.0f * s), ToU32(Col(ImGuiCol_Button)), text);

    ImGui::Dummy(ImVec2(width, height));
    ImGui::PopFont();
}

void UIManager::Render() {
    if (m_themeChanged) ApplyTheme();
    if (m_fonts.body) ImGui::PushFont(m_fonts.body);

    const float s = m_scale;
    const ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("##RunningManager", nullptr, windowFlags)) {
        const float sidebarWidth = 244.0f * s;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * s, 12.0f * s));
        ImGui::BeginChild("##Sidebar", ImVec2(sidebarWidth, 0), true);
        RenderSidebar();
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::SameLine(0.0f, 12.0f * s);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("##Content", ImVec2(0, 0), false);
        RenderContent();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    if (m_fonts.body) ImGui::PopFont();
}

void UIManager::RenderSidebar() {
    const float s = m_scale;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f * s, 6.0f * s));
    RenderSidebarHeader();

    ImGui::Dummy(ImVec2(0, 10.0f * s));
    ImGui::PushFont(m_fonts.caption ? m_fonts.caption : ImGui::GetFont());
    ImGui::PushStyleColor(ImGuiCol_Text, Col(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted("SYSTEM SUMMARY");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 4.0f * s));

    const ImVec4 cpuColor = m_isDark ? Rgba(96, 165, 250) : Rgba(59, 130, 246);
    const ImVec4 memColor = m_isDark ? Rgba(192, 132, 252) : Rgba(147, 51, 234);
    const ImVec4 diskColor = m_isDark ? Rgba(45, 212, 191) : Rgba(13, 148, 136);
    const ImVec4 gpuColor = m_isDark ? Rgba(251, 146, 120) : Rgba(234, 88, 12);

    std::string memDetail = FormatBytes(m_systemMonitor->GetUsedMemoryBytes());
    memDetail += " / ";
    memDetail += FormatBytes(m_systemMonitor->GetTotalMemoryBytes());

    DrawGauge("CPU", m_systemMonitor->GetTotalCpuUsage(), cpuColor,
              m_systemMonitor->GetCpuName(), true);

    DrawGauge("Memory", m_systemMonitor->GetTotalMemoryUsage(), memColor, memDetail, true);

    for (const auto& disk : m_systemMonitor->GetDisks()) {
        const std::string diskLabel = "Disk " + disk.name;
        const std::string diskDetail =
            FormatBytes(disk.totalBytes - disk.freeBytes) + " of " +
            FormatBytes(disk.totalBytes);
        DrawGauge(diskLabel.c_str(), disk.usagePercentage, diskColor, diskDetail, true);
    }

    std::string gpuDetail = m_systemMonitor->GetGpuName();
    if (m_systemMonitor->GetGpuDedicatedMemory() > 0) {
        gpuDetail += "  ";
        gpuDetail += FormatBytes(m_systemMonitor->GetGpuDedicatedMemory());
        gpuDetail += " VRAM";
    }
    const bool gpuAvailable = m_systemMonitor->IsGpuUsageAvailable();
    DrawGauge("GPU", gpuAvailable ? m_systemMonitor->GetTotalGpuUsage() : 0.0, gpuColor, gpuDetail,
              gpuAvailable);

    ImGui::Dummy(ImVec2(0, 4.0f * s));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4.0f * s));
    ImGui::PushFont(m_fonts.caption ? m_fonts.caption : ImGui::GetFont());
    DrawCenteredTextColored(Col(ImGuiCol_TextDisabled), "RunningManager 1.1.0");
    ImGui::PopFont();
    ImGui::PopStyleVar();
}

void UIManager::RenderSidebarHeader() {
    const float s = m_scale;
    const float iconSize = 40.0f * s;
    const float titleHeight = m_fonts.title ? m_fonts.title->FontSize : ImGui::GetFontSize();

    if (m_appIcon) {
        ImGui::Image(m_appIcon, ImVec2(iconSize, iconSize));
    } else {
        DrawAppIconFallback(iconSize);
    }

    ImGui::SameLine(0.0f, 10.0f * s);
    ImGui::BeginGroup();
    {
        const float cursorY = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(cursorY + std::max(0.0f, (iconSize - titleHeight - 16.0f * s) * 0.5f));
        if (m_fonts.title) ImGui::PushFont(m_fonts.title);
        ImGui::TextUnformatted("RunningManager");
        if (m_fonts.title) ImGui::PopFont();

        ImGui::PushFont(m_fonts.caption ? m_fonts.caption : ImGui::GetFont());
        ImGui::TextDisabled("Process & System Monitor");
        ImGui::PopFont();
    }
    ImGui::EndGroup();
}

void UIManager::RenderContent() {
    const float s = m_scale;
    const float titleHeight = m_fonts.title ? m_fonts.title->FontSize : ImGui::GetFontSize();
    const float bodyHeight = m_fonts.body ? m_fonts.body->FontSize : ImGui::GetFontSize();
    const float headerHeight =
        14.0f * s * 2.0f + titleHeight + ImGui::GetStyle().ItemSpacing.y + bodyHeight + 14.0f * s;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f * s, 14.0f * s));
    ImGui::BeginChild("##Header", ImVec2(0, headerHeight), true);
    RenderTopBar();
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, 12.0f * s));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * s, 12.0f * s));
    ImGui::BeginChild("##TableCard", ImVec2(0, 0), true);
    RenderProcessTable();
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void UIManager::RenderTopBar() {
    const float s = m_scale;
    int processCount = 0;
    for (const auto& proc : m_processManager->GetProcesses()) {
        if (MatchesSearch(proc)) ++processCount;
    }
    const float titleHeight = m_fonts.title ? m_fonts.title->FontSize : ImGui::GetFontSize();

    if (m_fonts.title) ImGui::PushFont(m_fonts.title);
    ImGui::TextUnformatted("Processes");
    if (m_fonts.title) ImGui::PopFont();

    ImGui::SameLine(0.0f, 10.0f * s);
    const float chipHeight = (m_fonts.caption ? m_fonts.caption->FontSize : ImGui::GetFontSize()) +
                             6.0f * s;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (titleHeight - chipHeight) * 0.5f));
    DrawCountChip(processCount);

    // Theme swatches, right aligned on the title row.
    const float swatch = 15.0f * s;
    const float swatchBox = swatch + 8.0f * s;
    const float swatchGap = 6.0f * s;
    const float groupWidth = 3.0f * swatchBox + 2.0f * swatchGap;
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - groupWidth);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (titleHeight - swatchBox) * 0.5f));

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const Theme themes[3] = {Theme::LightBlue, Theme::LightPink, Theme::Dark};
    const char* themeNames[3] = {"Light Blue", "Light Pink", "Dark"};
    for (int i = 0; i < 3; ++i) {
        ImGui::PushID(i);
        ImGui::InvisibleButton("##swatch", ImVec2(swatchBox, swatchBox));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) ImGui::SetTooltip("%s", themeNames[i]);
        if (ImGui::IsItemClicked()) SetTheme(themes[i]);

        ImVec2 center = ImGui::GetItemRectMin();
        center.x += swatchBox * 0.5f;
        center.y += swatchBox * 0.5f;
        draw->AddCircleFilled(center, swatch * 0.5f, ToU32(PaletteFor(themes[i]).accent), 24);
        if (m_currentTheme == themes[i]) {
            draw->AddCircle(center, swatch * 0.5f + 2.5f * s, ToU32(Col(ImGuiCol_Text)), 24,
                            1.5f * s);
        } else if (hovered) {
            draw->AddCircle(center, swatch * 0.5f + 2.5f * s, ToU32(Col(ImGuiCol_TextDisabled)),
                            24, 1.5f * s);
        }
        ImGui::PopID();
        if (i < 2) ImGui::SameLine(0.0f, swatchGap);
    }

    // Search field -----------------------------------------------------------
    const ImVec2 framePadding(30.0f * s, 7.0f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::InputTextWithHint(
        "##Search", "Search by name, PID, user or status", m_searchBuffer,
        IM_ARRAYSIZE(m_searchBuffer));
    ImGui::PopStyleVar(2);

    if (changed) {
        m_searchQuery = m_searchBuffer;
        std::transform(m_searchQuery.begin(), m_searchQuery.end(), m_searchQuery.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }

    const ImVec2 rectMin = ImGui::GetItemRectMin();
    const ImVec2 rectMax = ImGui::GetItemRectMax();
    const float midY = (rectMin.y + rectMax.y) * 0.5f;
    const ImVec2 iconCenter(rectMin.x + 16.0f * s, midY);
    const ImU32 iconColor = ToU32(Col(ImGuiCol_TextDisabled));
    draw->AddCircle(iconCenter, 5.5f * s, iconColor, 20, 1.6f * s);
    draw->AddLine(ImVec2(iconCenter.x + 4.0f * s, iconCenter.y + 4.0f * s),
                  ImVec2(iconCenter.x + 8.0f * s, iconCenter.y + 8.0f * s), iconColor, 1.6f * s);

    if (m_searchBuffer[0] != '\0') {
        const float size = rectMax.y - rectMin.y;
        const ImVec2 buttonMin(rectMax.x - size + 2.0f * s, rectMin.y + 2.0f * s);
        const ImVec2 buttonMax(rectMax.x - 2.0f * s, rectMax.y - 2.0f * s);
        const bool clearHovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            ImGui::IsMouseHoveringRect(buttonMin, buttonMax);

        const ImVec2 c(rectMax.x - size * 0.5f, midY);
        const float arm = 4.0f * s;
        const ImU32 clearColor =
            ToU32(Col(clearHovered ? ImGuiCol_Text : ImGuiCol_TextDisabled));
        draw->AddLine(ImVec2(c.x - arm, c.y - arm), ImVec2(c.x + arm, c.y + arm), clearColor,
                      1.6f * s);
        draw->AddLine(ImVec2(c.x - arm, c.y + arm), ImVec2(c.x + arm, c.y - arm), clearColor,
                      1.6f * s);

        if (clearHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            std::memset(m_searchBuffer, 0, sizeof(m_searchBuffer));
            m_searchQuery.clear();
            // The text field keeps an internal copy while active, so leave the
            // edit state; otherwise it would restore the old text next frame.
            ImGui::ClearActiveID();
        }
    }
}

void UIManager::RenderProcessTable() {
    const float s = m_scale;
    const auto& processes = m_processManager->GetProcesses();

    // Filter ------------------------------------------------------------------
    std::vector<const ProcessInfo*> visible;
    visible.reserve(processes.size());
    for (const auto& proc : processes) {
        if (MatchesSearch(proc)) visible.push_back(&proc);
    }

    // Empty state -------------------------------------------------------------
    if (visible.empty()) {
        ImGui::Dummy(ImVec2(0, std::max(0.0f, ImGui::GetContentRegionAvail().y * 0.22f)));

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float avail = ImGui::GetContentRegionAvail().x;
        const ImVec2 center(cursor.x + avail * 0.5f, cursor.y + 24.0f * s);
        const ImVec4 accent = Col(ImGuiCol_Button);
        draw->AddCircleFilled(center, 24.0f * s, ToU32(WithAlpha(accent, 0.10f)), 48);
        draw->AddCircle(center, 24.0f * s, ToU32(WithAlpha(accent, 0.28f)), 48, 1.5f * s);
        draw->AddCircle(ImVec2(center.x - 2.0f * s, center.y - 2.0f * s), 8.0f * s,
                        ToU32(accent), 24, 2.0f * s);
        draw->AddLine(ImVec2(center.x + 4.5f * s, center.y + 4.5f * s),
                      ImVec2(center.x + 11.0f * s, center.y + 11.0f * s), ToU32(accent),
                      2.0f * s);
        ImGui::Dummy(ImVec2(0, 58.0f * s));

        ImGui::PushFont(m_fonts.title ? m_fonts.title : ImGui::GetFont());
        DrawCenteredTextColored(Col(ImGuiCol_Text), "No matching processes");
        ImGui::PopFont();
        ImGui::PushFont(m_fonts.caption ? m_fonts.caption : ImGui::GetFont());
        DrawCenteredTextColored(Col(ImGuiCol_TextDisabled),
                                "Try a different name, PID, user or status.");
        ImGui::PopFont();
        return;
    }

    const float footerHeight = ImGui::GetTextLineHeightWithSpacing() + 4.0f * s;
    const float tableHeight =
        std::max(80.0f * s, ImGui::GetContentRegionAvail().y - footerHeight);

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##ProcessTable", 7, tableFlags, ImVec2(0.0f, tableHeight))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("NAME", ImGuiTableColumnFlags_WidthStretch, 220.0f * s);
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 56.0f * s);
        ImGui::TableSetupColumn("STATUS", ImGuiTableColumnFlags_WidthFixed, 124.0f * s);
        ImGui::TableSetupColumn("USER", ImGuiTableColumnFlags_WidthFixed, 76.0f * s);
        ImGui::TableSetupColumn("ADMIN", ImGuiTableColumnFlags_WidthFixed, 56.0f * s);
        ImGui::TableSetupColumn("CPU",
                                ImGuiTableColumnFlags_WidthFixed |
                                    ImGuiTableColumnFlags_DefaultSort |
                                    ImGuiTableColumnFlags_PreferSortDescending,
                                64.0f * s);
        ImGui::TableSetupColumn("MEMORY", ImGuiTableColumnFlags_WidthFixed, 80.0f * s);

        ImGui::PushFont(m_fonts.caption ? m_fonts.caption : ImGui::GetFont());
        ImGui::PushStyleColor(ImGuiCol_Text, WithAlpha(Col(ImGuiCol_Text), 0.62f));
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();
        ImGui::PopFont();

        // Sorting -------------------------------------------------------------
        ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
        if (sortSpecs && sortSpecs->SpecsCount > 0) {
            auto compareColumn = [this](int column, const ProcessInfo& a, const ProcessInfo& b) {
                switch (column) {
                    case 0:
                        return a.name.compare(b.name);
                    case 1:
                        return (a.pid > b.pid) ? 1 : ((a.pid < b.pid) ? -1 : 0);
                    case 2: {
                        ImVec4 ca, cb;
                        const std::string sa = StatusText(a, ca);
                        const std::string sb = StatusText(b, cb);
                        return sa.compare(sb);
                    }
                    case 3:
                        return a.username.compare(b.username);
                    case 4:
                        return (a.isAdmin > b.isAdmin) ? 1 : ((a.isAdmin < b.isAdmin) ? -1 : 0);
                    case 5:
                        return (a.cpuUsage > b.cpuUsage) ? 1 : ((a.cpuUsage < b.cpuUsage) ? -1 : 0);
                    case 6:
                        return (a.memoryUsageBytes > b.memoryUsageBytes)
                                   ? 1
                                   : ((a.memoryUsageBytes < b.memoryUsageBytes) ? -1 : 0);
                    default:
                        return 0;
                }
            };

            std::stable_sort(visible.begin(), visible.end(),
                             [&compareColumn, sortSpecs](const ProcessInfo* a, const ProcessInfo* b) {
                                 for (int i = 0; i < sortSpecs->SpecsCount; ++i) {
                                     const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[i];
                                     const int delta = compareColumn(spec.ColumnIndex, *a, *b);
                                     if (delta == 0) continue;
                                     return spec.SortDirection == ImGuiSortDirection_Descending
                                                ? delta > 0
                                                : delta < 0;
                                 }
                                 return false;
                             });
        }

        // Rows -----------------------------------------------------------------
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const ProcessInfo* proc = visible[static_cast<size_t>(row)];
                const bool selected = (m_selectedPid == static_cast<int>(proc->pid));
                const float iconSize = 20.0f * s;

                ImGui::PushID(static_cast<int>(proc->pid));
                ImGui::TableNextRow();

                // Name + icon + row interaction.
                ImGui::TableSetColumnIndex(0);
                const ImVec2 rowStart = ImGui::GetCursorScreenPos();
                const bool clicked =
                    ImGui::Selectable("##RowSelect", selected,
                                      ImGuiSelectableFlags_SpanAllColumns |
                                          ImGuiSelectableFlags_AllowOverlap |
                                          ImGuiSelectableFlags_AllowDoubleClick);
                if (clicked) {
                    m_selectedPid = static_cast<int>(proc->pid);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                        proc->hasMainWindow) {
                        ProcessManager::BringToFront(proc->pid);
                    }
                }

                if (ImGui::BeginPopupContextItem("##Actions")) {
                    m_selectedPid = static_cast<int>(proc->pid);
                    if (proc->hasMainWindow) {
                        if (ImGui::MenuItem("Bring to Front"))
                            ProcessManager::BringToFront(proc->pid);
                    }
                    if (ImGui::MenuItem("Open File Location", nullptr, false,
                                        !proc->fullPath.empty())) {
                        ProcessManager::OpenFileLocation(proc->fullPath);
                    }
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          StatusColor(0.83f, 0.28f, 0.28f, m_isDark));
                    if (ImGui::MenuItem("Kill Process", nullptr, false, proc->pid > 1))
                        ProcessManager::KillProcess(proc->pid);
                    ImGui::PopStyleColor();
                    ImGui::EndPopup();
                }

                ImGui::SetCursorScreenPos(rowStart);
                ImDrawList* rowDraw = ImGui::GetWindowDrawList();
                if (selected) {
                    rowDraw->AddRectFilled(
                        ImVec2(rowStart.x - 7.0f * s, rowStart.y + 1.0f * s),
                        ImVec2(rowStart.x - 4.0f * s, rowStart.y + iconSize - 1.0f * s),
                        ToU32(Col(ImGuiCol_Button)), 1.5f * s);
                }

                if (proc->iconTexture) {
                    ImGui::Image(proc->iconTexture, ImVec2(iconSize, iconSize));
                } else {
                    DrawProcessIconFallback(*proc, iconSize);
                }

                ImGui::SameLine(0.0f, 8.0f * s);
                ImGui::TextUnformatted(proc->name.c_str());

                if (proc->hasMainWindow) {
                    ImGui::SameLine(0.0f, 6.0f * s);
                    const ImVec2 dotPos = ImGui::GetCursorScreenPos();
                    rowDraw->AddCircleFilled(
                        ImVec2(dotPos.x + 3.0f * s, dotPos.y + ImGui::GetTextLineHeight() * 0.5f),
                        3.0f * s, ToU32(WithAlpha(Col(ImGuiCol_Button), 0.85f)), 12);
                }

                // PID.
                ImGui::TableSetColumnIndex(1);
                char pidBuffer[32];
                std::snprintf(pidBuffer, sizeof(pidBuffer), "%u",
                              static_cast<unsigned>(proc->pid));
                DrawRightAlignedTextColored(Col(ImGuiCol_TextDisabled), pidBuffer);

                // Status pill.
                ImGui::TableSetColumnIndex(2);
                ImVec4 statusColor;
                const char* statusText = StatusText(*proc, statusColor);
                DrawStatusPill(statusText, statusColor);

                // User.
                ImGui::TableSetColumnIndex(3);
                DrawCenteredTextColored(Col(ImGuiCol_Text), proc->username.c_str());

                // Admin.
                ImGui::TableSetColumnIndex(4);
                if (proc->isAdmin) {
                    DrawCenteredTextColored(StatusColor(0.10f, 0.62f, 0.42f, m_isDark), "Yes");
                } else {
                    DrawCenteredTextColored(Col(ImGuiCol_TextDisabled), "No");
                }

                // CPU.
                ImGui::TableSetColumnIndex(5);
                if (proc->validTime) {
                    char cpuBuffer[32];
                    std::snprintf(cpuBuffer, sizeof(cpuBuffer), "%.1f%%", proc->cpuUsage);
                    ImVec4 cpuColor = Col(ImGuiCol_Text);
                    if (proc->cpuUsage >= 60.0)
                        cpuColor = StatusColor(0.83f, 0.28f, 0.28f, m_isDark);
                    else if (proc->cpuUsage >= 25.0)
                        cpuColor = StatusColor(0.85f, 0.55f, 0.13f, m_isDark);
                    DrawRightAlignedTextColored(cpuColor, cpuBuffer);
                } else {
                    DrawRightAlignedTextColored(Col(ImGuiCol_TextDisabled), "-");
                }

                // Memory.
                ImGui::TableSetColumnIndex(6);
                DrawRightAlignedText(FormatBytes(proc->memoryUsageBytes).c_str());

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    // Footer -----------------------------------------------------------------
    ImGui::PushFont(m_fonts.caption ? m_fonts.caption : ImGui::GetFont());
    ImGui::PushStyleColor(ImGuiCol_Text, Col(ImGuiCol_TextDisabled));
    if (m_searchQuery.empty()) {
        ImGui::Text("%d processes", static_cast<int>(visible.size()));
    } else {
        ImGui::Text("%d of %d processes", static_cast<int>(visible.size()),
                    static_cast<int>(processes.size()));
    }
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("Right-click a row for actions").x);
    ImGui::TextUnformatted("Right-click a row for actions");
    ImGui::PopStyleColor();
    ImGui::PopFont();
}
