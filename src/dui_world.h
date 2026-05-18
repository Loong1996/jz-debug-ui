#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace dui {

inline uint32_t RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 220) {
    return (static_cast<uint32_t>(a) << 24)
         | (static_cast<uint32_t>(b) << 16)
         | (static_cast<uint32_t>(g) << 8) | r;
}

struct Entity {
    uint64_t id      = 0;
    uint32_t map_id  = 0;
    int      x = 0,  y = 0;
    float    fx = 0.f, fy = 0.f;  // float accumulator — drives x/y via rounding
    float    vx = 0.f, vy = 0.f;  // velocity (grid units/sec)
    float    radius  = 0.8f;      // visual fill ratio within cell (0..1)
    uint32_t color   = RGBA(180, 180, 180);
    uint8_t  type    = 0;         // caller-defined tag (0 = generic)
    char     label[16] = {};
    void*    userdata  = nullptr; // caller-owned pointer; library does not interpret or free

    // SetPos syncs fx/fy and derives x/y via rounding
    Entity& SetPos(float fx_, float fy_) {
        fx = fx_; fy = fy_;
        x  = static_cast<int>(roundf(fx));
        y  = static_cast<int>(roundf(fy));
        return *this;
    }
    Entity& SetVel(float vx_, float vy_)  { vx = vx_; vy = vy_; return *this; }
    Entity& SetRadius(float r)            { radius = r; return *this; }
    Entity& SetType(uint8_t t)            { type = t; return *this; }
    Entity& SetMapId(uint32_t mid)        { map_id = mid; return *this; }
    Entity& SetUserdata(void* ud)         { userdata = ud; return *this; }
    Entity& SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 220) {
        color = RGBA(r, g, b, a); return *this;
    }
    Entity& SetColor(uint32_t packed) { color = packed; return *this; }
    Entity& SetLabel(const char* fmt, ...) {
        va_list ap; va_start(ap, fmt);
        vsnprintf(label, sizeof(label), fmt, ap);
        va_end(ap);
        return *this;
    }
};

struct Cell {
    uint32_t map_id  = 0;
    int      x = 0,  y = 0;
    uint32_t color   = RGBA(120, 120, 120);
    uint8_t  type    = 0;         // caller-defined tag
    char     label[12] = {};
    void*    userdata  = nullptr; // caller-owned pointer; library does not interpret or free

    Cell& SetPos(int x_, int y_)  { x = x_; y = y_; return *this; }
    Cell& SetType(uint8_t t)      { type = t; return *this; }
    Cell& SetMapId(uint32_t mid)  { map_id = mid; return *this; }
    Cell& SetUserdata(void* ud)   { userdata = ud; return *this; }
    Cell& SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 220) {
        color = RGBA(r, g, b, a); return *this;
    }
    Cell& SetColor(uint32_t packed) { color = packed; return *this; }
    Cell& SetLabel(const char* fmt, ...) {
        va_list ap; va_start(ap, fmt);
        vsnprintf(label, sizeof(label), fmt, ap);
        va_end(ap);
        return *this;
    }
};

struct World {
    std::vector<Entity>   entities;
    std::vector<Cell>     cells;
    uint32_t active_map_id = 0;
    int  selected_id    = -1;   // primary selection; -1 when nothing selected
    int  player_id      = -1;
    bool sel_cell_valid = false;
    int  sel_cell_x     = 0;
    int  sel_cell_y     = 0;
    std::vector<uint64_t> selected_ids; // multi-selection set; always includes selected_id's entity

    // Returns a reference to the new entity. Do not store the reference
    // across further SpawnEntity calls (vector reallocation may invalidate it).
    Entity& SpawnEntity(uint64_t id) {
        entities.push_back(Entity{});
        entities.back().id = id;
        return entities.back();
    }

    Cell& SpawnCell(int x, int y) {
        cells.push_back(Cell{});
        cells.back().x = x;
        cells.back().y = y;
        return cells.back();
    }
};

} // namespace dui
