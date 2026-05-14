#include "dui_events.h"
#include <imgui.h>
#include <chrono>
#include <deque>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>

namespace dui {
namespace {

static const auto s_start = std::chrono::steady_clock::now();
static float now_sec() {
    return std::chrono::duration<float>(
        std::chrono::steady_clock::now() - s_start).count();
}

static const uint32_t kPalette[] = {
    IM_COL32(100, 200, 255, 230),
    IM_COL32(255, 180,  80, 230),
    IM_COL32( 80, 230, 130, 230),
    IM_COL32(255,  90, 120, 230),
    IM_COL32(190, 130, 255, 230),
    IM_COL32(255, 230,  80, 230),
    IM_COL32( 80, 220, 210, 230),
    IM_COL32(255, 150, 220, 230),
};
static const int kPaletteSize = 8;

struct EventEntry {
    float    time;
    int      cat_idx;
    char     text[128];
    uint32_t color;
};

struct Category {
    std::string name;
    uint32_t    color;
    bool        visible;
};

static std::deque<EventEntry> s_events;
static std::vector<Category>  s_cats;
static constexpr float        kWindowSec  = 60.f;
static constexpr int          kMaxEvents  = 5000;
static bool                   s_paused    = false;
static float                  s_pause_time = 0.f;

static int find_or_add_cat(const char* name, uint32_t color) {
    for (int i = 0; i < static_cast<int>(s_cats.size()); i++)
        if (s_cats[i].name == name) return i;
    Category c;
    c.name    = name;
    c.color   = color ? color : kPalette[static_cast<int>(s_cats.size()) % kPaletteSize];
    c.visible = true;
    s_cats.push_back(c);
    return static_cast<int>(s_cats.size()) - 1;
}

} // anonymous namespace

void PushEvent(const char* category, const char* text, uint32_t color) {
    if (!category || !text) return;
    int cat_idx = find_or_add_cat(category, color);
    EventEntry e{};
    e.time    = now_sec();
    e.cat_idx = cat_idx;
    e.color   = color ? color : s_cats[cat_idx].color;
    std::snprintf(e.text, sizeof(e.text), "%s", text);
    s_events.push_back(e);
    if (static_cast<int>(s_events.size()) > kMaxEvents)
        s_events.pop_front();
}

void DrawEvents() {
    if (!ImGui::Begin(u8"事件")) { ImGui::End(); return; }

    // --- Toolbar ---
    if (ImGui::Button(s_paused ? u8"▶ 继续" : u8"⏸ 暂停")) {
        if (!s_paused) { s_paused = true; s_pause_time = now_sec(); }
        else           s_paused = false;
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"清空")) { s_events.clear(); }

    // Category filter checkboxes
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    for (auto& cat : s_cats) {
        ImGui::SameLine();
        ImVec4 col = ImGui::ColorConvertU32ToFloat4(cat.color);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, col);
        ImGui::Checkbox(cat.name.c_str(), &cat.visible);
        ImGui::PopStyleColor();
    }
    ImGui::Separator();

    float display_time = s_paused ? s_pause_time : now_sec();

    // --- Timeline canvas ---
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.y < 40.f) avail.y = 40.f;

    const int   n_cats  = static_cast<int>(s_cats.size());
    const float row_h   = 24.f;
    const float axis_h  = 20.f;
    const float lbl_w   = 90.f;
    const float canvas_h = (n_cats > 0 ? n_cats * row_h : row_h) + axis_h;
    const float panel_h  = (canvas_h < avail.y ? canvas_h : avail.y);

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1(p0.x + avail.x, p0.y + panel_h);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 26, 255));
    dl->PushClipRect(p0, p1, true);

    // Category row backgrounds + labels
    for (int i = 0; i < n_cats; i++) {
        float ry = p0.y + i * row_h;
        uint32_t bg = (i % 2 == 0) ? IM_COL32(28, 28, 38, 255) : IM_COL32(22, 22, 32, 255);
        dl->AddRectFilled(ImVec2(p0.x, ry), ImVec2(p1.x, ry + row_h), bg);
        // Category color stripe
        dl->AddRectFilled(ImVec2(p0.x, ry), ImVec2(p0.x + 4.f, ry + row_h), s_cats[i].color);
        // Label
        dl->AddText(ImVec2(p0.x + 8.f, ry + (row_h - ImGui::GetFontSize()) * 0.5f),
                    IM_COL32(200, 200, 210, 200), s_cats[i].name.c_str());
    }

    // Time axis ticks at bottom
    const float timeline_x0 = p0.x + lbl_w;
    const float timeline_w  = avail.x - lbl_w;
    const float axis_y      = p0.y + n_cats * row_h;
    dl->AddLine(ImVec2(timeline_x0, axis_y), ImVec2(p1.x, axis_y), IM_COL32(80, 80, 90, 200));
    for (int s = 0; s <= 60; s += 10) {
        float xpos = timeline_x0 + (1.f - s / 60.f) * timeline_w;
        dl->AddLine(ImVec2(xpos, axis_y), ImVec2(xpos, axis_y + 4.f), IM_COL32(100, 100, 110, 200));
        char buf[8];
        std::snprintf(buf, sizeof(buf), "-%ds", s);
        dl->AddText(ImVec2(xpos - 12.f, axis_y + 5.f), IM_COL32(120, 120, 130, 200), buf);
    }

    // GC old events and draw dots
    float oldest = display_time - kWindowSec;
    while (!s_events.empty() && !s_paused && s_events.front().time < oldest - 5.f)
        s_events.pop_front();

    bool hover_found = false;
    for (const auto& ev : s_events) {
        if (ev.time < oldest) continue;
        int ci = ev.cat_idx;
        if (ci < 0 || ci >= n_cats) continue;
        if (!s_cats[ci].visible) continue;

        float frac = (ev.time - oldest) / kWindowSec;
        float ex   = timeline_x0 + frac * timeline_w;
        float ey   = p0.y + ci * row_h + row_h * 0.5f;

        dl->AddCircleFilled(ImVec2(ex, ey), 4.f, ev.color);
        dl->AddCircle(ImVec2(ex, ey), 4.f, IM_COL32(255, 255, 255, 60), 12, 1.f);

        if (!hover_found) {
            ImVec2 mp = ImGui::GetMousePos();
            float dx = mp.x - ex, dy = mp.y - ey;
            if (dx*dx + dy*dy < 36.f) {
                ImGui::SetTooltip("[%.2fs] %s: %s", ev.time, s_cats[ci].name.c_str(), ev.text);
                hover_found = true;
            }
        }
    }

    dl->PopClipRect();

    // Consume the space
    ImGui::Dummy(ImVec2(avail.x, panel_h));
    ImGui::End();
}

} // namespace dui
