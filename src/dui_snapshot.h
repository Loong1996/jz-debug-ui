#pragma once
#include "dui_world.h"

namespace dui {

// Save world state (entities, cells, pins, selection) to a JSON file.
// Returns true on success.
bool SaveWorldSnapshot(const World& w, const char* path);

// Load world state from a previously-saved JSON snapshot.
// Overwrites w completely on success; leaves w unchanged on failure.
// Returns true on success.
bool LoadWorldSnapshot(World& w, const char* path);

// Open Win32 Save/Open dialogs and delegate to the above.
// Returns true if the user selected a file and the operation succeeded.
bool SaveWorldSnapshotDialog(const World& w);
bool LoadWorldSnapshotDialog(World& w);

} // namespace dui
