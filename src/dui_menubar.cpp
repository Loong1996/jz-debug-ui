#include "dui_menubar.h"
#include "dui_commands.h"
#include "dui_hotkeys.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace dui {

// ---- Panel open-state table -------------------------------------------------
// Indexed by the UTF-8 panel name. All panels default to open (true).

namespace {

struct PanelState { const char* name; bool open; };

static PanelState s_panels[] = {
    { u8"检视器",   true },
    { u8"场景视图", true },
    { u8"实体详情", true },
    { u8"日志",     true },
    { u8"监视",     true },
    { u8"性能指标", true },
    { u8"命令",     true },
    { u8"事件",     true },
    { "Profiler",   true },
};
static const int kNumPanels = static_cast<int>(sizeof(s_panels) / sizeof(s_panels[0]));

static PanelState* find_panel(const char* name) {
    for (int i = 0; i < kNumPanels; ++i)
        if (strcmp(s_panels[i].name, name) == 0) return &s_panels[i];
    return nullptr;
}

} // anonymous namespace

bool IsBuiltinPanelOpen(const char* name) {
    const PanelState* p = find_panel(name);
    return p ? p->open : true;
}

bool& BuiltinPanelOpenRef(const char* name) {
    PanelState* p = find_panel(name);
    static bool s_fallback = true;
    return p ? p->open : s_fallback;
}

// ---- Menu bar ---------------------------------------------------------------

namespace {

// Parse "Category/Name" → category = "Category", label = "Name"
// If no slash, category = u8"通用", label = full name.
static void parse_cmd_name(const std::string& full,
                            std::string& cat, std::string& label) {
    auto slash = full.find('/');
    if (slash == std::string::npos) {
        cat   = u8"通用";
        label = full;
    } else {
        cat   = full.substr(0, slash);
        label = full.substr(slash + 1);
    }
}

static void draw_help_popup() {
    // Modal for hotkey cheat-sheet
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.f, 320.f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(u8"热键速查##hk_modal", nullptr,
                                ImGuiWindowFlags_NoResize)) return;

    std::vector<CommandInfo> cmds;
    GetAllCommands(cmds);

    ImGui::TextDisabled(u8"所有已绑定热键：");
    ImGui::Separator();

    if (ImGui::BeginTable("##hk_tbl", 2,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn(u8"命令",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(u8"热键",  ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableHeadersRow();
        for (const auto& ci : cmds) {
            std::string lbl = GetHotkeyLabel(ci.name.c_str());
            if (lbl.empty()) continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(ci.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(lbl.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 80.f) * 0.5f +
                          ImGui::GetCursorPosX());
    if (ImGui::Button(u8"关闭", ImVec2(80.f, 0.f)))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

static void draw_about_popup() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(320.f, 140.f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(u8"关于##about_modal", nullptr,
                                ImGuiWindowFlags_NoResize)) return;
    ImGui::TextUnformatted("dui — in-process debug UI library");
    ImGui::TextDisabled(u8"ImGui + ImPlot + Win32/D3D11");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 80.f) * 0.5f +
                          ImGui::GetCursorPosX());
    if (ImGui::Button(u8"关闭", ImVec2(80.f, 0.f)))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

} // anonymous namespace

void DrawMenuBar() {
    // ---- Render pending modals (must be outside BeginMainMenuBar) ----
    draw_help_popup();
    draw_about_popup();

    if (!ImGui::BeginMainMenuBar()) return;

    // ---- Auto-built command menus ----
    {
        std::vector<CommandInfo> cmds;
        GetAllCommands(cmds);

        // Collect sorted category list
        std::vector<std::string> cats;
        for (const auto& ci : cmds) {
            std::string cat, lbl;
            parse_cmd_name(ci.name, cat, lbl);
            bool dup = false;
            for (const auto& c : cats) if (c == cat) { dup = true; break; }
            if (!dup) cats.push_back(cat);
        }
        std::sort(cats.begin(), cats.end());

        for (const auto& cat : cats) {
            if (!ImGui::BeginMenu(cat.c_str())) continue;
            for (const auto& ci : cmds) {
                std::string c, label;
                parse_cmd_name(ci.name, c, label);
                if (c != cat) continue;
                std::string hk = GetHotkeyLabel(ci.name.c_str());
                // Append "…" for commands that open a modal form
                if (ci.has_args && (label.empty() || label.back() != 0xA6))
                    label += u8"…";
                if (ImGui::MenuItem(label.c_str(),
                                    hk.empty() ? nullptr : hk.c_str())) {
                    if (ci.has_args) OpenArgsModalByName(ci.name.c_str());
                    else             ExecuteCommand      (ci.name.c_str());
                }
            }
            ImGui::EndMenu();
        }
    }

    // ---- 视图 menu ----
    if (ImGui::BeginMenu(u8"视图")) {
        for (int i = 0; i < kNumPanels; ++i) {
            if (ImGui::MenuItem(s_panels[i].name, nullptr, s_panels[i].open)) {
                s_panels[i].open = !s_panels[i].open;
                // If re-opening, bring the window to focus
                if (s_panels[i].open) ImGui::SetWindowFocus(s_panels[i].name);
            }
        }
        ImGui::EndMenu();
    }

    // ---- 帮助 menu ----
    if (ImGui::BeginMenu(u8"帮助")) {
        if (ImGui::MenuItem(u8"热键速查")) ImGui::OpenPopup(u8"热键速查##hk_modal");
        ImGui::Separator();
        if (ImGui::MenuItem(u8"关于"))     ImGui::OpenPopup(u8"关于##about_modal");
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

} // namespace dui
