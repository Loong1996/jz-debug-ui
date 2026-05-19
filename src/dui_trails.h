#pragma once
#include "dui_world.h"
#include <cstdint>

struct ImDrawList;

namespace dui {

// Record the current fx/fy position of every entity in world.active_map
// into per-entity trail buffers. Call this wherever your state updates —
// e.g. after applying a server snapshot. Not called automatically.
void RecordTrailSnapshot(const World& world);

void SetTrailLength(int n);       // max samples per entity, default 60
void EnableEntityTrails(bool on); // master switch; default false
bool IsEntityTrailsEnabled();
void ClearAllTrails();
void ClearTrailsFor(uint64_t entity_id);

// Accumulate tile visit frequency: increment heat at each entity's current
// tile, then multiply all existing entries by the configured decay factor.
// Call this once per state update alongside RecordTrailSnapshot.
void  AccumulateTileVisits(const World& world);
void  SetTileVisitDecay(float decay); // 0 < decay < 1; default 0.99
                                      // closer to 1 = slower fade (longer trail)
                                      // closer to 0 = faster fade (shorter trail)
                                      // 0.99 → half-life ~70 calls (~1s at 60fps)
                                      // 0.90 → half-life ~7 calls  (~0.1s at 60fps)
float GetTileVisitHeat(uint32_t map_id, int x, int y); // 0 if never visited
void  ClearTileHeat();

// Internal: called by DrawCanvas when show_trails is true.
void InvokeTrails_(const World& world, ImDrawList* dl);

} // namespace dui
