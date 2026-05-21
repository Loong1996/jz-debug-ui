#include "dui_layers.h"
#include "dui_canvas.h"
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
    CanvasView& cv = GetActiveCanvasView();

    if (layers.empty()) {
        ImGui::TextDisabled(u8"(无已注册图层)");
        ImGui::End();
        return;
    }

    // --- 热力图：组级 checkbox (cv.show_heatmaps) + 逐项 ---
    {
        bool any = false;
        for (const auto& li : layers)
            if (std::strcmp(li.kind, "Heatmap") == 0) { any = true; break; }
        if (any) {
            ImGui::Checkbox("##hm_all", &cv.show_heatmaps);
            ImGui::SameLine();
            bool open = ImGui::TreeNodeEx(u8"热力图##hm_tree", ImGuiTreeNodeFlags_DefaultOpen);
            if (open) {
                if (!cv.show_heatmaps) ImGui::BeginDisabled();
                int idx = 0;
                for (auto& li : layers) {
                    if (std::strcmp(li.kind, "Heatmap") != 0) continue;
                    ImGui::PushID(idx++);
                    bool en = li.enabled;
                    if (ImGui::Checkbox(li.name, &en))
                        SetLayerEnabled(li.kind, li.name, en);
                    ImGui::PopID();
                }
                if (!cv.show_heatmaps) ImGui::EndDisabled();
                ImGui::TreePop();
            }
        }
    }

    // --- 连线：EntityLinks + CellLinks 合并，组级 checkbox (cv.show_links) + 逐项 ---
    {
        bool any = false;
        for (const auto& li : layers)
            if (std::strcmp(li.kind, "EntityLinks") == 0 ||
                std::strcmp(li.kind, "CellLinks")   == 0) { any = true; break; }
        if (any) {
            ImGui::Checkbox("##lnk_all", &cv.show_links);
            ImGui::SameLine();
            bool open = ImGui::TreeNodeEx(u8"连线##lnk_tree", ImGuiTreeNodeFlags_DefaultOpen);
            if (open) {
                if (!cv.show_links) ImGui::BeginDisabled();
                int idx = 0;
                for (auto& li : layers) {
                    if (std::strcmp(li.kind, "EntityLinks") != 0 &&
                        std::strcmp(li.kind, "CellLinks")   != 0) continue;
                    ImGui::PushID(idx++);
                    bool en = li.enabled;
                    if (ImGui::Checkbox(li.name, &en))
                        SetLayerEnabled(li.kind, li.name, en);
                    ImGui::PopID();
                }
                if (!cv.show_links) ImGui::EndDisabled();
                ImGui::TreePop();
            }
        }
    }

    // --- 其余组：普通折叠，无组级 checkbox ---
    struct Group { const char* display; const char* kind; };
    static const Group kOtherGroups[] = {
        { u8"全局 Overlay", "GlobalOverlay" },
        { u8"实体 Overlay", "EntityOverlay" },
        { u8"格子 Overlay", "CellOverlay"   },
    };

    for (const auto& grp : kOtherGroups) {
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
