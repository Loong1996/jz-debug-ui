#pragma once
namespace dui {

bool  IsWorldPaused();
void  SetWorldPaused(bool paused);
bool  ToggleWorldPaused(); // returns new paused state

float GetTimeScale();
void  SetTimeScale(float s); // clamped [0.0625, 8.0]

void  RequestSingleStep(); // steps one raw_dt tick when paused

// Replace `dt` with this in your tick. Returns 0 when paused, raw_dt once on step,
// raw_dt * time_scale otherwise.
float EffectiveDt(float raw_dt);

} // namespace dui
