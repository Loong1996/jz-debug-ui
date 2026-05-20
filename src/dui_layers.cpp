#include "dui_layers.h"
#include "dui_ext.h"
#include "dui_menubar.h"
#include <imgui.h>
#include <cstring>
#include <vector>

namespace dui {

void DrawLayerPanel(World& /*world*/) {
    if (!ImGui::Begin(u8"图层", &BuiltinPanelOpenRef(u8"图层")))
        { ImGui::End(); return; }

    std::vector<LayerInfo> layers;
    ListLayers(layers);

    if (layers.empty()) {
        ImGui::TextDisabled(u8"(无已注册图层)");
        ImGui::End();
        return;
    }

    // Group by kind.
    struct Group {
        const char* display;
        const char* kind;
    };
    static const Group kGroups[] = {
        { u8"全局 Overlay",  "GlobalOverlay"  },
        { u8"热力图",        "Heatmap"        },
        { u8"实体连线",      "EntityLinks"    },
        { u8"格子连线",      "CellLinks"      },
        { u8"实体 Overlay",  "EntityOverlay"  },
        { u8"格子 Overlay",  "CellOverlay"    },
    };

    for (const auto& grp : kGroups) {
        bool any = false;
        for (const auto& li : layers)
            if (std::strcmp(li.kind, grp.kind) == 0) { any = true; break; }
        if (!any) continue;

        if (ImGui::CollapsingHeader(grp.display, ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID(grp.kind);
            int idx = 0;
            for (auto& li : layers) {
                if (std::strcmp(li.kind, grp.kind) != 0) continue;
                ImGui::PushID(idx++);
                bool en = li.enabled;
                if (ImGui::Checkbox(li.name, &en))
                    SetLayerEnabled(li.kind, li.name, en);
                ImGui::PopID();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}

} // namespace dui
