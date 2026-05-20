#include "dui_commands.h"
#include "dui_hotkeys.h"
#include "dui_menubar.h"
#include <imgui.h>
#include <initializer_list>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstring>

namespace {

struct Command {
    std::string    name;
    dui::CommandFn fn;
};

struct ArgState {
    int         i_val = 0;
    float       f_val = 0.f;
    bool        b_val = false;
    std::string s_val;
};

struct CommandWithArgs {
    std::string                  name;
    std::vector<dui::CommandArg> args;
    dui::CommandFnArgs           fn;
    std::vector<ArgState>        state;
};

static std::vector<Command>         g_cmds;
static std::vector<CommandWithArgs> g_cmds_args;
static bool s_open_args_popup = false;
static int  s_args_cmd_idx    = -1;

// Returns category from "Cat/Name" or "" for uncategorized.
static std::string get_cat(const std::string& name) {
    size_t slash = name.find('/');
    return slash != std::string::npos ? name.substr(0, slash) : std::string(u8"通用");
}

// Collect unique categories across both registries, preserving first-seen order.
static std::vector<std::string> collect_cats(const char* filter) {
    std::vector<std::string> cats;
    auto add_cat = [&](const std::string& cat) {
        for (const auto& c : cats) if (c == cat) return;
        cats.push_back(cat);
    };
    for (const auto& cmd : g_cmds) {
        if (filter[0] && cmd.name.find(filter) == std::string::npos) continue;
        add_cat(get_cat(cmd.name));
    }
    for (const auto& cmd : g_cmds_args) {
        if (filter[0] && cmd.name.find(filter) == std::string::npos) continue;
        add_cat(get_cat(cmd.name));
    }
    return cats;
}

} // anonymous namespace

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

void ExecuteCommand(const char* name) {
    if (!name) return;
    for (auto& c : g_cmds) {
        if (c.name == name) { if (c.fn) c.fn(); return; }
    }
    // With-args commands: just execute with last-saved values
    for (auto& c : g_cmds_args) {
        if (c.name != name) continue;
        if (!c.fn) return;
        std::vector<CommandArgValue> vals;
        for (size_t j = 0; j < c.args.size(); j++) {
            CommandArgValue v{};
            v.b = c.state[j].b_val;
            v.i = c.state[j].i_val;
            v.f = c.state[j].f_val;
            v.s = c.state[j].s_val.c_str();
            vals.push_back(v);
        }
        c.fn(vals.data(), static_cast<int>(vals.size()));
        return;
    }
}

void RegisterCommandWithArgs(const char* name,
                             const CommandArg* args, int arg_count,
                             CommandFnArgs fn) {
    // Update existing if found
    for (auto& c : g_cmds_args) {
        if (c.name == name) {
            c.args.assign(args, args + arg_count);
            c.fn = std::move(fn);
            // Re-initialize state with defaults
            c.state.clear();
            for (int j = 0; j < arg_count; j++) {
                ArgState st;
                switch (args[j].type) {
                    case ArgType::Bool:   st.b_val = args[j].default_v.b; break;
                    case ArgType::Int:    st.i_val = args[j].default_v.i; break;
                    case ArgType::Float:  st.f_val = args[j].default_v.f; break;
                    case ArgType::String: st.s_val = args[j].default_str ? args[j].default_str : ""; break;
                    case ArgType::Enum:   st.i_val = args[j].default_v.i; break;
                }
                c.state.push_back(std::move(st));
            }
            return;
        }
    }
    CommandWithArgs cwa;
    cwa.name = name;
    cwa.args.assign(args, args + arg_count);
    cwa.fn   = std::move(fn);
    for (int j = 0; j < arg_count; j++) {
        ArgState st;
        switch (args[j].type) {
            case ArgType::Bool:   st.b_val = args[j].default_v.b; break;
            case ArgType::Int:    st.i_val = args[j].default_v.i; break;
            case ArgType::Float:  st.f_val = args[j].default_v.f; break;
            case ArgType::String: st.s_val = args[j].default_str ? args[j].default_str : ""; break;
            case ArgType::Enum:   st.i_val = args[j].default_v.i; break;
        }
        cwa.state.push_back(std::move(st));
    }
    g_cmds_args.push_back(std::move(cwa));
}

void RegisterCommandWithArgs(const char* name,
                             std::initializer_list<CommandArg> args,
                             CommandFnArgs fn) {
    std::vector<CommandArg> v(args);
    RegisterCommandWithArgs(name, v.data(), static_cast<int>(v.size()), std::move(fn));
}

void DrawCommands() {
    if (!ImGui::Begin(u8"命令", &BuiltinPanelOpenRef(u8"命令")))
        { ImGui::End(); return; }

    static char search[64] = {};
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText(u8"##cmd_search", search, sizeof(search));
    ImGui::Separator();

    auto cats = collect_cats(search);

    // Track pending actions
    int  to_exec      = -1;
    static bool s_open_capture    = false;
    static int  s_capture_cmd_idx = -1;

    for (const auto& cat : cats) {
        if (search[0]) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        if (ImGui::TreeNodeEx(cat.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            // --- Zero-arg commands ---
            for (int i = 0; i < static_cast<int>(g_cmds.size()); ++i) {
                const Command& cmd = g_cmds[i];
                if (search[0] && cmd.name.find(search) == std::string::npos) continue;
                if (get_cat(cmd.name) != cat) continue;

                size_t slash = cmd.name.find('/');
                const char* short_name = slash != std::string::npos
                    ? cmd.name.c_str() + slash + 1 : cmd.name.c_str();

                ImGui::PushID(i);

                std::string hk = GetHotkeyLabel(cmd.name.c_str());
                float hk_w  = hk.empty() ? 0.f
                    : ImGui::CalcTextSize(hk.c_str()).x + ImGui::GetStyle().ItemSpacing.x;
                float btn_w = ImGui::CalcTextSize(u8"执行").x
                            + ImGui::GetStyle().FramePadding.x * 2.f;
                float kb_w  = ImGui::CalcTextSize(u8"⌨").x
                            + ImGui::GetStyle().FramePadding.x * 2.f;
                float sel_w = ImGui::GetContentRegionAvail().x
                            - btn_w - kb_w - hk_w
                            - ImGui::GetStyle().ItemSpacing.x * 3.f;
                bool dbl = ImGui::Selectable(short_name, false,
                    ImGuiSelectableFlags_AllowDoubleClick, ImVec2(sel_w, 0.f));
                bool exec = dbl && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                ImGui::SameLine();
                exec |= ImGui::SmallButton(u8"执行");
                ImGui::SameLine();
                if (ImGui::SmallButton(u8"⌨")) {
                    s_capture_cmd_idx = i;
                    s_open_capture    = true;
                }
                if (!hk.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", hk.c_str());
                }
                if (exec) to_exec = i;

                ImGui::PopID();
            }
            // --- With-args commands ---
            for (int i = 0; i < static_cast<int>(g_cmds_args.size()); ++i) {
                const CommandWithArgs& cwa = g_cmds_args[i];
                if (search[0] && cwa.name.find(search) == std::string::npos) continue;
                if (get_cat(cwa.name) != cat) continue;

                size_t slash = cwa.name.find('/');
                const char* short_name = slash != std::string::npos
                    ? cwa.name.c_str() + slash + 1 : cwa.name.c_str();

                ImGui::PushID(10000 + i);

                float btn_w = ImGui::CalcTextSize(u8"设置...").x
                            + ImGui::GetStyle().FramePadding.x * 2.f;
                float sel_w = ImGui::GetContentRegionAvail().x
                            - btn_w - ImGui::GetStyle().ItemSpacing.x;
                bool dbl = ImGui::Selectable(short_name, false,
                    ImGuiSelectableFlags_AllowDoubleClick, ImVec2(sel_w, 0.f));
                bool open = dbl && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                ImGui::SameLine();
                open |= ImGui::SmallButton(u8"设置...");
                if (open) {
                    s_args_cmd_idx    = i;
                    s_open_args_popup = true;
                }

                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }

    // Execute zero-arg command
    if (to_exec >= 0 && to_exec < static_cast<int>(g_cmds.size())) {
        CommandFn fn = g_cmds[to_exec].fn;
        if (fn) fn();
    }

    // Open args modal
    if (s_open_args_popup) {
        s_open_args_popup = false;
        ImGui::OpenPopup(u8"命令参数##modal");
    }

    // Render args modal
    if (ImGui::BeginPopupModal(u8"命令参数##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (s_args_cmd_idx >= 0 && s_args_cmd_idx < static_cast<int>(g_cmds_args.size())) {
            CommandWithArgs& cwa = g_cmds_args[s_args_cmd_idx];

            size_t slash = cwa.name.find('/');
            const char* short_name = slash != std::string::npos
                ? cwa.name.c_str() + slash + 1 : cwa.name.c_str();
            ImGui::TextDisabled("%s", short_name);
            ImGui::Separator();

            for (int j = 0; j < static_cast<int>(cwa.args.size()); j++) {
                const CommandArg& arg = cwa.args[j];
                ArgState& st = cwa.state[j];
                ImGui::Text("%s", arg.name);
                ImGui::SameLine(140.f);
                ImGui::PushID(j);
                ImGui::SetNextItemWidth(180.f);
                switch (arg.type) {
                    case ArgType::Bool:
                        ImGui::Checkbox("##v", &st.b_val);
                        break;
                    case ArgType::Int:
                        if (arg.min_v != arg.max_v)
                            ImGui::SliderInt("##v", &st.i_val,
                                static_cast<int>(arg.min_v), static_cast<int>(arg.max_v));
                        else
                            ImGui::InputInt("##v", &st.i_val);
                        break;
                    case ArgType::Float:
                        if (arg.min_v != arg.max_v)
                            ImGui::SliderFloat("##v", &st.f_val, arg.min_v, arg.max_v, "%.2f");
                        else
                            ImGui::InputFloat("##v", &st.f_val, 0.f, 0.f, "%.3f");
                        break;
                    case ArgType::String:
                        if (st.s_val.size() < 256) st.s_val.resize(256, '\0');
                        ImGui::InputText("##v", &st.s_val[0], st.s_val.size());
                        break;
                    case ArgType::Enum:
                        if (arg.enum_items && arg.enum_count > 0)
                            ImGui::Combo("##v", &st.i_val, arg.enum_items, arg.enum_count);
                        break;
                }
                ImGui::PopID();
            }

            ImGui::Separator();
            if (ImGui::Button(u8"确定", ImVec2(80, 0))) {
                std::vector<CommandArgValue> vals;
                for (const auto& st : cwa.state) {
                    CommandArgValue v{};
                    v.b = st.b_val;
                    v.i = st.i_val;
                    v.f = st.f_val;
                    v.s = st.s_val.c_str();
                    vals.push_back(v);
                }
                CommandFnArgs fn = cwa.fn;
                ImGui::CloseCurrentPopup();
                if (fn) fn(vals.data(), static_cast<int>(vals.size()));
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"取消", ImVec2(80, 0)))
                ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Open hotkey capture modal
    if (s_open_capture) {
        s_open_capture = false;
        ImGui::OpenPopup(u8"绑定快捷键##hk");
    }

    if (ImGui::BeginPopupModal(u8"绑定快捷键##hk", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(u8"按下键组合... (Esc 取消)");

        ImGuiIO& io = ImGui::GetIO();
        int mods = 0;
        if (io.KeyCtrl)  mods |= ImGuiMod_Ctrl;
        if (io.KeyShift) mods |= ImGuiMod_Shift;
        if (io.KeyAlt)   mods |= ImGuiMod_Alt;

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        } else {
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++) {
                ImGuiKey key = static_cast<ImGuiKey>(k);
                // Skip modifier-only keys
                if (key == ImGuiKey_LeftCtrl  || key == ImGuiKey_RightCtrl  ||
                    key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
                    key == ImGuiKey_LeftAlt   || key == ImGuiKey_RightAlt   ||
                    key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper ||
                    key == ImGuiKey_Escape) continue;
                if (!ImGui::IsKeyPressed(key, false)) continue;
                if (s_capture_cmd_idx >= 0 &&
                    s_capture_cmd_idx < static_cast<int>(g_cmds.size())) {
                    BindHotkey(key, mods, g_cmds[s_capture_cmd_idx].name.c_str());
                }
                ImGui::CloseCurrentPopup();
                break;
            }
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

void GetAllCommands(std::vector<CommandInfo>& out) {
    out.clear();
    for (int i = 0; i < static_cast<int>(g_cmds.size()); i++)
        out.push_back({ g_cmds[i].name, false, i });
    for (int i = 0; i < static_cast<int>(g_cmds_args.size()); i++)
        out.push_back({ g_cmds_args[i].name, true, i });
}

void OpenArgsModalByName(const char* name) {
    for (int i = 0; i < static_cast<int>(g_cmds_args.size()); i++) {
        if (g_cmds_args[i].name == name) {
            s_args_cmd_idx    = i;
            s_open_args_popup = true;
            return;
        }
    }
}

} // namespace dui
