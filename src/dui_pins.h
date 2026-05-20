#pragma once
#include "dui_world.h"
#include <cstdint>
#include <string>
#include <vector>

struct ImDrawList;

namespace dui {

struct Pin {
    uint64_t    id;
    uint32_t    map_id;
    float       wx, wy;
    uint32_t    color;
    std::string text;
};

// Returns the new pin's id (non-zero). color defaults to gold.
uint64_t AddPin(uint32_t map_id, float wx, float wy,
                const char* text, uint32_t color = RGBA(255, 200, 0));
bool     RemovePin(uint64_t pin_id);
void     ClearAllPins();
void     ClearPinsOnMap(uint32_t map_id);
const std::vector<Pin>& ListPins();

// Internal — called by DrawCanvas and DrawInspector.
void DrawPinsOverlay_(const World& world, ImDrawList* dl);
void DrawPinsPanel_  (World& world);

} // namespace dui
