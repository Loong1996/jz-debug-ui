#pragma once
#include <vector>
#include <cstdint>

namespace dui {

struct Entity {
    uint64_t id;
    int      x,  y;
    float    fx, fy;   // float accumulator — drives x/y via rounding
    float    vx, vy;   // velocity (grid units/sec)
    float    radius;   // visual fill ratio within cell (0..1)
    uint32_t color;
    uint8_t  type;     // caller-defined tag (0 = generic)
    char     label[16];
    void*    userdata = nullptr;  // caller-owned pointer; library does not interpret or free
};

struct Cell {
    int      x, y;
    uint32_t color;
    uint8_t  type;     // caller-defined tag
    char     label[12];
    void*    userdata = nullptr;  // caller-owned pointer; library does not interpret or free
};

struct World {
    std::vector<Entity> entities;
    std::vector<Cell>   cells;
    int  selected_id   = -1;
    int  player_id     = -1;
    bool sel_cell_valid = false;
    int  sel_cell_x    = 0;
    int  sel_cell_y    = 0;
};

} // namespace dui
