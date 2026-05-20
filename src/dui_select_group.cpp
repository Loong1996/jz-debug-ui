#include "dui_select_group.h"
#include "dui_ext.h"
#include <cstdint>
#include <vector>

namespace dui {

namespace {
    static std::vector<uint64_t> s_groups[9];
}

void SaveSelectionGroup(World& w, int slot) {
    if (slot < 1 || slot > 9) return;
    std::vector<uint64_t>& g = s_groups[slot - 1];
    g.clear();
    ForEachSelected(w, [&](const Entity& e) { g.push_back(e.id); });
}

void RecallSelectionGroup(World& w, int slot) {
    if (slot < 1 || slot > 9) return;
    const std::vector<uint64_t>& g = s_groups[slot - 1];
    if (g.empty()) return;
    SelectClear(w);
    for (uint64_t id : g)
        SelectAdd(w, id, false);
    // Make the first id the primary selection.
    if (!g.empty()) {
        w.selected_id = g[0];
    }
}

bool HasSelectionGroup(int slot) {
    if (slot < 1 || slot > 9) return false;
    return !s_groups[slot - 1].empty();
}

int CountSelectionGroup(int slot) {
    if (slot < 1 || slot > 9) return 0;
    return static_cast<int>(s_groups[slot - 1].size());
}

} // namespace dui
