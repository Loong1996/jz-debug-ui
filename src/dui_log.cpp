#include "dui_log.h"
#include "dui_metrics.h"
#include <imgui.h>
#include <windows.h>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace dui {
namespace {

struct LogEntry {
    LogLevel level;
    char     ts[16];   // "HH:MM:SS.mmm"
    char     text[256];
};

struct WatchEntry {
    char name[32];
    char value[64];
};

static RingBuffer<LogEntry, 1000> s_log;
static std::vector<WatchEntry>    s_watch;

static void make_timestamp(char* buf, size_t n) {
    using namespace std::chrono;
    auto now    = system_clock::now();
    auto ms     = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto t      = system_clock::to_time_t(now);
    struct tm tm_local;
#ifdef _WIN32
    localtime_s(&tm_local, &t);
#else
    localtime_r(&t, &tm_local);
#endif
    std::snprintf(buf, n, "%02d:%02d:%02d.%03d",
        tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec,
        static_cast<int>(ms.count()));
}

static void push_log(LogLevel level, const char* fmt, va_list ap) {
    LogEntry e{};
    e.level = level;
    make_timestamp(e.ts, sizeof(e.ts));
    std::vsnprintf(e.text, sizeof(e.text), fmt, ap);
    s_log.push(e);
}

static bool export_log(const char* filter, bool show_info, bool show_warn, bool show_error) {
    OPENFILENAMEW ofn = {};
    wchar_t path[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"txt";
    if (!GetSaveFileNameW(&ofn)) return false;

    FILE* f = nullptr;
    _wfopen_s(&f, path, L"wb");
    if (!f) return false;
    // UTF-8 BOM
    const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, 3, f);

    const int n     = s_log.size();
    const int cap   = s_log.capacity();
    const int start = s_log.plot_offset();
    for (int i = 0; i < n; i++) {
        const LogEntry& e = s_log.data()[(start + i) % cap];
        if (!show_info  && e.level == LogLevel::Info)  continue;
        if (!show_warn  && e.level == LogLevel::Warn)  continue;
        if (!show_error && e.level == LogLevel::Error) continue;
        if (filter[0] && std::strstr(e.text, filter) == nullptr) continue;
        const char* lv = e.level == LogLevel::Error ? "[ERR] "
                       : e.level == LogLevel::Warn  ? "[WRN] " : "[INF] ";
        std::fprintf(f, "[%s] %s%s\n", e.ts, lv, e.text);
    }
    fclose(f);
    return true;
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
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputText(u8"##过滤", filter, sizeof(filter));
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"复制全部")) {
        // Build clipboard string from visible entries
        std::string clip;
        const int n     = s_log.size();
        const int cap   = s_log.capacity();
        const int start = s_log.plot_offset();
        for (int i = 0; i < n; i++) {
            const LogEntry& e = s_log.data()[(start + i) % cap];
            if (!show_info  && e.level == LogLevel::Info)  continue;
            if (!show_warn  && e.level == LogLevel::Warn)  continue;
            if (!show_error && e.level == LogLevel::Error) continue;
            if (filter[0] && std::strstr(e.text, filter) == nullptr) continue;
            char line[300];
            std::snprintf(line, sizeof(line), "[%s] %s\n", e.ts, e.text);
            clip += line;
        }
        ImGui::SetClipboardText(clip.c_str());
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"导出...")) {
        export_log(filter, show_info, show_warn, show_error);
    }
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
        std::snprintf(line, sizeof(line), "[%s] %s", e.ts, e.text);

        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::PushID(i);
        ImGui::Selectable(line, false, ImGuiSelectableFlags_None);
        if (ImGui::BeginPopupContextItem("##log_ctx")) {
            if (ImGui::MenuItem(u8"复制此行")) {
                ImGui::SetClipboardText(line);
            }
            if (ImGui::MenuItem(u8"过滤此前缀")) {
                const char* sp = std::strchr(e.text, ':');
                size_t plen = sp ? static_cast<size_t>(sp - e.text + 1) : 0;
                if (plen > 0 && plen < sizeof(filter))
                    std::snprintf(filter, plen + 1, "%s", e.text);
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
        ImGui::PopStyleColor();
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
