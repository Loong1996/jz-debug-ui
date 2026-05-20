#pragma once

namespace dui {

// Library-built main menu bar.
// Auto-populates from the RegisterCommand registry (commands grouped by Category/Name),
// shows hotkey labels, and includes built-in 视图 (panel toggles) and 帮助 menus.
// DrawAll already calls this; users need not call it manually.
void DrawMenuBar();

// Per-panel open state used by the 视图 menu and each panel's Draw function.
bool  IsBuiltinPanelOpen(const char* name);
bool& BuiltinPanelOpenRef(const char* name); // reference for Begin(name, &ref)

} // namespace dui
