#include "dui_profiler.h"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <vector>

namespace dui {

using Clock   = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct ScopeRecord {
    const char* name;
    double      t_start; // seconds from frame start
    double      t_end;
    int         depth;
};

// ---- Per-frame live recording ----
namespace {
static std::vector<ScopeRecord> s_live;
static std::vector<double>      s_depth_start;  // start time stack per depth
static TimePoint                s_frame_start;
static int                      s_cur_depth = 0;
static bool                     s_recording = false;

// Completed-frame snapshot displayed by DrawProfiler
static std::vector<ScopeRecord> s_last_frame;
static double                   s_last_frame_ms = 0.0;

// Rolling average over the last N frames per scope name
struct RollingEntry {
    char   name[64];
    double sum_ms;
    int    count;
    double last_ms;
};
static std::vector<RollingEntry> s_rolling;
static const int kRollingN = 60;

} // anonymous namespace

void BeginProfilerFrame_() {
    s_live.clear();
    s_depth_start.clear();
    s_frame_start = Clock::now();
    s_cur_depth   = 0;
    s_recording   = true;
}

void EndProfilerFrame_() {
    s_recording = false;
    // Close any unclosed scopes (shouldn't happen in correct usage)
    double frame_ms = std::chrono::duration<double>(Clock::now() - s_frame_start).count() * 1000.0;
    s_last_frame    = s_live;
    s_last_frame_ms = frame_ms;

    // Update rolling averages
    for (const auto& r : s_live) {
        double ms = (r.t_end - r.t_start) * 1000.0;
        RollingEntry* found = nullptr;
        for (auto& re : s_rolling)
            if (strcmp(re.name, r.name) == 0) { found = &re; break; }
        if (!found) {
            s_rolling.push_back({});
            found = &s_rolling.back();
            std::snprintf(found->name, sizeof(found->name), "%s", r.name);
        }
        found->sum_ms += ms;
        found->count  = std::min(found->count + 1, kRollingN);
        found->last_ms = ms;
    }
}

ProfileScope::ProfileScope(const char* name) {
    if (!s_recording) return;
    ScopeRecord r;
    r.name    = name;
    r.t_start = std::chrono::duration<double>(Clock::now() - s_frame_start).count();
    r.t_end   = r.t_start;
    r.depth   = s_cur_depth++;
    s_live.push_back(r);
    s_depth_start.push_back(r.t_start);
}

ProfileScope::~ProfileScope() {
    if (!s_recording) return;
    if (s_cur_depth <= 0) return;
    --s_cur_depth;
    // Find the last open scope at this depth (last pushed onto live)
    for (int i = static_cast<int>(s_live.size()) - 1; i >= 0; --i) {
        if (s_live[i].depth == s_cur_depth && s_live[i].t_end == s_live[i].t_start) {
            s_live[i].t_end = std::chrono::duration<double>(Clock::now() - s_frame_start).count();
            break;
        }
    }
    if (!s_depth_start.empty()) s_depth_start.pop_back();
}

// ---- Flame chart drawing ----

void DrawProfiler() {
    ImGui::Begin(u8"Profiler");

    ImGui::Text(u8"帧总耗时: %.3f ms", s_last_frame_ms);
    ImGui::Separator();

    if (s_last_frame.empty()) {
        ImGui::TextDisabled(u8"(无数据 — 请用 DUI_PROFILE_SCOPE 包裹代码段)");
        ImGui::End();
        return;
    }

    // ---- Flame chart ----
    const float kRowH   = 18.f;
    const float kPad    = 2.f;
    const float kMinW   = 2.f;

    int max_depth = 0;
    for (const auto& r : s_last_frame)
        if (r.depth > max_depth) max_depth = r.depth;

    float chart_h = (max_depth + 1) * (kRowH + kPad) + kPad;
    ImVec2 canvas_size(ImGui::GetContentRegionAvail().x, chart_h);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    // Background
    dl->AddRectFilled(p0, ImVec2(p0.x + canvas_size.x, p0.y + canvas_size.y),
                      IM_COL32(25, 25, 25, 200), 4.f);

    double frame_dur = s_last_frame_ms / 1000.0;
    if (frame_dur <= 0.0) frame_dur = 1e-6;

    for (const auto& r : s_last_frame) {
        double dur   = r.t_end - r.t_start;
        float  xf    = static_cast<float>(r.t_start / frame_dur);
        float  xw    = static_cast<float>(dur       / frame_dur);

        float x0 = p0.x + xf * canvas_size.x;
        float x1 = p0.x + (xf + xw) * canvas_size.x;
        if (x1 - x0 < kMinW) x1 = x0 + kMinW;

        float y0 = p0.y + kPad + r.depth * (kRowH + kPad);
        float y1 = y0 + kRowH;

        // Color palette: hue based on name hash
        unsigned h = 2166136261u;
        for (const char* s = r.name; *s; ++s) h = (h ^ (unsigned char)*s) * 16777619u;
        uint8_t hue8 = static_cast<uint8_t>(h & 0xFF);
        // Map hue to a warm/cool color
        float hue = hue8 / 255.f;
        float cr = 0.f, cg = 0.f, cb = 0.f;
        ImGui::ColorConvertHSVtoRGB(hue, 0.7f, 0.85f, cr, cg, cb);
        uint32_t col = IM_COL32(
            static_cast<int>(cr * 255),
            static_cast<int>(cg * 255),
            static_cast<int>(cb * 255), 210);

        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), col, 2.f);
        dl->AddRect      (ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(0, 0, 0, 80), 2.f, 0, 1.f);

        if (x1 - x0 > 24.f) {
            char label[80];
            std::snprintf(label, sizeof(label), "%s  %.2fms", r.name, dur * 1000.0);
            float tw = ImGui::CalcTextSize(label).x;
            if (tw > x1 - x0 - 4.f)
                std::snprintf(label, sizeof(label), "%.2fms", dur * 1000.0);
            dl->AddText(ImVec2(x0 + 3.f, y0 + 2.f), IM_COL32(240, 240, 240, 230), label);
        }

        // Tooltip on hover
        ImGui::SetCursorScreenPos(ImVec2(x0, y0));
        ImGui::InvisibleButton(r.name, ImVec2(x1 - x0, kRowH));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%.4f ms", r.name, dur * 1000.0);
    }

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + chart_h + 4.f));
    ImGui::Dummy(ImVec2(canvas_size.x, 0.f));

    ImGui::Separator();

    // ---- Stats table ----
    if (!s_rolling.empty()) {
        if (ImGui::BeginTable("##pf_tbl", 3,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn(u8"名称",     ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(u8"上帧 ms",  ImGuiTableColumnFlags_WidthFixed, 70.f);
            ImGui::TableSetupColumn(u8"均值 ms",  ImGuiTableColumnFlags_WidthFixed, 70.f);
            ImGui::TableHeadersRow();
            for (const auto& re : s_rolling) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(re.name);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", re.last_ms);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", re.count > 0 ? re.sum_ms / re.count : 0.0);
            }
            ImGui::EndTable();
        }
    }

    ImGui::End();
}

} // namespace dui
