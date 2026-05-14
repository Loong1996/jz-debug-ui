#pragma once
#include <functional>

namespace dui {

using CommandFn = std::function<void()>;

// Register a zero-argument command. Names support "Category/Short Name" for grouping.
// Registering the same name again overwrites the previous handler.
void RegisterCommand  (const char* name, CommandFn fn);
void UnregisterCommand(const char* name);

// Execute a command by name (used by hotkey system). No-op if name not found.
void ExecuteCommand(const char* name);

// ---- Parameterized commands ----

enum class ArgType { Bool, Int, Float, String, Enum };

struct CommandArg {
    const char* name;            // form label
    ArgType     type;
    union { bool b; int i; float f; } default_v = {};
    const char*        default_str = nullptr;   // String default (may be nullptr)
    const char* const* enum_items  = nullptr;   // Enum options (caller must keep alive)
    int                enum_count  = 0;
    float              min_v       = 0.f;       // min_v == max_v → no range clamp shown
    float              max_v       = 0.f;
};

struct CommandArgValue {
    bool        b;
    int         i;
    float       f;
    const char* s;   // points into persistent per-form buffer — valid until next Execute call
};

using CommandFnArgs = std::function<void(const CommandArgValue* values, int count)>;

// Register a command that opens a parameter form before executing.
// args and their string pointers must remain valid for the lifetime of the registration.
void RegisterCommandWithArgs(const char* name,
                             const CommandArg* args, int arg_count,
                             CommandFnArgs fn);

void DrawCommands();  // ImGui panel

} // namespace dui
