#pragma once
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

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

    static CommandArg Bool  (const char* n, bool def = false)
        { CommandArg a{}; a.name = n; a.type = ArgType::Bool;   a.default_v.b = def; return a; }
    static CommandArg Int   (const char* n, int def = 0, float mn = 0.f, float mx = 0.f)
        { CommandArg a{}; a.name = n; a.type = ArgType::Int;    a.default_v.i = def; a.min_v = mn; a.max_v = mx; return a; }
    static CommandArg Float (const char* n, float def = 0.f, float mn = 0.f, float mx = 0.f)
        { CommandArg a{}; a.name = n; a.type = ArgType::Float;  a.default_v.f = def; a.min_v = mn; a.max_v = mx; return a; }
    static CommandArg String(const char* n, const char* def = "")
        { CommandArg a{}; a.name = n; a.type = ArgType::String; a.default_str = def; return a; }
    static CommandArg Enum  (const char* n, int def, const char* const* items, int count)
        { CommandArg a{}; a.name = n; a.type = ArgType::Enum;   a.default_v.i = def; a.enum_items = items; a.enum_count = count; return a; }
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

// Initializer_list overload: args are copied internally — no lifetime concerns for the list.
void RegisterCommandWithArgs(const char* name,
                             std::initializer_list<CommandArg> args,
                             CommandFnArgs fn);

void DrawCommands();  // ImGui panel

// ---- Search / global access ----

struct CommandInfo {
    std::string name;
    bool        has_args;
    int         index;   // index in internal g_cmds or g_cmds_args
};
void GetAllCommands(std::vector<CommandInfo>& out);

// Trigger the args-modal for a with-args command by name.
// No-op if not found. DrawCommands() must be called this frame for the modal to appear.
void OpenArgsModalByName(const char* name);

} // namespace dui
