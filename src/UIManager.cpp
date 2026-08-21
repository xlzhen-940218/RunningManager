#include "UIManager.h"
#include "imgui.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

UIManager::UIManager(ProcessManager* processManager)
    : m_processManager(processManager), m_firstFrame(true), m_currentTheme(Theme::LightBlue), m_themeChanged(true) {
    memset(m_searchBuffer, 0, sizeof(m_searchBuffer));
}

UIManager::~UIManager() {}

void UIManager::SetTheme(Theme theme) {
    m_currentTheme = theme;
    m_themeChanged = true;
}

std::string UIManager::WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::string UIManager::FormatBytes(SIZE_T bytes) {
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (gb >= 1.0) ss << gb << " GB";
    else if (mb >= 1.0) ss << mb << " MB";
    else if (kb >= 1.0) ss << kb << " KB";
    else ss << bytes << " B";
    return ss.str();
}

void UIManager::Render() {
    if (m_themeChanged) {
        ImGuiStyle& style = ImGui::GetStyle();
        
        style.Colors[ImGuiCol_Text]                  = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
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

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    
    ImGui::Begin("Main Window", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    RenderTopBar();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    RenderProcessTable();

    ImGui::End();
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
        std::transform(m_searchQuery.begin(), m_searchQuery.end(), m_searchQuery.begin(), ::tolower);
    }
    
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1);
}

void UIManager::RenderProcessTable() {
    static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("ProcessTableV2", 8, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 32.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableHeadersRow();

        auto CenterText = [](const char* text) {
            float avail = ImGui::GetContentRegionAvail().x;
            float width = ImGui::CalcTextSize(text).x;
            if (avail > width) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
            }
            ImGui::TextUnformatted(text);
        };

        auto CenterTextColored = [](const ImVec4& color, const char* text) {
            float avail = ImGui::GetContentRegionAvail().x;
            float width = ImGui::CalcTextSize(text).x;
            if (avail > width) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
            }
            ImGui::TextColored(color, text);
        };

        const auto& processes = m_processManager->GetProcesses();

        // Optional sorting
        ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs();
        std::vector<const ProcessInfo*> sorted_procs;
        for (const auto& proc : processes) {
            // Apply search filter
            if (!m_searchQuery.empty()) {
                std::string name = WStringToString(proc.name);
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                std::string username = WStringToString(proc.username);
                std::transform(username.begin(), username.end(), username.begin(), ::tolower);
                
                std::string pidStr = std::to_string(proc.pid);
                
                if (name.find(m_searchQuery) == std::string::npos && 
                    username.find(m_searchQuery) == std::string::npos &&
                    pidStr.find(m_searchQuery) == std::string::npos) {
                    continue;
                }
            }
            sorted_procs.push_back(&proc);
        }

        if (sorts_specs && sorts_specs->SpecsCount > 0 && !sorted_procs.empty()) {
            const ImGuiTableColumnSortSpecs* sort_spec = &sorts_specs->Specs[0];
            bool descending = sort_spec->SortDirection == ImGuiSortDirection_Descending;

            std::sort(sorted_procs.begin(), sorted_procs.end(), [sort_spec, descending](const ProcessInfo* a, const ProcessInfo* b) {
                int delta = 0;
                switch (sort_spec->ColumnIndex) {
                    case 1: delta = a->name.compare(b->name); break; // Name
                    case 2: delta = (a->pid > b->pid) ? 1 : ((a->pid < b->pid) ? -1 : 0); break; // PID
                    case 3: delta = (a->isNotResponding > b->isNotResponding) ? 1 : ((a->isNotResponding < b->isNotResponding) ? -1 : 0); break; // Status
                    case 4: delta = a->username.compare(b->username); break; // Username
                    case 5: delta = (a->cpuUsage > b->cpuUsage) ? 1 : ((a->cpuUsage < b->cpuUsage) ? -1 : 0); break; // CPU
                    case 6: delta = (a->memoryUsageBytes > b->memoryUsageBytes) ? 1 : ((a->memoryUsageBytes < b->memoryUsageBytes) ? -1 : 0); break; // Mem
                    default: break;
                }
                return descending ? (delta > 0) : (delta < 0);
            });
        }

        ImGuiListClipper clipper;
        clipper.Begin((int)sorted_procs.size());
        while (clipper.Step()) {
            for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++) {
                const ProcessInfo* proc = sorted_procs[row_n];
                
                ImGui::PushID(proc->pid);
                ImGui::TableNextRow();
                
                // 0: Icon
                ImGui::TableSetColumnIndex(0);
                if (proc->iconTexture) {
                    ImGui::Image((void*)proc->iconTexture, ImVec2(24, 24));
                }

                // 1: Name
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(WStringToString(proc->name).c_str());

                // 2: PID
                ImGui::TableSetColumnIndex(2);
                char pidBuf[32];
                snprintf(pidBuf, sizeof(pidBuf), "%u", proc->pid);
                CenterText(pidBuf);

                // 3: Status
                ImGui::TableSetColumnIndex(3);
                if (proc->isNotResponding) {
                    CenterTextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "Not Responding");
                } else {
                    CenterTextColored(ImVec4(0.1f, 0.6f, 0.1f, 1.0f), "Running");
                }

                // 4: Username
                ImGui::TableSetColumnIndex(4);
                CenterText(WStringToString(proc->username).c_str());

                // 5: CPU
                ImGui::TableSetColumnIndex(5);
                if (proc->validTime) {
                    char cpuBuf[32];
                    snprintf(cpuBuf, sizeof(cpuBuf), "%.1f%%", proc->cpuUsage);
                    CenterText(cpuBuf);
                } else {
                    CenterText("-");
                }

                // 6: Memory
                ImGui::TableSetColumnIndex(6);
                CenterText(FormatBytes(proc->memoryUsageBytes).c_str());
                
                // 7: Actions
                ImGui::TableSetColumnIndex(7);
                
                ImVec2 btnSizeFront(50, 0);
                ImVec2 btnSizeLocate(60, 0);
                ImVec2 btnSizeKill(50, 0);

                // Push white text color for all action buttons
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

                if (proc->hasMainWindow) {
                    if (ImGui::Button("Front", btnSizeFront)) {
                        ProcessManager::BringToFront(proc->pid);
                    }
                } else {
                    // Reserve space but pop text color so it doesn't affect dummy (though dummy has no text)
                    ImGui::Dummy(btnSizeFront);
                }
                ImGui::SameLine();
                
                if (ImGui::Button("Locate", btnSizeLocate)) {
                    ProcessManager::OpenFileLocation(proc->fullPath);
                }
                ImGui::SameLine();
                
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("Kill", btnSizeKill)) {
                    ProcessManager::KillProcess(proc->pid);
                }
                ImGui::PopStyleColor(3);
                
                ImGui::PopStyleColor(1); // Pop Text color

                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}
