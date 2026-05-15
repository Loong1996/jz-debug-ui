#include "dui_mock.h"

namespace dui {

World MakeMockWorld() {
    World w;
    unsigned seed = 0xDEADBEEFu;
    auto lcg = [&]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 16) / 65535.f;
    };
    auto rng = [&](float lo, float hi) { return lo + lcg() * (hi - lo); };

    const uint32_t palette[5] = {
        RGBA(255, 80,  80),
        RGBA(80,  200, 80),
        RGBA(80,  120, 255),
        RGBA(255, 200, 50),
        RGBA(200, 80,  255),
    };

    // --- Map 0: 主城 (15 entities + walls / water / trap) ---
    for (int i = 0; i < 15; ++i)
        w.SpawnEntity(static_cast<uint64_t>(1000 + i))
         .SetMapId(0)
         .SetPos(rng(-10.f, 10.f), rng(-10.f, 10.f))
         .SetVel(rng(-3.f, 3.f),   rng(-3.f, 3.f))
         .SetRadius(rng(0.5f, 0.85f))
         .SetColor(palette[i % 5])
         .SetType(static_cast<uint8_t>(i % 3))
         .SetLabel("#%d", 1000 + i);

    w.player_id   = static_cast<int>(w.entities[0].id);
    w.selected_id = w.player_id;

    const uint32_t wall_col  = RGBA(90,  70,  70,  200);
    const uint32_t water_col = RGBA(40,  80,  160, 160);
    const uint32_t trap_col  = RGBA(180, 80,  200, 180);

    for (int x = -6; x <= -2; ++x) w.SpawnCell(x, 0).SetType(1).SetColor(wall_col).SetLabel("Wall");
    for (int x =  2; x <=  6; ++x) w.SpawnCell(x, 0).SetType(1).SetColor(wall_col).SetLabel("Wall");
    for (int y = -6; y <= -2; ++y) w.SpawnCell(0, y).SetType(1).SetColor(wall_col).SetLabel("Wall");
    for (int y =  2; y <=  6; ++y) w.SpawnCell(0, y).SetType(1).SetColor(wall_col).SetLabel("Wall");
    for (int x = -3; x <= -1; ++x)
        for (int y = -3; y <= -1; ++y)
            w.SpawnCell(x, y).SetType(2).SetColor(water_col).SetLabel("Water");
    w.SpawnCell(-6, 0).SetType(3).SetColor(trap_col).SetLabel("Trap");

    // --- Map 1: 副本 (8 entities + simple arena) ---
    const uint32_t dungeon_wall = RGBA(60,  50,  80,  220);
    const uint32_t lava_col     = RGBA(200, 60,  20,  180);

    for (int x = -3; x <= 3; ++x) {
        w.SpawnCell(x, -3).SetType(1).SetColor(dungeon_wall).SetLabel("Wall").SetMapId(1);
        w.SpawnCell(x,  3).SetType(1).SetColor(dungeon_wall).SetLabel("Wall").SetMapId(1);
    }
    for (int y = -2; y <= 2; ++y) {
        w.SpawnCell(-3, y).SetType(1).SetColor(dungeon_wall).SetLabel("Wall").SetMapId(1);
        w.SpawnCell( 3, y).SetType(1).SetColor(dungeon_wall).SetLabel("Wall").SetMapId(1);
    }
    for (int dx : {0, 1, -1}) w.SpawnCell(dx,  0).SetType(2).SetColor(lava_col).SetLabel("Lava").SetMapId(1);
    for (int dy : {1, -1})    w.SpawnCell( 0, dy).SetType(2).SetColor(lava_col).SetLabel("Lava").SetMapId(1);
    w.SpawnCell(-3, -3).SetType(3).SetColor(trap_col).SetLabel("Trap").SetMapId(1);

    for (int i = 0; i < 8; ++i)
        w.SpawnEntity(static_cast<uint64_t>(2000 + i))
         .SetMapId(1)
         .SetPos(rng(-2.f, 2.f), rng(-2.f, 2.f))
         .SetVel(rng(-2.f, 2.f), rng(-2.f, 2.f))
         .SetRadius(rng(0.4f, 0.75f))
         .SetColor(palette[(i + 2) % 5])
         .SetType(static_cast<uint8_t>(i % 2))
         .SetLabel("D#%d", 2000 + i);

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
        e.SetPos(e.fx, e.fy);
    }
}

} // namespace dui
