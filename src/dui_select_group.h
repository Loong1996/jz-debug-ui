#pragma once
#include "dui_world.h"
#include <cstdint>

namespace dui {

// RTS-style named selection groups (slots 1–9).
// Save current selection to a slot; recall it later.
// Groups are session-only (not persisted).

void SaveSelectionGroup  (World& w, int slot); // slot 1..9
void RecallSelectionGroup(World& w, int slot); // slot 1..9; no-op if empty
bool HasSelectionGroup   (int slot);           // true if slot has been saved
int  CountSelectionGroup (int slot);           // number of ids in slot

} // namespace dui
