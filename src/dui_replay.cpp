#include "dui_replay.h"
#include "dui_time.h"
#include "dui_menubar.h"
#include <imgui.h>
#include <vector>
#include <cstdio>

namespace dui {

static bool s_recording     = false;
static int  s_buf_size      = 600;
static bool s_replay_active = false;
static int  s_cursor        = 0;
static bool s_was_paused    = false;

struct ReplayFrame { uint32_t frame_idx; World world; };

static std::vector<ReplayFrame> s_buf;
static int s_head  = 0;
static int s_count = 0;

// ---- public API ----

void EnableReplayRecording(bool on) {
    s_recording = on;
    if (!on) { s_buf.clear(); s_buf.resize(static_cast<size_t>(s_buf_size)); s_head = s_count = 0; }
}
bool IsReplayRecording() { return s_recording; }

void SetReplayBufferSize(int n) {
    if (n < 1) n = 1;
    s_buf_size = n;
    s_buf.clear(); s_buf.resize(static_cast<size_t>(n));
    s_head = s_count = 0;
}
int GetReplayBufferFrames() { return s_count; }

bool IsReplayActive() { return s_replay_active; }

void EnterReplay() {
    if (s_count == 0) return;
    s_was_paused    = IsWorldPaused();
    SetWorldPaused(true);
    s_replay_active = true;
    s_cursor        = s_count - 1;
}

void ExitReplay() {
    s_replay_active = false;
    SetWorldPaused(s_was_paused);
}

void SetReplayCursor(int f) {
    if (f < 0) f = 0;
    if (f >= s_count) f = s_count - 1;
    s_cursor = f;
}
int GetReplayCursor() { return s_cursor; }

const World& SelectActiveWorld_(const World& real_world) {
    if (!s_replay_active || s_count == 0) return real_world;
    int oldest = (s_count < s_buf_size) ? 0 : s_head;
    int idx    = (oldest + s_cursor) % s_buf_size;
    return s_buf[idx].world;
}

void CaptureReplayFrame_(const World& w) {
    if (!s_recording || s_replay_active) return;
    if (static_cast<int>(s_buf.size()) != s_buf_size) {
        s_buf.clear(); s_buf.resize(static_cast<size_t>(s_buf_size)); s_head = s_count = 0;
    }
    s_buf[s_head].frame_idx = static_cast<uint32_t>(s_count);
    s_buf[s_head].world     = w;
    s_head = (s_head + 1) % s_buf_size;
    if (s_count < s_buf_size) ++s_count;
}

// ---- panel ----

void DrawReplayPanel(World& world) {
    (void)world;
    if (!ImGui::Begin(u8"回放", &BuiltinPanelOpenRef(u8"回放")))
        { ImGui::End(); return; }

    // REC toggle
    if (s_recording) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(160, 35, 35, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 50, 50, 255));
        if (ImGui::Button(u8"● REC")) EnableReplayRecording(false);
        ImGui::PopStyleColor(2);
    } else {
        if (ImGui::Button(u8"○ REC")) EnableReplayRecording(true);
    }
    ImGui::SameLine();
    ImGui::Text(u8"容量 %d  /  已录 %d", s_buf_size, s_count);
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"清空")) {
        s_buf.clear(); s_buf.resize(static_cast<size_t>(s_buf_size));
        s_head = s_count = 0;
        if (s_replay_active) ExitReplay();
    }

    ImGui::Separator();

    if (s_count == 0) {
        ImGui::TextDisabled(u8"(暂无录制数据。先点 ○ REC 开始录制)");
        ImGui::End();
        return;
    }

    // Playback buttons
    static bool s_playing = false;

    if (ImGui::Button(u8"⏮")) { if (!s_replay_active) EnterReplay(); s_cursor = 0; s_playing = false; }
    ImGui::SameLine();
    if (ImGui::Button(u8"◀"))  { if (!s_replay_active) EnterReplay(); if (s_cursor > 0) --s_cursor; s_playing = false; }
    ImGui::SameLine();

    if (s_replay_active && s_playing) {
        if (ImGui::Button(u8"⏸")) s_playing = false;
    } else {
        if (ImGui::Button(u8"▶")) { if (!s_replay_active) EnterReplay(); s_playing = true; }
    }

    ImGui::SameLine();
    if (ImGui::Button(u8"▶|")) { if (!s_replay_active) EnterReplay(); if (s_cursor < s_count-1) ++s_cursor; s_playing = false; }
    ImGui::SameLine();
    if (ImGui::Button(u8"⏭"))  { if (!s_replay_active) EnterReplay(); s_cursor = s_count - 1; s_playing = false; }
    ImGui::SameLine();

    if (!s_replay_active) {
        if (ImGui::Button(u8"进入回放")) EnterReplay();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(160, 60, 35, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 85, 50, 255));
        if (ImGui::Button(u8"返回实时")) { ExitReplay(); s_playing = false; }
        ImGui::PopStyleColor(2);
    }

    // Auto-advance when playing
    if (s_replay_active && s_playing) {
        if (s_cursor < s_count - 1) ++s_cursor;
        else                         s_playing = false;
    }

    // Timeline slider
    {
        int cur = s_replay_active ? s_cursor : (s_count - 1);
        if (ImGui::SliderInt("##replay_slider", &cur, 0, s_count - 1)) {
            if (!s_replay_active) EnterReplay();
            s_cursor  = cur;
            s_playing = false;
        }
        ImGui::SameLine();
        ImGui::Text("%d / %d", (s_replay_active ? s_cursor : (s_count - 1)) + 1, s_count);
    }

    ImGui::End();
}

} // namespace dui
