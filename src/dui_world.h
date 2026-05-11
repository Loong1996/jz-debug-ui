#pragma once
#include <vector>
#include <cstdint>
#include <cstdio>

namespace dui {

struct Entity {
    uint64_t id;
    float x, y;
    float vx, vy;
    float radius;
    uint32_t color;  // ABGR (IM_COL32 format: (A<<24)|(B<<16)|(G<<8)|R)
    char label[16];
};

struct World {
    std::vector<Entity> entities;
    int selected_id = -1;
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
        e.x      = rng(-10.f, 10.f);
        e.y      = rng(-10.f, 10.f);
        e.vx     = rng(-3.f,  3.f);
        e.vy     = rng(-3.f,  3.f);
        e.radius = rng(0.3f,  1.0f);
        e.color  = palette[i % 5];
        std::snprintf(e.label, sizeof(e.label), "#%d", static_cast<int>(e.id));
        w.entities.push_back(e);
    }
    return w;
}

inline void TickMockWorld(World& w, float dt) {
    const float bounds = 15.f;
    for (auto& e : w.entities) {
        e.x += e.vx * dt;
        e.y += e.vy * dt;
        if (e.x >  bounds) { e.x =  bounds; e.vx = -e.vx; }
        if (e.x < -bounds) { e.x = -bounds; e.vx = -e.vx; }
        if (e.y >  bounds) { e.y =  bounds; e.vy = -e.vy; }
        if (e.y < -bounds) { e.y = -bounds; e.vy = -e.vy; }
    }
}

} // namespace dui
