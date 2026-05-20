#pragma once
#include <cstdint>

namespace dui {

struct EntityLogEntry {
    char    ts[12];   // "HH:MM:SS"
    uint8_t level;    // 0=Info 1=Warn 2=Error
    char    text[160];
};

void LogEntity     (uint64_t id, const char* fmt, ...);
void LogEntityWarn (uint64_t id, const char* fmt, ...);
void LogEntityError(uint64_t id, const char* fmt, ...);

void SetEntityLogSize(int n_per_entity); // default 32, max 1024
void ClearEntityLog  (uint64_t id);

// Returns entries oldest-first. out_count set to number of entries (may be 0).
// Pointer valid until the next call to GetEntityLog for any id.
const EntityLogEntry* GetEntityLog(uint64_t id, int* out_count);

void OnEntityDespawned_(uint64_t id); // called by DespawnEntity

} // namespace dui
