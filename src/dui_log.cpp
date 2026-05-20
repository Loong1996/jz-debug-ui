#include "dui_log.h"
#include "dui_metrics.h"
#include "dui_menubar.h"
#include <imgui.h>
#include <windows.h>
#include <algorithm>
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
    char  name[64];
    char  value[64];
    bool  is_numeric = false;
    RingBuffer<float, 64> history;
};

struct TunableEntry {
    char  name[64];
    enum class Kind : uint8_t { Float, Int, Bool } kind;
    float lo, hi;
    union { float* fp; int* ip; bool* bp; } ptr;
};

static RingBuffer<LogEntry, 1000> s_log;
static std::vector<WatchEntry>    s_watch;
static std::vector<TunableEntry>  s_tunables;

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

static bool export_watch_csv() {
    OPENFILENAMEW ofn = {};
    wchar_t path[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"csv";
    if (!GetSaveFileNameW(&ofn)) return false;
    FILE* f = nullptr;
    _wfopen_s(&f, path, L"wb");
    if (!f) return false;
    const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, 3, f);
    std::fprintf(f, "name,value\n");
    for (const auto& w : s_watch)
        std::fprintf(f, "\"%s\",\"%s\"\n", w.name, w.value);
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
    for (auto& w : s_watch)
        if (std::strcmp(w.name, name) == 0) { w.is_numeric = true; w.history.push(static_cast<float>(v)); break; }
}
void Watch(const char* name, float v) {
    char buf[64]; std::snprintf(buf, sizeof(buf), "%.4g", v);
    Watch(name, static_cast<const char*>(buf));
    for (auto& w : s_watch)
        if (std::strcmp(w.name, name) == 0) { w.is_numeric = true; w.history.push(v); break; }
}
void Watch(const char* name, bool v) {
    Watch(name, v ? "true" : "false");
}
void Watch(const char* name, float x, float y) {
    char buf[64]; std::snprintf(buf, sizeof(buf), "(%.2f, %.2f)", x, y);
    Watch(name, static_cast<const char*>(buf));
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
    s_watch.push_back(std::move(e));
}

void RemoveWatch(const char* name) {
    s_watch.erase(std::remove_if(s_watch.begin(), s_watch.end(),
        [name](const WatchEntry& w) { return std::strcmp(w.name, name) == 0; }),
        s_watch.end());
}
void ClearWatch() {
    s_watch.clear();
}

void Tunable(const char* name, float* value, float lo, float hi) {
    if (!name || !value) return;
    for (auto& t : s_tunables)
        if (std::strcmp(t.name, name) == 0) { t.ptr.fp = value; t.lo = lo; t.hi = hi; return; }
    TunableEntry t{}; std::snprintf(t.name, sizeof(t.name), "%s", name);
    t.kind = TunableEntry::Kind::Float; t.lo = lo; t.hi = hi; t.ptr.fp = value;
    s_tunables.push_back(t);
}
void Tunable(const char* name, int* value, int lo, int hi) {
    if (!name || !value) return;
    for (auto& t : s_tunables)
        if (std::strcmp(t.name, name) == 0) { t.ptr.ip = value; t.lo = static_cast<float>(lo); t.hi = static_cast<float>(hi); return; }
    TunableEntry t{}; std::snprintf(t.name, sizeof(t.name), "%s", name);
    t.kind = TunableEntry::Kind::Int; t.lo = static_cast<float>(lo); t.hi = static_cast<float>(hi); t.ptr.ip = value;
    s_tunables.push_back(t);
}
void Tunable(const char* name, bool* value) {
    if (!name || !value) return;
    for (auto& t : s_tunables)
        if (std::strcmp(t.name, name) == 0) { t.ptr.bp = value; return; }
    TunableEntry t{}; std::snprintf(t.name, sizeof(t.name), "%s", name);
    t.kind = TunableEntry::Kind::Bool; t.ptr.bp = value;
    s_tunables.push_back(t);
}
void RemoveTunable(const char* name) {
    s_tunables.erase(std::remove_if(s_tunables.begin(), s_tunables.end(),
        [name](const TunableEntry& t) { return std::strcmp(t.name, name) == 0; }),
        s_tunables.end());
}

void DrawLog() {
    static bool show_info  = true;
    static bool show_warn  = true;
    static bool show_error = true;
    static char filter[128] = {};

    if (!ImGui::Begin(u8"日志", &BuiltinPanelOpenRef(u8"日志")))
        { ImGui::End(); return; }

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

static void draw_sparkline(const WatchEntry& w) {
    int cnt = w.history.plot_count();
    if (cnt < 2) return;
    int off = w.history.plot_offset();
    int cap = w.history.capacity();
    const float* data = w.history.data();

    float vmin = data[off % cap], vmax = vmin;
    for (int i = 1; i < cnt; i++) {
        float v = data[(off + i) % cap];
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
    }

    // Trend color from last third of samples
    int seg = cnt / 3; if (seg < 1) seg = 1;
    float first = data[(off + cnt - 1 - seg) % cap];
    float last  = data[(off + cnt - 1) % cap];
    float slope = last - first;
    ImU32 col = slope >  0.01f * (vmax - vmin + 0.001f) ? IM_COL32(80, 220, 80, 220)
              : slope < -0.01f * (vmax - vmin + 0.001f) ? IM_COL32(220, 80, 80, 220)
              : IM_COL32(80, 160, 220, 220);

    ImVec2 pos  = ImGui::GetCursorScreenPos();
    float  w_px = ImGui::GetContentRegionAvail().x - 2.f;
    float  h_px = ImGui::GetTextLineHeight() * 1.2f;
    if (w_px < 10.f) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float range = vmax - vmin;
    auto pt = [&](int i) -> ImVec2 {
        float v = data[(off + i) % cap];
        float nx = static_cast<float>(i) / static_cast<float>(cnt - 1);
        float ny = (range > 0.f) ? 1.f - (v - vmin) / range : 0.5f;
        return ImVec2(pos.x + nx * w_px, pos.y + ny * h_px);
    };
    for (int i = 0; i < cnt - 1; i++)
        dl->AddLine(pt(i), pt(i + 1), col, 1.f);
    ImVec2 tip = pt(cnt - 1);
    dl->AddCircleFilled(tip, 2.f, col);

    ImGui::Dummy(ImVec2(w_px, h_px));
}

void DrawWatch() {
    if (!ImGui::Begin(u8"监视", &BuiltinPanelOpenRef(u8"监视")))
        { ImGui::End(); return; }

    static char watch_filter[64] = {};
    float csv_btn_w = ImGui::CalcTextSize("CSV...").x
                    + ImGui::GetStyle().FramePadding.x * 2.f + 6.f;
    ImGui::SetNextItemWidth(-csv_btn_w);
    ImGui::InputTextWithHint("##wf", u8"过滤名称", watch_filter, sizeof(watch_filter));
    ImGui::SameLine();
    if (ImGui::SmallButton("CSV...")) export_watch_csv();
    ImGui::Separator();

    // --- Tunables section ---
    if (!s_tunables.empty() &&
        ImGui::CollapsingHeader(u8"调节器##tunables", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto& t : s_tunables) {
            ImGui::PushID(t.name);
            const char* lbl = std::strchr(t.name, '/');
            lbl = lbl ? lbl + 1 : t.name;
            switch (t.kind) {
                case TunableEntry::Kind::Float:
                    ImGui::SetNextItemWidth(-1.f);
                    ImGui::SliderFloat(lbl, t.ptr.fp, t.lo, t.hi);
                    break;
                case TunableEntry::Kind::Int:
                    ImGui::SetNextItemWidth(-1.f);
                    ImGui::SliderInt(lbl, t.ptr.ip,
                                     static_cast<int>(t.lo), static_cast<int>(t.hi));
                    break;
                case TunableEntry::Kind::Bool:
                    ImGui::Checkbox(lbl, t.ptr.bp);
                    break;
            }
            ImGui::PopID();
        }
        ImGui::Separator();
    }

    auto pass_watch = [&](const WatchEntry& w) {
        return watch_filter[0] == '\0' || std::strstr(w.name, watch_filter) != nullptr;
    };

    if (ImGui::BeginTable("##watch_tbl", 3,
            ImGuiTableFlags_Borders     |
            ImGuiTableFlags_RowBg       |
            ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn(u8"名称", ImGuiTableColumnFlags_WidthStretch, 0.40f);
        ImGui::TableSetupColumn(u8"值",   ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn(u8"趋势", ImGuiTableColumnFlags_WidthFixed,   80.f);
        ImGui::TableHeadersRow();

        // Collect groups preserving insertion order, respecting filter
        std::vector<std::string> groups;
        for (const auto& w : s_watch) {
            if (!pass_watch(w)) continue;
            const char* slash = std::strchr(w.name, '/');
            std::string g = slash ? std::string(w.name, static_cast<size_t>(slash - w.name)) : u8"默认";
            bool found = false;
            for (const auto& gg : groups) if (gg == g) { found = true; break; }
            if (!found) groups.push_back(g);
        }

        std::string remove_name;
        std::string remove_group;

        for (const auto& grp : groups) {
            // Group header row — collapsible TreeNode
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::PushID(("grp_" + grp).c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.f));
            bool open = ImGui::TreeNodeEx(grp.c_str(),
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAllColumns);
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.f));
            if (ImGui::SmallButton(u8"清空"))
                remove_group = grp;
            ImGui::PopStyleColor();
            ImGui::PopID();

            // Entries — only rendered when group is expanded
            if (open) {
                for (int idx = 0; idx < static_cast<int>(s_watch.size()); idx++) {
                    WatchEntry& w = s_watch[idx];
                    if (!pass_watch(w)) continue;
                    const char* sl = std::strchr(w.name, '/');
                    std::string wg = sl ? std::string(w.name, static_cast<size_t>(sl - w.name)) : u8"默认";
                    if (wg != grp) continue;

                    const char* display_name = sl ? sl + 1 : w.name;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID(idx);
                    ImGui::Selectable(display_name, false,
                        ImGuiSelectableFlags_AllowOverlap, ImVec2(0, 0));
                    if (ImGui::IsItemHovered()) {
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.f));
                        if (ImGui::SmallButton("x"))
                            remove_name = w.name;
                        ImGui::PopStyleColor();
                    }
                    ImGui::PopID();

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(w.value);

                    ImGui::TableSetColumnIndex(2);
                    if (w.is_numeric)
                        draw_sparkline(w);
                }
                ImGui::TreePop();
            }
        }

        ImGui::EndTable();

        if (!remove_name.empty()) {
            RemoveWatch(remove_name.c_str());
        } else if (!remove_group.empty()) {
            s_watch.erase(std::remove_if(s_watch.begin(), s_watch.end(),
                [&](const WatchEntry& w) {
                    const char* sl = std::strchr(w.name, '/');
                    std::string wg = sl ? std::string(w.name, static_cast<size_t>(sl - w.name)) : u8"默认";
                    return wg == remove_group;
                }), s_watch.end());
        }
    }

    ImGui::End();
}

} // namespace dui
