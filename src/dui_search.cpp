#include "dui_search.h"
#include "dui_commands.h"
#include "dui_hotkeys.h"
#include "dui_ext.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace dui {
namespace {

static bool s_open    = false;
static bool s_just_opened = false;
static char s_query[128] = {};
static int  s_sel   = 0;

struct SearchResult {
    enum Kind { KEntity, KCmd, KCmdArgs } kind;
    int         entity_idx = -1;
    int         cmd_index  = -1;
    int         score      = 0;
    std::string display;
    std::string name;   // for command execution / entity selection
};

static std::vector<SearchResult> s_results;

static std::string to_lower(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

static int score_substr(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return 1;
    return to_lower(hay).find(to_lower(needle)) != std::string::npos ? 1 : 0;
}

static void rebuild(World& world) {
    s_results.clear();
    std::string q = s_query;

    // Entities
    for (int i = 0; i < static_cast<int>(world.entities.size()); i++) {
        const Entity& e = world.entities[i];
        int sc = 0;
        if (!q.empty()) {
            char id_buf[24]; std::snprintf(id_buf, sizeof(id_buf), "%llu",
                static_cast<unsigned long long>(e.id));
            if (score_substr(id_buf,  q)) sc = std::max(sc, 10);
            if (score_substr(e.label, q)) sc = std::max(sc, 8);
            const char* tn = GetEntityTypeName(e.type);
            if (tn && score_substr(tn, q)) sc = std::max(sc, 5);
            const char* mn = GetMapName(e.map_id);
            if (mn && score_substr(mn, q)) sc = std::max(sc, 3);
        } else {
            sc = 2; // include all when empty
        }
        if (sc == 0) continue;

        const char* type_name = GetEntityTypeName(e.type);
        const char* map_name  = GetMapName(e.map_id);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[E #%llu] %s '%s' @ %s",
            static_cast<unsigned long long>(e.id),
            type_name ? type_name : "?",
            e.label,
            map_name  ? map_name  : "?");

        SearchResult r;
        r.kind       = SearchResult::KEntity;
        r.entity_idx = i;
        r.score      = sc;
        r.display    = buf;
        s_results.push_back(std::move(r));
    }

    // Commands
    std::vector<CommandInfo> cmds;
    GetAllCommands(cmds);
    for (const auto& ci : cmds) {
        int sc = 0;
        if (!q.empty()) {
            if (score_substr(ci.name, q)) sc = 7;
        } else {
            sc = 1;
        }
        if (sc == 0) continue;

        std::string hk = GetHotkeyLabel(ci.name.c_str());
        std::string disp = ci.has_args
            ? "[CMD ...] " + ci.name
            : "[CMD] "     + ci.name;
        if (!hk.empty()) disp += "  " + hk;

        SearchResult r;
        r.kind      = ci.has_args ? SearchResult::KCmdArgs : SearchResult::KCmd;
        r.cmd_index = ci.index;
        r.score     = sc;
        r.display   = std::move(disp);
        r.name      = ci.name;
        s_results.push_back(std::move(r));
    }

    // Sort: higher score first, then stable by insertion order
    std::stable_sort(s_results.begin(), s_results.end(),
        [](const SearchResult& a, const SearchResult& b) { return a.score > b.score; });

    if (static_cast<int>(s_results.size()) > 30)
        s_results.resize(30);

    s_sel = 0;
}

static void activate(World& world) {
    if (s_sel < 0 || s_sel >= static_cast<int>(s_results.size())) { s_open = false; return; }
    const SearchResult& r = s_results[s_sel];
    switch (r.kind) {
    case SearchResult::KEntity: {
        const Entity& e = world.entities[r.entity_idx];
        if (e.map_id != world.active_map_id)
            SwitchActiveMap(world, e.map_id);
        world.selected_id = static_cast<int>(e.id);
        break;
    }
    case SearchResult::KCmd:
        ExecuteCommand(r.name.c_str());
        break;
    case SearchResult::KCmdArgs:
        OpenArgsModalByName(r.name.c_str());
        break;
    }
    s_open = false;
}

} // anonymous namespace

void DrawGlobalSearch(World& world) {
    ImGuiIO& io = ImGui::GetIO();

    // Open on Ctrl+P
    if (!io.WantTextInput && io.KeyCtrl &&
        ImGui::IsKeyPressed(ImGuiKey_P, false)) {
        s_open       = true;
        s_just_opened = true;
        s_query[0]   = '\0';
        s_sel        = 0;
        rebuild(world);
    }

    if (!s_open) return;

    // Position: centered horizontally, 25% from top
    ImVec2 display = io.DisplaySize;
    float  win_w   = 600.f;
    float  win_h   = 400.f;
    ImGui::SetNextWindowPos(
        ImVec2((display.x - win_w) * 0.5f, display.y * 0.25f),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(win_w, win_h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar    |
        ImGuiWindowFlags_NoResize      |
        ImGuiWindowFlags_NoMove        |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking     |
        ImGuiWindowFlags_NoScrollbar;

    if (!ImGui::Begin("##global_search", &s_open, flags)) {
        ImGui::End();
        return;
    }

    // Esc to close
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        s_open = false;
        ImGui::End();
        return;
    }

    // Close if window loses focus and user clicks elsewhere
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !s_just_opened) {
        s_open = false;
        ImGui::End();
        return;
    }

    // Search input
    ImGui::SetNextItemWidth(-1.f);
    if (s_just_opened) ImGui::SetKeyboardFocusHere();
    bool changed = ImGui::InputText(u8"##search_input", s_query, sizeof(s_query));
    if (changed) rebuild(world);
    s_just_opened = false;

    // Keyboard navigation
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        s_sel = std::min(s_sel + 1, static_cast<int>(s_results.size()) - 1);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        s_sel = std::max(s_sel - 1, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
        activate(world);
        ImGui::End();
        return;
    }

    ImGui::Separator();

    // Results list
    ImGui::BeginChild("##search_results", ImVec2(0, 0), false);
    for (int i = 0; i < static_cast<int>(s_results.size()); i++) {
        bool selected = (i == s_sel);
        ImGui::PushID(i);
        if (ImGui::Selectable(s_results[i].display.c_str(), selected,
                ImGuiSelectableFlags_None)) {
            s_sel = i;
            activate(world);
            ImGui::PopID();
            ImGui::EndChild();
            ImGui::End();
            return;
        }
        if (selected && ImGui::IsWindowAppearing())
            ImGui::SetScrollHereY();
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace dui
