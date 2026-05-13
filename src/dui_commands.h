#pragma once
#include <functional>

namespace dui {

using CommandFn = std::function<void()>;

// Register a command by name. Names support "Category/Short Name" for grouping.
// Registering the same name again overwrites the previous handler.
void RegisterCommand(const char* name, CommandFn fn);
void UnregisterCommand(const char* name);

void DrawCommands();  // ImGui panel

} // namespace dui
