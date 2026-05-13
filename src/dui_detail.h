#pragma once
#include "dui_world.h"
#include <functional>
#include <string>
#include <cstdint>

namespace dui {

using EntityDetailTextFn = std::function<std::string(const Entity&)>;

void RegisterEntityDetailText(uint8_t type, EntityDetailTextFn fn);
std::string InvokeEntityDetailText(const Entity& e);

void DrawEntityDetail(World& world);

} // namespace dui
