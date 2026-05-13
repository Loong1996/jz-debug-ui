#include "dui_commands.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace {
    struct Command {
        std::string    name;
        dui::CommandFn fn;
    };
    std::vector<Command> g_cmds;
}

namespace dui {

void RegisterCommand(const char* name, CommandFn fn) {
    for (auto& c : g_cmds) {
        if (c.name == name) { c.fn = std::move(fn); return; }
    }
    g_cmds.push_back({ std::string(name), std::move(fn) });
}

void UnregisterCommand(const char* name) {
    for (auto it = g_cmds.begin(); it != g_cmds.end(); ++it) {
        if (it->name == name) { g_cmds.erase(it); return; }
    }
}

void DrawCommands() {
    ImGui::Begin(u8"命令");

    static char search[64] = {};
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText(u8"##cmd_search", search, sizeof(search));
    ImGui::Separator();

    // Collect ordered unique categories for the filtered command set
    std::vector<std::string> cats;
    for (const auto& cmd : g_cmds) {
        if (search[0] && cmd.name.find(search) == std::string::npos) continue;
        size_t slash = cmd.name.find('/');
        std::string cat = (slash != std::string::npos)
            ? cmd.name.substr(0, slash)
            : std::string(u8"通用");
        bool dup = false;
        for (const auto& c : cats) if (c == cat) { dup = true; break; }
        if (!dup) cats.push_back(cat);
    }

    // Track which command to execute after the draw loop (avoids iterator invalidation)
    int to_exec = -1;

    for (const auto& cat : cats) {
        if (search[0]) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        if (ImGui::TreeNodeEx(cat.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (int i = 0; i < static_cast<int>(g_cmds.size()); ++i) {
                const Command& cmd = g_cmds[i];
                if (search[0] && cmd.name.find(search) == std::string::npos) continue;
                size_t slash = cmd.name.find('/');
                std::string this_cat = (slash != std::string::npos)
                    ? cmd.name.substr(0, slash)
                    : std::string(u8"通用");
                if (this_cat != cat) continue;

                const char* short_name = (slash != std::string::npos)
                    ? cmd.name.c_str() + slash + 1
                    : cmd.name.c_str();

                ImGui::PushID(i);
                float btn_w = ImGui::CalcTextSize(u8"执行").x
                            + ImGui::GetStyle().FramePadding.x * 2.f;
                float sel_w = ImGui::GetContentRegionAvail().x
                            - btn_w - ImGui::GetStyle().ItemSpacing.x;
                bool dbl = ImGui::Selectable(short_name, false,
                    ImGuiSelectableFlags_AllowDoubleClick, ImVec2(sel_w, 0.f));
                bool exec = dbl && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                ImGui::SameLine();
                exec |= ImGui::SmallButton(u8"执行");
                if (exec) to_exec = i;
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }

    // Execute outside the draw loop so any RegisterCommand/UnregisterCommand inside
    // the handler does not invalidate iterators used above.
    if (to_exec >= 0 && to_exec < static_cast<int>(g_cmds.size())) {
        CommandFn fn = g_cmds[to_exec].fn;
        if (fn) fn();
    }

    ImGui::End();
}

} // namespace dui
