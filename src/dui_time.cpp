#include "dui_time.h"

namespace dui {

static bool  s_paused       = false;
static float s_time_scale   = 1.f;
static bool  s_step_pending = false;

bool  IsWorldPaused()     { return s_paused; }
bool  ToggleWorldPaused() { s_paused = !s_paused; return s_paused; }
void  SetWorldPaused(bool p) { s_paused = p; }

float GetTimeScale() { return s_time_scale; }
void  SetTimeScale(float s) {
    if (s < 0.0625f) s = 0.0625f;
    if (s > 8.f)     s = 8.f;
    s_time_scale = s;
}

void RequestSingleStep() {
    if (s_paused) s_step_pending = true;
}

float EffectiveDt(float raw_dt) {
    if (s_step_pending) {
        s_step_pending = false;
        s_paused = true;
        return raw_dt;
    }
    if (s_paused) return 0.f;
    return raw_dt * s_time_scale;
}

} // namespace dui
