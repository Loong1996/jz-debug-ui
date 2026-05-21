#include "dui_hotkeys.h"
#include "dui_commands.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace dui {
namespace {

struct HotkeyEntry {
    ImGuiKey    key;
    int         mods;
    std::string command_name;
};

static std::vector<HotkeyEntry> s_bindings;
static const char* kIniPath = "dui_hotkeys.ini";

static std::string format_label(ImGuiKey key, int mods) {
    std::string s = "[";
    if (mods & ImGuiMod_Ctrl)  s += "Ctrl+";
    if (mods & ImGuiMod_Shift) s += "Shift+";
    if (mods & ImGuiMod_Alt)   s += "Alt+";
    const char* kn = ImGui::GetKeyName(key);
    s += kn ? kn : "?";
    s += "]";
    return s;
}

static ImGuiKey find_key_by_name(const char* name) {
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++) {
        const char* kn = ImGui::GetKeyName(static_cast<ImGuiKey>(k));
        if (kn && std::strcmp(kn, name) == 0)
            return static_cast<ImGuiKey>(k);
    }
    return ImGuiKey_None;
}

} // anonymous namespace

void BindHotkey(ImGuiKey key, int mods, const char* command_name) {
    if (!command_name) return;
    for (auto& b : s_bindings) {
        if (b.key == key && b.mods == mods) {
            b.command_name = command_name;
            SaveHotkeys();
            return;
        }
    }
    s_bindings.push_back({ key, mods, command_name });
    SaveHotkeys();
}

void ClearHotkeys() { s_bindings.clear(); SaveHotkeys(); }
void UnbindHotkey(ImGuiKey key, int mods) {
    for (auto it = s_bindings.begin(); it != s_bindings.end(); ++it) {
        if (it->key == key && it->mods == mods) {
            s_bindings.erase(it);
            SaveHotkeys();
            return;
        }
    }
}

std::string GetHotkeyLabel(const char* command_name) {
    if (!command_name) return "";
    for (const auto& b : s_bindings)
        if (b.command_name == command_name)
            return format_label(b.key, b.mods);
    return "";
}

void ProcessHotkeys() {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    ImGuiIO& io = ImGui::GetIO();
    int cur_mods = 0;
    if (io.KeyCtrl)  cur_mods |= ImGuiMod_Ctrl;
    if (io.KeyShift) cur_mods |= ImGuiMod_Shift;
    if (io.KeyAlt)   cur_mods |= ImGuiMod_Alt;

    for (const auto& b : s_bindings) {
        if (b.mods != cur_mods) continue;
        if (!ImGui::IsKeyPressed(b.key, false)) continue;
        ExecuteCommand(b.command_name.c_str());
    }
}

void LoadHotkeys() {
    FILE* f = std::fopen(kIniPath, "r");
    if (!f) return;
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        char* eq = std::strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key_str = line;
        const char* cmd     = eq + 1;
        // strip trailing newline from cmd
        size_t len = std::strlen(cmd);
        while (len > 0 && (cmd[len-1] == '\n' || cmd[len-1] == '\r')) len--;
        std::string cmd_str(cmd, len);

        // Parse "Ctrl+Shift+F5" style
        int mods = 0;
        std::string ks = key_str;
        auto strip_prefix = [&](const char* prefix, int mod) {
            size_t plen = std::strlen(prefix);
            if (ks.size() > plen && ks.substr(0, plen) == prefix) {
                mods |= mod;
                ks = ks.substr(plen);
                return true;
            }
            return false;
        };
        bool changed = true;
        while (changed) {
            changed  = strip_prefix("Ctrl+",  ImGuiMod_Ctrl);
            changed |= strip_prefix("Shift+", ImGuiMod_Shift);
            changed |= strip_prefix("Alt+",   ImGuiMod_Alt);
        }
        ImGuiKey k = find_key_by_name(ks.c_str());
        if (k != ImGuiKey_None && !cmd_str.empty())
            s_bindings.push_back({ k, mods, cmd_str });
    }
    std::fclose(f);
}

void SaveHotkeys() {
    FILE* f = std::fopen(kIniPath, "w");
    if (!f) return;
    for (const auto& b : s_bindings) {
        std::string key_str;
        if (b.mods & ImGuiMod_Ctrl)  key_str += "Ctrl+";
        if (b.mods & ImGuiMod_Shift) key_str += "Shift+";
        if (b.mods & ImGuiMod_Alt)   key_str += "Alt+";
        const char* kn = ImGui::GetKeyName(b.key);
        key_str += kn ? kn : "Unknown";
        std::fprintf(f, "%s=%s\n", key_str.c_str(), b.command_name.c_str());
    }
    std::fclose(f);
}

} // namespace dui
