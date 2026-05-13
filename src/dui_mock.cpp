#include "dui_mock.h"
#include <cstdio>
#include <cmath>

namespace dui {

World MakeMockWorld() {
    World w;
    unsigned seed = 0xDEADBEEFu;
    auto lcg = [&]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 16) / 65535.f;
    };
    auto rng = [&](float lo, float hi) { return lo + lcg() * (hi - lo); };
    auto col32 = [](uint8_t r, uint8_t g, uint8_t b, uint8_t a = 220) -> uint32_t {
        return (static_cast<uint32_t>(a) << 24)
             | (static_cast<uint32_t>(b) << 16)
             | (static_cast<uint32_t>(g) << 8) | r;
    };
    const uint32_t palette[5] = {
        col32(255, 80,  80 ),
        col32(80,  200, 80 ),
        col32(80,  120, 255),
        col32(255, 200, 50 ),
        col32(200, 80,  255),
    };

    for (int i = 0; i < 15; ++i) {
        Entity e{};
        e.id     = static_cast<uint64_t>(1000 + i);
        e.fx     = rng(-10.f, 10.f);
        e.fy     = rng(-10.f, 10.f);
        e.x      = static_cast<int>(roundf(e.fx));
        e.y      = static_cast<int>(roundf(e.fy));
        e.vx     = rng(-3.f, 3.f);
        e.vy     = rng(-3.f, 3.f);
        e.radius = rng(0.5f, 0.85f);
        e.color  = palette[i % 5];
        e.type   = static_cast<uint8_t>(i % 3);
        std::snprintf(e.label, sizeof(e.label), "#%d", static_cast<int>(e.id));
        w.entities.push_back(e);
    }
    w.player_id = static_cast<int>(w.entities[0].id);

    auto addCell = [&](int x, int y, uint8_t type, uint32_t color, const char* name) {
        Cell c{};
        c.x = x; c.y = y;
        c.type  = type;
        c.color = color;
        std::snprintf(c.label, sizeof(c.label), "%s", name);
        w.cells.push_back(c);
    };
    const uint32_t wall_col  = col32(90,  70,  70,  200);
    const uint32_t water_col = col32(40,  80,  160, 160);
    for (int x = -6; x <= -2; ++x) addCell(x,  0, 1, wall_col,  "Wall");
    for (int x =  2; x <=  6; ++x) addCell(x,  0, 1, wall_col,  "Wall");
    for (int y = -6; y <= -2; ++y) addCell(0, y,  1, wall_col,  "Wall");
    for (int y =  2; y <=  6; ++y) addCell(0, y,  1, wall_col,  "Wall");
    for (int x = -3; x <= -1; ++x)
        for (int y = -3; y <= -1; ++y)
            addCell(x, y, 2, water_col, "Water");

    // Extra cell layered on an existing wall position to exercise overlap display
    addCell(-6, 0, 3, col32(180, 80, 200, 180), "Trap");

    return w;
}

void TickMockWorld(World& w, float dt) {
    const float bounds = 15.f;
    for (auto& e : w.entities) {
        e.fx += e.vx * dt;
        e.fy += e.vy * dt;
        if (e.fx >  bounds) { e.fx =  bounds; e.vx = -e.vx; }
        if (e.fx < -bounds) { e.fx = -bounds; e.vx = -e.vx; }
        if (e.fy >  bounds) { e.fy =  bounds; e.vy = -e.vy; }
        if (e.fy < -bounds) { e.fy = -bounds; e.vy = -e.vy; }
        e.x = static_cast<int>(roundf(e.fx));
        e.y = static_cast<int>(roundf(e.fy));
    }
}

} // namespace dui
