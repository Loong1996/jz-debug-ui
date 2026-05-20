#include "dui_entity_log.h"
#include <unordered_map>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <cstring>

namespace dui {

static int s_log_cap = 32;

struct EntityLog {
    std::vector<EntityLogEntry> buf;
    int head  = 0;
    int count = 0;

    void resize(int cap) {
        if (static_cast<int>(buf.size()) == cap) return;
        buf.assign(static_cast<size_t>(cap), EntityLogEntry{});
        head = count = 0;
    }

    void push(const EntityLogEntry& e) {
        int cap = static_cast<int>(buf.size());
        buf[head] = e;
        head = (head + 1) % cap;
        if (count < cap) ++count;
    }
};

static std::unordered_map<uint64_t, EntityLog> s_logs;
static std::vector<EntityLogEntry>              s_get_buf;

static void push_entry(uint64_t id, uint8_t level, const char* fmt, va_list ap) {
    auto& lg = s_logs[id];
    lg.resize(s_log_cap);
    EntityLogEntry e{};
    e.level = level;
    time_t t = time(nullptr);
    struct tm* tm_ = localtime(&t);
    if (tm_) std::snprintf(e.ts, sizeof(e.ts), "%02d:%02d:%02d", tm_->tm_hour, tm_->tm_min, tm_->tm_sec);
    vsnprintf(e.text, sizeof(e.text), fmt, ap);
    lg.push(e);
}

void LogEntity(uint64_t id, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); push_entry(id, 0, fmt, ap); va_end(ap);
}
void LogEntityWarn(uint64_t id, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); push_entry(id, 1, fmt, ap); va_end(ap);
}
void LogEntityError(uint64_t id, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); push_entry(id, 2, fmt, ap); va_end(ap);
}

void SetEntityLogSize(int n) {
    if (n < 1) n = 1; if (n > 1024) n = 1024;
    s_log_cap = n;
}

void ClearEntityLog(uint64_t id) { s_logs.erase(id); }

const EntityLogEntry* GetEntityLog(uint64_t id, int* out_count) {
    auto it = s_logs.find(id);
    if (it == s_logs.end() || it->second.count == 0) { *out_count = 0; return nullptr; }
    auto& lg = it->second;
    int cap = static_cast<int>(lg.buf.size());
    int cnt = lg.count;
    s_get_buf.resize(static_cast<size_t>(cnt));
    for (int i = 0; i < cnt; ++i) {
        int idx = (lg.head - cnt + i + cap) % cap;
        s_get_buf[i] = lg.buf[idx];
    }
    *out_count = cnt;
    return s_get_buf.data();
}

void OnEntityDespawned_(uint64_t id) { s_logs.erase(id); }

} // namespace dui
