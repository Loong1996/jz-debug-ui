#include "dui_pins.h"
#include "dui_canvas.h"
#include "dui_ext.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dui {
namespace {

static std::vector<Pin> s_pins;
static uint64_t s_next_id = 1;

} // anonymous namespace

uint64_t AddPin(uint32_t map_id, float wx, float wy, const char* text, uint32_t color) {
    Pin p;
    p.id     = s_next_id++;
    p.map_id = map_id;
    p.wx     = wx;
    p.wy     = wy;
    p.color  = color;
    p.text   = text ? text : "";
    s_pins.push_back(std::move(p));
    return s_pins.back().id;
}

bool RemovePin(uint64_t pin_id) {
    for (auto it = s_pins.begin(); it != s_pins.end(); ++it) {
        if (it->id == pin_id) { s_pins.erase(it); return true; }
    }
    return false;
}

void ClearAllPins() { s_pins.clear(); }

void ClearPinsOnMap(uint32_t map_id) {
    s_pins.erase(
        std::remove_if(s_pins.begin(), s_pins.end(),
                       [map_id](const Pin& p){ return p.map_id == map_id; }),
        s_pins.end());
}

const std::vector<Pin>& ListPins() { return s_pins; }

// ---- Overlay ----

void DrawPinsOverlay_(const World& world, ImDrawList* dl) {
    if (s_pins.empty()) return;
    const CanvasView& v = GetActiveCanvasView();
    // We rely on CanvasToScreen_ which is set each frame by DrawCanvas
    for (const auto& p : s_pins) {
        if (p.map_id != world.active_map_id) continue;
        ImVec2 sc = CanvasToScreen_(p.wx, p.wy);
        float th  = CanvasTileHalf_();
        float ts  = fmaxf(th * 0.4f, 5.f);

        // Pin diamond (filled marker)
        dl->AddCircleFilled(sc, ts * 0.6f, p.color);
        dl->AddCircle      (sc, ts * 0.6f, IM_COL32(0, 0, 0, 120), 12, 1.f);

        // Pin stem — a small downward line
        dl->AddLine(sc, ImVec2(sc.x, sc.y + ts), IM_COL32(0, 0, 0, 150), 1.5f);

        // Text bubble
        if (!p.text.empty() && th >= 8.f) {
            ImVec2 tsz = ImGui::CalcTextSize(p.text.c_str());
            float  pad = 3.f;
            ImVec2 bp0(sc.x - tsz.x * 0.5f - pad, sc.y - ts - tsz.y - pad * 2.f);
            ImVec2 bp1(sc.x + tsz.x * 0.5f + pad, sc.y - ts);
            dl->AddRectFilled(bp0, bp1, IM_COL32(20, 20, 20, 200), 3.f);
            dl->AddRect      (bp0, bp1, p.color, 3.f, 0, 1.f);
            dl->AddText(ImVec2(bp0.x + pad, bp0.y + pad), IM_COL32(240, 240, 240, 240), p.text.c_str());
        }
    }
}

// ---- Inspector panel ----

void DrawPinsPanel_(World& world) {
    int n = 0;
    for (const auto& p : s_pins) if (p.map_id == world.active_map_id) n++;
    char hdr[64];
    std::snprintf(hdr, sizeof(hdr), u8"标注 (%d)##pins_hdr", n);
    if (!ImGui::CollapsingHeader(hdr)) return;

    if (s_pins.empty()) { ImGui::TextDisabled(u8"(暂无标注)"); return; }

    static uint64_t s_editing_id = 0;
    static char     s_edit_buf[128] = {};

    for (int i = 0; i < static_cast<int>(s_pins.size()); ++i) {
        Pin& p = s_pins[i];
        ImGui::PushID(static_cast<int>(p.id));

        // Color swatch
        ImVec4 col = ImGui::ColorConvertU32ToFloat4(p.color);
        ImGui::ColorButton("##pc", col, ImGuiColorEditFlags_NoTooltip, ImVec2(12, 12));
        ImGui::SameLine();

        if (s_editing_id == p.id) {
            ImGui::SetNextItemWidth(120.f);
            if (ImGui::InputText("##pe", s_edit_buf, sizeof(s_edit_buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                p.text = s_edit_buf;
                s_editing_id = 0;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"✓")) { p.text = s_edit_buf; s_editing_id = 0; }
        } else {
            char row[160];
            std::snprintf(row, sizeof(row), "[%.0f,%.0f] %s", p.wx, p.wy, p.text.c_str());
            ImGui::TextUnformatted(row);
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"跳")) {
                // Jump to pin location
                CanvasView& v  = GetActiveCanvasView();
                if (p.map_id != world.active_map_id) SwitchActiveMap(world, p.map_id);
                v.cam_x        = p.wx;
                v.cam_y        = p.wy;
                v.follow_player = false;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"改")) {
                s_editing_id = p.id;
                std::snprintf(s_edit_buf, sizeof(s_edit_buf), "%s", p.text.c_str());
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"删")) {
                RemovePin(p.id);
                ImGui::PopID();
                break;
            }
        }
        ImGui::PopID();
    }
}

} // namespace dui
