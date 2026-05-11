#include "dui_log.h"
#include "dui_metrics.h"
#include <imgui.h>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

namespace dui {
namespace {

static const auto s_start = std::chrono::steady_clock::now();

static float now_sec() {
    return std::chrono::duration<float>(
        std::chrono::steady_clock::now() - s_start).count();
}

struct LogEntry {
    LogLevel level;
    float    time;
    char     text[256];
};

struct WatchEntry {
    char name[32];
    char value[64];
};

static RingBuffer<LogEntry, 1000> s_log;
static std::vector<WatchEntry>    s_watch;

static void push_log(LogLevel level, const char* fmt, va_list ap) {
    LogEntry e{};
    e.level = level;
    e.time  = now_sec();
    std::vsnprintf(e.text, sizeof(e.text), fmt, ap);
    s_log.push(e);
}

} // anonymous namespace

void Log(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); push_log(LogLevel::Info,  fmt, ap); va_end(ap);
}
void LogWarn(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); push_log(LogLevel::Warn,  fmt, ap); va_end(ap);
}
void LogError(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); push_log(LogLevel::Error, fmt, ap); va_end(ap);
}

void Watch(const char* name, int v) {
    char buf[64]; std::snprintf(buf, sizeof(buf), "%d", v);
    Watch(name, static_cast<const char*>(buf));
}
void Watch(const char* name, float v) {
    char buf[64]; std::snprintf(buf, sizeof(buf), "%.4g", v);
    Watch(name, static_cast<const char*>(buf));
}
void Watch(const char* name, bool v) {
    Watch(name, v ? "true" : "false");
}
void Watch(const char* name, const char* v) {
    for (auto& w : s_watch) {
        if (std::strcmp(w.name, name) == 0) {
            std::snprintf(w.value, sizeof(w.value), "%s", v);
            return;
        }
    }
    WatchEntry e{};
    std::snprintf(e.name,  sizeof(e.name),  "%s", name);
    std::snprintf(e.value, sizeof(e.value), "%s", v);
    s_watch.push_back(e);
}

void DrawLog() {
    static bool show_info  = true;
    static bool show_warn  = true;
    static bool show_error = true;
    static char filter[128] = {};

    ImGui::Begin(u8"日志");

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    ImGui::Checkbox("Info",  &show_info);  ImGui::SameLine();
    ImGui::Checkbox("Warn",  &show_warn);  ImGui::SameLine();
    ImGui::Checkbox("Error", &show_error); ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText(u8"##过滤", filter, sizeof(filter));
    ImGui::PopStyleVar();

    ImGui::Separator();

    const float footer = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##log_scroll", ImVec2(0, -footer), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.f;

    const int n     = s_log.size();
    const int cap   = s_log.capacity();
    const int start = s_log.plot_offset();

    for (int i = 0; i < n; ++i) {
        const LogEntry& e = s_log.data()[(start + i) % cap];

        if (!show_info  && e.level == LogLevel::Info)  continue;
        if (!show_warn  && e.level == LogLevel::Warn)  continue;
        if (!show_error && e.level == LogLevel::Error) continue;
        if (filter[0] != '\0' && std::strstr(e.text, filter) == nullptr) continue;

        ImVec4 col;
        if      (e.level == LogLevel::Error) col = ImVec4(1.f, 0.40f, 0.40f, 1.f);
        else if (e.level == LogLevel::Warn)  col = ImVec4(1.f, 0.85f, 0.20f, 1.f);
        else                                  col = ImVec4(0.85f, 0.85f, 0.85f, 1.f);

        char line[320];
        std::snprintf(line, sizeof(line), "[%.2f] %s", e.time, e.text);
        ImGui::TextColored(col, "%s", line);
    }

    if (at_bottom)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}

void DrawWatch() {
    ImGui::Begin(u8"监视");

    if (ImGui::BeginTable("##watch_tbl", 2,
            ImGuiTableFlags_Borders     |
            ImGuiTableFlags_RowBg       |
            ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn(u8"名称", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn(u8"值",   ImGuiTableColumnFlags_WidthStretch, 0.55f);
        ImGui::TableHeadersRow();

        for (const auto& w : s_watch) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(w.name);
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(w.value);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace dui
