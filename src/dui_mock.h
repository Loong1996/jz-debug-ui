#pragma once
#include "dui_world.h"

namespace dui {
World MakeMockWorld();
void  TickMockWorld(World& w, float dt);
} // namespace dui
