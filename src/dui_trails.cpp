#include "dui_trails.h"
#include <imgui.h>
#include <deque>
#include <unordered_map>
#include <cmath>

namespace {

struct TrailSample { float fx, fy; uint32_t map_id; };

static bool  g_enabled    = false;
static int   g_trail_len  = 60;
static std::unordered_map<uint64_t, std::deque<TrailSample>> g_trails;

// Tile visit heat: key encodes (map_id, x, y), value is accumulated frequency.
static std::unordered_map<uint64_t, float> g_tile_heat;
static float g_tile_decay = 0.99f;

// Pack map_id into high 16 bits of x-half so each map has its own key space.
// map_id is truncated to 16 bits — sufficient for any realistic number of maps.
static uint64_t TileKey(uint32_t map_id, int x, int y) {
    uint32_t xpart = (static_cast<uint32_t>(x) & 0x0000FFFFu)
                   | ((map_id & 0xFFFFu) << 16);
    return (static_cast<uint64_t>(xpart) << 32) | static_cast<uint32_t>(y);
}

} // namespace

namespace dui {

void EnableEntityTrails(bool on) { g_enabled = on; }
bool IsEntityTrailsEnabled()      { return g_enabled; }

void SetTrailLength(int n) {
    if (n < 2) n = 2;
    g_trail_len = n;
    for (auto& kv : g_trails)
        while (static_cast<int>(kv.second.size()) > g_trail_len)
            kv.second.pop_front();
}

void ClearAllTrails()                    { g_trails.clear(); }
void ClearTrailsFor(uint64_t entity_id) { g_trails.erase(entity_id); }

void RecordTrailSnapshot(const World& world) {
    if (!g_enabled) return;
    for (const auto& e : world.entities) {
        if (e.map_id != world.active_map_id) continue;
        auto& dq = g_trails[e.id];
        dq.push_back({ e.fx, e.fy, e.map_id });
        while (static_cast<int>(dq.size()) > g_trail_len)
            dq.pop_front();
    }
}

void InvokeTrails_(const World& world, ImDrawList* dl) {
    // Build active-map entity set for lookup
    // Draw each entity's trail as a polyline with fading alpha
    for (const auto& e : world.entities) {
        if (e.map_id != world.active_map_id) continue;
        auto it = g_trails.find(e.id);
        if (it == g_trails.end()) continue;
        const auto& dq = it->second;
        int n = static_cast<int>(dq.size());
        if (n < 2) continue;

        // Convert world positions to screen positions
        // We rely on CanvasToScreen_ being set for this frame by DrawCanvas
        extern ImVec2 CanvasToScreen_(float wx, float wy);

        static ImVec2 pts[1024];
        int cnt = n < 1024 ? n : 1024;
        for (int i = 0; i < cnt; ++i) {
            const auto& s = dq[static_cast<size_t>(n - cnt + i)];
            pts[i] = CanvasToScreen_(s.fx, s.fy);
        }

        // Render as segments with fading alpha (oldest = transparent, newest = solid)
        uint8_t r = static_cast<uint8_t>((e.color)       & 0xFF);
        uint8_t g = static_cast<uint8_t>((e.color >> 8)  & 0xFF);
        uint8_t b = static_cast<uint8_t>((e.color >> 16) & 0xFF);
        for (int i = 1; i < cnt; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(cnt - 1);
            uint8_t a = static_cast<uint8_t>(t * 180.f);
            uint32_t col = IM_COL32(r, g, b, a);
            dl->AddLine(pts[i - 1], pts[i], col, 1.5f);
        }
    }
}

void SetTileVisitDecay(float decay) {
    if (decay <= 0.f) decay = 0.001f;
    if (decay >= 1.f) decay = 0.999f;
    g_tile_decay = decay;
}

void AccumulateTileVisits(const World& world) {
    // Decay all existing entries; prune negligible ones to keep map bounded
    for (auto it = g_tile_heat.begin(); it != g_tile_heat.end(); ) {
        it->second *= g_tile_decay;
        if (it->second < 0.005f) it = g_tile_heat.erase(it);
        else                     ++it;
    }
    // Refresh tiles currently occupied by active-map entities to full heat (1.0).
    // Using assignment rather than accumulation keeps the range bounded at [0,1],
    // so auto_range normalization always divides by 1.0 and decay^N gives the
    // visible trail length naturally.
    for (const auto& e : world.entities) {
        if (e.map_id != world.active_map_id) continue;
        g_tile_heat[TileKey(e.map_id, e.x, e.y)] = 1.f;
    }
}

float GetTileVisitHeat(uint32_t map_id, int x, int y) {
    auto it = g_tile_heat.find(TileKey(map_id, x, y));
    return it != g_tile_heat.end() ? it->second : 0.f;
}

void ClearTileHeat() { g_tile_heat.clear(); }

} // namespace dui
