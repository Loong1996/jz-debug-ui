#pragma once
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cmath>

namespace dui {

struct Entity {
    uint64_t id;
    int   x,  y;   // grid position (integer coords)
    float fx, fy;  // float accumulator — drives x/y via rounding
    float vx, vy;  // velocity (grid units/sec)
    float radius;  // visual fill ratio within cell (0..1)
    uint32_t color;
    char label[16];
};

struct World {
    std::vector<Entity> entities;
    int selected_id = -1;
    int player_id   = -1;
};

inline World MakeMockWorld() {
    World w;
    unsigned seed = 0xDEADBEEFu;
    auto lcg = [&]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 16) / 65535.f;
    };
    auto rng = [&](float lo, float hi) { return lo + lcg() * (hi - lo); };
    auto col32 = [](uint8_t r, uint8_t g, uint8_t b) -> uint32_t {
        return (220u << 24) | (static_cast<uint32_t>(b) << 16)
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
        Entity e;
        e.id     = static_cast<uint64_t>(1000 + i);
        e.fx     = rng(-10.f, 10.f);
        e.fy     = rng(-10.f, 10.f);
        e.x      = static_cast<int>(roundf(e.fx));
        e.y      = static_cast<int>(roundf(e.fy));
        e.vx     = rng(-3.f, 3.f);
        e.vy     = rng(-3.f, 3.f);
        e.radius = rng(0.5f, 0.85f);
        e.color  = palette[i % 5];
        std::snprintf(e.label, sizeof(e.label), "#%d", static_cast<int>(e.id));
        w.entities.push_back(e);
    }
    w.player_id = static_cast<int>(w.entities[0].id);
    return w;
}

inline void TickMockWorld(World& w, float dt) {
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
