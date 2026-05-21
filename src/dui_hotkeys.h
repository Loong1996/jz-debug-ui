#pragma once
#include <imgui.h>
#include <string>

namespace dui {

// Bind a global hotkey to a registered command (by exact name as passed to RegisterCommand).
// mods: bitmask of ImGuiMod_Ctrl / ImGuiMod_Shift / ImGuiMod_Alt. 0 = no modifiers.
// Registering the same (key, mods) pair again overwrites the previous binding.
void BindHotkey  (ImGuiKey key, int mods, const char* command_name);
void UnbindHotkey(ImGuiKey key, int mods);
void ClearHotkeys();

// Returns a display string like "[F5]" or "[Ctrl+R]" for the first binding of this command.
// Returns empty string if no binding exists.
std::string GetHotkeyLabel(const char* command_name);

// Internal: called from App::Tick each frame.
void ProcessHotkeys();
// Internal: called from App::Init / App::Attach.
void LoadHotkeys();
// Internal: called after any bind change.
void SaveHotkeys();

} // namespace dui
