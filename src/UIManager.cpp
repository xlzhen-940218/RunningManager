#include "UIManager.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace {
constexpr ImU32 kWhite = IM_COL32(255, 255, 255, 255);

ImU32 ColorForProcess(const ProcessInfo& proc) {
    // Stable, pleasant color derived from the executable name + pid.
    uint32_t hash = 2166136261u;
    for (unsigned char c : proc.name) {
        hash ^= c;
        hash *= 16777619u;
    }
    hash ^= proc.pid;
    const float hue = static_cast<float>(hash % 360) / 360.0f;
    ImVec4 color;
    ImGui::ColorConvertHSVtoRGB(hue, 0.45f, 0.82f, color.x, color.y, color.z);
    color.w = 1.0f;
    return ImGui::GetColorU32(color);
}
} // namespace

UIManager::UIManager(ProcessManager* processManager, SystemMonitor* systemMonitor,
                     ImTextureID appIcon)
    : m_processManager(processManager),
      m_systemMonitor(systemMonitor),
      m_appIcon(appIcon),
      m_currentTheme(Theme::LightBlue),
      m_themeChanged(true),
      m_firstFrame(true) {
    std::memset(m_searchBuffer, 0, sizeof(m_searchBuffer));
}

UIManager::~UIManager() {}

void UIManager::SetTheme(Theme theme) {
    m_currentTheme = theme;
    m_themeChanged = true;
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

const char* UIManager::StatusText(const ProcessInfo& proc, ImVec4& color) {
    if (proc.isNotResponding) {
        color = ImVec4(0.80f, 0.20f, 0.20f, 1.0f);
        return "Not Responding";
    }

    switch (proc.state) {
        case 'R':
            color = ImVec4(0.10f, 0.60f, 0.15f, 1.0f);
            return "Running";
        case 'S':
            color = ImVec4(0.35f, 0.50f, 0.62f, 1.0f);
            return "Sleeping";
        case 'D':
            color = ImVec4(0.85f, 0.55f, 0.10f, 1.0f);
            return "Disk Wait";
        case 'Z':
            color = ImVec4(0.80f, 0.20f, 0.20f, 1.0f);
            return "Zombie";
        case 'T':
        case 't':
            color = ImVec4(0.85f, 0.50f, 0.10f, 1.0f);
            return "Stopped";
        case 'I':
            color = ImVec4(0.45f, 0.45f, 0.55f, 1.0f);
            return "Idle";
        default:
            color = ImVec4(0.10f, 0.60f, 0.15f, 1.0f);
            return "Running";
    }
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

void UIManager::DrawAppIconFallback(float size) const {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    const ImU32 bg = ImGui::GetColorU32(ImVec4(0.31f, 0.60f, 0.94f, 1.0f));
    draw->AddRectFilled(p, ImVec2(p.x + size, p.y + size), bg, size * 0.22f);

    // Three ascending bars: a small "running processes" glyph.
    const float pad = size * 0.24f;
    const float barW = size * 0.14f;
    const float gap = size * 0.12f;
    const float bottom = p.y + size - pad;
    const float heights[3] = {size * 0.26f, size * 0.40f, size * 0.54f};
    for (int i = 0; i < 3; ++i) {
        const float x = p.x + pad + i * (barW + gap);
        const float y = bottom - heights[i];
        draw->AddRectFilled(ImVec2(x, y), ImVec2(x + barW, bottom), kWhite, barW * 0.35f);
    }

    ImGui::Dummy(ImVec2(size, size));
}

void UIManager::DrawProcessIconFallback(const ProcessInfo& proc, float size) const {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(p, ImVec2(p.x + size, p.y + size), ColorForProcess(proc), size * 0.24f);

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
    draw->AddText(textPos, kWhite, letter);

    ImGui::Dummy(ImVec2(size, size));
}

void UIManager::Render() {
    if (m_themeChanged) {
        ImGuiStyle& style = ImGui::GetStyle();

        style.Colors[ImGuiCol_Text]                  = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
        style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
        style.Colors[ImGuiCol_PopupBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
        style.Colors[ImGuiCol_Border]                = ImVec4(0.80f, 0.80f, 0.80f, 0.50f);
        style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_FrameBg]               = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
        style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.85f, 0.88f, 0.92f, 1.00f);
        style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
        style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
        style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
        style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
        style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);

        ImVec4 primaryColor, primaryHover, primaryActive;
        ImVec4 headerColor, headerHover, headerActive;
        ImVec4 rowBgAlt;

        if (m_currentTheme == Theme::LightBlue) {
            primaryColor  = ImVec4(0.35f, 0.65f, 0.95f, 1.00f);
            primaryHover  = ImVec4(0.30f, 0.60f, 0.90f, 1.00f);
            primaryActive = ImVec4(0.25f, 0.55f, 0.85f, 1.00f);

            headerColor   = ImVec4(0.85f, 0.92f, 1.00f, 1.00f);
            headerHover   = ImVec4(0.75f, 0.88f, 1.00f, 1.00f);
            headerActive  = ImVec4(0.65f, 0.82f, 1.00f, 1.00f);

            rowBgAlt      = ImVec4(0.94f, 0.96f, 0.99f, 1.00f);
        } else {
            // Light Pink
            primaryColor  = ImVec4(0.95f, 0.55f, 0.65f, 1.00f);
            primaryHover  = ImVec4(0.90f, 0.50f, 0.60f, 1.00f);
            primaryActive = ImVec4(0.85f, 0.45f, 0.55f, 1.00f);

            headerColor   = ImVec4(1.00f, 0.85f, 0.88f, 1.00f);
            headerHover   = ImVec4(1.00f, 0.78f, 0.82f, 1.00f);
            headerActive  = ImVec4(1.00f, 0.70f, 0.76f, 1.00f);

            rowBgAlt      = ImVec4(1.00f, 0.94f, 0.96f, 1.00f);
        }

        style.Colors[ImGuiCol_Button]                = primaryColor;
        style.Colors[ImGuiCol_ButtonHovered]         = primaryHover;
        style.Colors[ImGuiCol_ButtonActive]          = primaryActive;
        style.Colors[ImGuiCol_CheckMark]             = primaryColor;
        style.Colors[ImGuiCol_SliderGrab]            = primaryColor;
        style.Colors[ImGuiCol_SliderGrabActive]      = primaryActive;

        style.Colors[ImGuiCol_Header]                = headerColor;
        style.Colors[ImGuiCol_HeaderHovered]         = headerHover;
        style.Colors[ImGuiCol_HeaderActive]          = headerActive;

        style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
        style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
        style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
        style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_TableRowBgAlt]         = rowBgAlt;

        m_themeChanged = false;
    }

    const ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

    if (ImGui::Begin("Main Window", nullptr, window_flags)) {
        // Layout: Two columns using child windows
        ImGui::BeginChild("LeftPanel", ImVec2(140, 0), true);
        RenderSystemInfoPanel();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("RightPanel", ImVec2(0, 0), false);
        RenderTopBar();
        ImGui::Separator();
        RenderProcessTable();
        ImGui::EndChild();
    }
    ImGui::End();

    ImGui::PopStyleVar(3);

    if (m_firstFrame) m_firstFrame = false;
}

void UIManager::RenderTopBar() {
    // Theme Selector
    int themeIndex = (m_currentTheme == Theme::LightBlue) ? 0 : 1;
    ImGui::PushItemWidth(150.0f);
    if (ImGui::Combo("Theme", &themeIndex, "Light Blue\0Light Pink\0")) {
        SetTheme(themeIndex == 0 ? Theme::LightBlue : Theme::LightPink);
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(0.0f, 30.0f);

    ImGui::Text("Search:");
    ImGui::SameLine();

    // Span the remaining width of the window
    ImGui::SetNextItemWidth(-FLT_MIN);

    // Add slightly darker background to the search box to make it stand out on white
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

    if (ImGui::InputText("##Search", m_searchBuffer, IM_ARRAYSIZE(m_searchBuffer))) {
        m_searchQuery = m_searchBuffer;
        // Convert to lowercase for case-insensitive search
        std::transform(m_searchQuery.begin(), m_searchQuery.end(), m_searchQuery.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1);
}

void UIManager::RenderSystemInfoPanel() {
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font

    {
        const float panelWidth = ImGui::GetContentRegionAvail().x;
        const float iconSize = 40.0f;
        const float offsetX = (panelWidth - iconSize) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        if (m_appIcon)
            ImGui::Image(m_appIcon, ImVec2(iconSize, iconSize));
        else
            DrawAppIconFallback(iconSize);

        const ImVec2 titleSize = ImGui::CalcTextSize("RunningManager");
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (panelWidth - titleSize.x) * 0.5f);
        const ImVec4 brandColor = (m_currentTheme == Theme::LightPink)
            ? ImVec4(0.95f, 0.45f, 0.55f, 1.0f)
            : ImVec4(0.06f, 0.37f, 0.96f, 1.0f);
        ImGui::TextColored(brandColor, "RunningManager");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    ImGui::TextDisabled("SYSTEM SUMMARY");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto DrawWaterBucket = [&](const char* label, double percentage, const ImVec4& color,
                               const char* primaryText, const char* detailText) {
        ImGui::BeginGroup();
        const float panelWidth = ImGui::GetContentRegionAvail().x;
        const float width = 80.0f;
        const float height = 80.0f;
        const float offsetX = (panelWidth - width) * 0.5f;

        // Center Label
        const ImVec2 labelSize = ImGui::CalcTextSize(label);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (panelWidth - labelSize.x) * 0.5f);
        ImGui::TextUnformatted(label);
        ImGui::Spacing();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        const ImVec2 p = ImGui::GetCursorScreenPos();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Background
        ImVec4 bgColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        if (m_currentTheme == Theme::LightPink) bgColor = ImVec4(0.95f, 0.9f, 0.9f, 1.0f);
        drawList->AddRectFilled(p, ImVec2(p.x + width, p.y + height),
                                ImGui::GetColorU32(bgColor), 8.0f);

        // Fill
        const float fillHeight = height * static_cast<float>(percentage / 100.0f);
        if (fillHeight > 0.0f) {
            ImDrawFlags flags = ImDrawFlags_RoundCornersBottom;
            if (fillHeight >= height - 1.0f) flags = ImDrawFlags_RoundCornersAll;
            drawList->AddRectFilled(ImVec2(p.x, p.y + height - fillHeight),
                                    ImVec2(p.x + width, p.y + height),
                                    ImGui::GetColorU32(color), 8.0f, flags);
        }

        // Border
        drawList->AddRect(p, ImVec2(p.x + width, p.y + height),
                          ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, 1.0f)), 8.0f, 0, 1.0f);

        // Text overlay
        const ImVec2 textSize = ImGui::CalcTextSize(primaryText);
        drawList->AddText(ImVec2(p.x + (width - textSize.x) * 0.5f, p.y + (height - textSize.y) * 0.5f),
                          ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 1.0f)), primaryText);

        ImGui::Dummy(ImVec2(width, height));

        // Detail text (wrapped and optionally centered)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        const ImVec2 detailSize = ImGui::CalcTextSize(detailText);
        if (detailSize.x < panelWidth) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (panelWidth - detailSize.x) * 0.5f);
            ImGui::TextUnformatted(detailText);
        } else {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + panelWidth - 10.0f);
            ImGui::TextWrapped("%s", detailText);
            ImGui::PopTextWrapPos();
        }
        ImGui::PopStyleColor();

        ImGui::EndGroup();
    };

    const ImVec4 cpuColor(0.2f, 0.6f, 0.9f, 1.0f);
    const ImVec4 memColor(0.6f, 0.2f, 0.9f, 1.0f);
    const ImVec4 diskColor(0.2f, 0.8f, 0.4f, 1.0f);
    const ImVec4 gpuColor(0.9f, 0.4f, 0.2f, 1.0f);

    // CPU
    char cpuBuf[32];
    std::snprintf(cpuBuf, sizeof(cpuBuf), "%.1f %%", m_systemMonitor->GetTotalCpuUsage());
    DrawWaterBucket("CPU", m_systemMonitor->GetTotalCpuUsage(), cpuColor, cpuBuf,
                    m_systemMonitor->GetCpuName().c_str());
    ImGui::Spacing();
    ImGui::Spacing();

    // Memory
    char memBuf[64];
    std::snprintf(memBuf, sizeof(memBuf), "%.1f %%", m_systemMonitor->GetTotalMemoryUsage());
    const std::string memDetail = FormatBytes(m_systemMonitor->GetTotalMemoryBytes()) + " Total";
    DrawWaterBucket("Memory", m_systemMonitor->GetTotalMemoryUsage(), memColor, memBuf,
                    memDetail.c_str());
    ImGui::Spacing();
    ImGui::Spacing();

    // Disks
    for (const auto& disk : m_systemMonitor->GetDisks()) {
        char diskBuf[32];
        std::snprintf(diskBuf, sizeof(diskBuf), "%.1f %%", disk.usagePercentage);
        const std::string diskLabel = "Disk " + disk.name;
        const std::string diskDetail =
            FormatBytes(disk.totalBytes - disk.freeBytes) + " / " + FormatBytes(disk.totalBytes);
        DrawWaterBucket(diskLabel.c_str(), disk.usagePercentage, diskColor, diskBuf,
                        diskDetail.c_str());
        ImGui::Spacing();
        ImGui::Spacing();
    }

    // GPU
    char gpuBuf[32];
    if (m_systemMonitor->IsGpuUsageAvailable())
        std::snprintf(gpuBuf, sizeof(gpuBuf), "%.1f %%", m_systemMonitor->GetTotalGpuUsage());
    else
        std::snprintf(gpuBuf, sizeof(gpuBuf), "N/A");

    std::string gpuDetail = m_systemMonitor->GetGpuName();
    if (m_systemMonitor->GetGpuDedicatedMemory() > 0) {
        gpuDetail += "\n" + FormatBytes(m_systemMonitor->GetGpuDedicatedMemory()) + " VRAM";
    }
    DrawWaterBucket("GPU", m_systemMonitor->IsGpuUsageAvailable()
                               ? m_systemMonitor->GetTotalGpuUsage()
                               : 0.0,
                    gpuColor, gpuBuf, gpuDetail.c_str());

    ImGui::PopFont();
}

void UIManager::RenderProcessTable() {
    static const ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("ProcessTableV4", 8, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 32.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("IsAdmin", ImGuiTableColumnFlags_WidthFixed, 65.0f);
        ImGui::TableSetupColumn("CPU",
                                ImGuiTableColumnFlags_WidthFixed |
                                    ImGuiTableColumnFlags_DefaultSort |
                                    ImGuiTableColumnFlags_PreferSortDescending,
                                60.0f);
        ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        const auto& processes = m_processManager->GetProcesses();

        // Optional sorting
        ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs();
        std::vector<const ProcessInfo*> sorted_procs;
        sorted_procs.reserve(processes.size());
        for (const auto& proc : processes) {
            // Apply search filter
            if (!m_searchQuery.empty()) {
                std::string name = proc.name;
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::string username = proc.username;
                std::transform(username.begin(), username.end(), username.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::string fullPath = proc.fullPath;
                std::transform(fullPath.begin(), fullPath.end(), fullPath.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                ImVec4 statusColor;
                std::string status = StatusText(proc, statusColor);
                std::transform(status.begin(), status.end(), status.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                const std::string pidStr = std::to_string(proc.pid);
                const bool matchesAdmin =
                    proc.isAdmin && (m_searchQuery == "admin" || m_searchQuery == "isadmin" ||
                                     m_searchQuery == "yes");

                if (name.find(m_searchQuery) == std::string::npos &&
                    username.find(m_searchQuery) == std::string::npos &&
                    fullPath.find(m_searchQuery) == std::string::npos &&
                    status.find(m_searchQuery) == std::string::npos &&
                    pidStr.find(m_searchQuery) == std::string::npos &&
                    !matchesAdmin) {
                    continue;
                }
            }
            sorted_procs.push_back(&proc);
        }

        if (sorts_specs && sorts_specs->SpecsCount > 0 && !sorted_procs.empty()) {
            const ImGuiTableColumnSortSpecs* sort_spec = &sorts_specs->Specs[0];
            const bool descending = sort_spec->SortDirection == ImGuiSortDirection_Descending;

            std::sort(sorted_procs.begin(), sorted_procs.end(),
                      [sort_spec, descending](const ProcessInfo* a, const ProcessInfo* b) {
                          int delta = 0;
                          switch (sort_spec->ColumnIndex) {
                              case 1: delta = a->name.compare(b->name); break; // Name
                              case 2: delta = (a->pid > b->pid) ? 1 : ((a->pid < b->pid) ? -1 : 0); break; // PID
                              case 3: delta = (a->isNotResponding > b->isNotResponding)
                                                  ? 1
                                                  : ((a->isNotResponding < b->isNotResponding) ? -1 : 0);
                                      break; // Status
                              case 4: delta = a->username.compare(b->username); break; // Username
                              case 5: delta = (a->isAdmin > b->isAdmin) ? 1 : ((a->isAdmin < b->isAdmin) ? -1 : 0);
                                      break; // IsAdmin
                              case 6: delta = (a->cpuUsage > b->cpuUsage) ? 1 : ((a->cpuUsage < b->cpuUsage) ? -1 : 0);
                                      break; // CPU
                              case 7: delta = (a->memoryUsageBytes > b->memoryUsageBytes)
                                                  ? 1
                                                  : ((a->memoryUsageBytes < b->memoryUsageBytes) ? -1 : 0);
                                      break; // Mem
                              default: break;
                          }
                          return descending ? (delta > 0) : (delta < 0);
                      });
        }

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(sorted_procs.size()));
        while (clipper.Step()) {
            for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++) {
                const ProcessInfo* proc = sorted_procs[row_n];

                ImGui::PushID(static_cast<int>(proc->pid));
                ImGui::TableNextRow();

                if (proc->hasMainWindow) {
                    ImVec4 highlightColor;
                    if (m_currentTheme == Theme::LightPink)
                        highlightColor = ImVec4(0.9f, 0.3f, 0.5f, 0.08f);
                    else
                        highlightColor = ImVec4(0.1f, 0.5f, 0.9f, 0.08f); // LightBlue default
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                           ImGui::GetColorU32(highlightColor));
                }

                // 0: Icon & Context Menu
                ImGui::TableSetColumnIndex(0);

                const ImVec2 cursorPos = ImGui::GetCursorPos();
                ImGui::Selectable("##rowSelectable", false,
                                  ImGuiSelectableFlags_SpanAllColumns |
                                      ImGuiSelectableFlags_AllowOverlap |
                                      ImGuiSelectableFlags_AllowDoubleClick);
                if (ImGui::BeginPopupContextItem("ActionsPopup")) {
                    if (proc->hasMainWindow) {
                        if (ImGui::MenuItem("Bring to Front"))
                            ProcessManager::BringToFront(proc->pid);
                    }
                    if (ImGui::MenuItem("Open File Location", nullptr, false,
                                        !proc->fullPath.empty())) {
                        ProcessManager::OpenFileLocation(proc->fullPath);
                    }
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                    if (ImGui::MenuItem("Kill Process", nullptr, false, proc->pid > 1))
                        ProcessManager::KillProcess(proc->pid);
                    ImGui::PopStyleColor();
                    ImGui::EndPopup();
                }
                ImGui::SetCursorPos(cursorPos);

                if (proc->iconTexture)
                    ImGui::Image(proc->iconTexture, ImVec2(24, 24));
                else
                    DrawProcessIconFallback(*proc, 24.0f);

                // 1: Name
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(proc->name.c_str());

                // 2: PID
                ImGui::TableSetColumnIndex(2);
                char pidBuf[32];
                std::snprintf(pidBuf, sizeof(pidBuf), "%u", static_cast<unsigned>(proc->pid));
                DrawCenteredText(pidBuf);

                // 3: Status
                ImGui::TableSetColumnIndex(3);
                ImVec4 statusColor;
                const char* statusText = StatusText(*proc, statusColor);
                DrawCenteredTextColored(statusColor, statusText);

                // 4: Username
                ImGui::TableSetColumnIndex(4);
                DrawCenteredText(proc->username.c_str());

                // 5: IsAdmin
                ImGui::TableSetColumnIndex(5);
                if (proc->isAdmin)
                    DrawCenteredTextColored(ImVec4(0.1f, 0.6f, 0.1f, 1.0f), "Yes");
                else
                    DrawCenteredTextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "No");

                // 6: CPU
                ImGui::TableSetColumnIndex(6);
                if (proc->validTime) {
                    char cpuBuf[32];
                    std::snprintf(cpuBuf, sizeof(cpuBuf), "%.1f%%", proc->cpuUsage);
                    DrawCenteredText(cpuBuf);
                } else {
                    DrawCenteredText("-");
                }

                // 7: Memory
                ImGui::TableSetColumnIndex(7);
                DrawCenteredText(FormatBytes(proc->memoryUsageBytes).c_str());

                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}
