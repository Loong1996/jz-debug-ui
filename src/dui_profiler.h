#pragma once
#include <cstdint>

namespace dui {

// RAII scope timer — stack-depth tracked automatically.
struct ProfileScope {
    explicit ProfileScope(const char* name);
    ~ProfileScope();
};

#define DUI_PROFILE_SCOPE(name) ::dui::ProfileScope _dui_ps_##__LINE__(name)

// Called by App::Tick at frame start/end to bracket the frame.
void BeginProfilerFrame_();
void EndProfilerFrame_();

// ImGui panel: flame chart + per-scope stats table.
void DrawProfiler();

} // namespace dui
