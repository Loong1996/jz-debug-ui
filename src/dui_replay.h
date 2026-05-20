#pragma once
#include "dui_world.h"

namespace dui {

void EnableReplayRecording(bool on); // off by default — no overhead when off
bool IsReplayRecording();

void SetReplayBufferSize(int frames); // default 600 (~10s @ 60fps)
int  GetReplayBufferFrames();         // number of recorded frames so far

bool IsReplayActive();  // true while scrubbing / watching a historical frame
void EnterReplay();     // pause world + switch to most-recent captured frame
void ExitReplay();      // restore previous pause state + return to live

void SetReplayCursor(int frame); // 0 = oldest, GetReplayBufferFrames()-1 = newest
int  GetReplayCursor();

// Internal — called once per frame by DrawAll (only records when !replay_active)
void CaptureReplayFrame_(const World& w);

// Returns historical world when replay is active, real_world otherwise.
// Read-only — used by DrawCanvas, DrawInspector, DrawEntityDetail for rendering.
const World& SelectActiveWorld_(const World& real_world);

void DrawReplayPanel(World& world);

} // namespace dui
