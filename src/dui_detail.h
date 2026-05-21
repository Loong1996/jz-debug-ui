#pragma once
#include "dui_world.h"
#include <functional>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <cstdint>

namespace dui {

// ---- Builder-style detail text ----

class DetailBuilder {
public:
    DetailBuilder& Section(const char* name) {
        buf_ += "=== "; buf_ += name; buf_ += " ===\n"; return *this;
    }
    DetailBuilder& KV(const char* key, int         v) { return kvfmt(key, "%d",   v); }
    DetailBuilder& KV(const char* key, float       v) { return kvfmt(key, "%.3g", v); }
    DetailBuilder& KV(const char* key, uint64_t    v) { return kvfmt(key, "%llu", static_cast<unsigned long long>(v)); }
    DetailBuilder& KV(const char* key, const char* v) { return kvfmt(key, "%s",   v ? v : ""); }
    DetailBuilder& KVFmt(const char* key, const char* fmt, ...) {
        char vbuf[128]; va_list ap; va_start(ap, fmt); vsnprintf(vbuf, sizeof(vbuf), fmt, ap); va_end(ap);
        return kvfmt(key, "%s", vbuf);
    }
    DetailBuilder& Line(const char* fmt, ...) {
        char vbuf[256]; va_list ap; va_start(ap, fmt); vsnprintf(vbuf, sizeof(vbuf), fmt, ap); va_end(ap);
        buf_ += vbuf; buf_ += '\n'; return *this;
    }
    DetailBuilder& Blank() { buf_ += '\n'; return *this; }
    std::string Take() { std::string s; s.swap(buf_); return s; }

private:
    std::string buf_;
    DetailBuilder& kvfmt(const char* key, const char* vfmt, ...) {
        char vbuf[128]; va_list ap; va_start(ap, vfmt); vsnprintf(vbuf, sizeof(vbuf), vfmt, ap); va_end(ap);
        char line[192]; snprintf(line, sizeof(line), "%-8s: %s\n", key, vbuf);
        buf_ += line; return *this;
    }
};

// ---- Registration ----

using EntityDetailTextFn    = std::function<std::string(const Entity&)>;
using EntityDetailBuilderFn = std::function<void(const Entity&, DetailBuilder&)>;

void RegisterEntityDetailText    (uint8_t  type, EntityDetailTextFn    fn);
void RegisterEntityDetailText    (uint8_t  type, EntityDetailBuilderFn fn);
void UnregisterEntityDetailText  (uint8_t  type);
void ClearEntityDetailTexts();                     // clears all type + id registrations
void RegisterEntityDetailTextById (uint64_t id,  EntityDetailTextFn    fn);
void RegisterEntityDetailTextById (uint64_t id,  EntityDetailBuilderFn fn);
void UnregisterEntityDetailTextById(uint64_t id);

std::string InvokeEntityDetailText(const Entity& e);

// ---- Cell detail text (symmetric to RegisterEntityDetailText) ----
using CellDetailTextFn    = std::function<std::string(const Cell&)>;
using CellDetailBuilderFn = std::function<void(const Cell&, DetailBuilder&)>;

void RegisterCellDetailText  (uint8_t type, CellDetailTextFn    fn);
void RegisterCellDetailText  (uint8_t type, CellDetailBuilderFn fn);
void UnregisterCellDetailText(uint8_t type);
void ClearCellDetailTexts();

std::string InvokeCellDetailText(const Cell& c);

// ---- Live entity/cell editor (read-write; body uses ImGui::Drag*/Slider*/ColorEdit*) ----
using EntityEditFn = std::function<void(Entity&)>;
using CellEditFn   = std::function<void(Cell&)>;

void RegisterEntityEditor  (uint8_t type, EntityEditFn fn);
void UnregisterEntityEditor(uint8_t type);
void ClearEntityEditors();
void RegisterCellEditor    (uint8_t type, CellEditFn   fn);
void UnregisterCellEditor  (uint8_t type);
void ClearCellEditors();

// Internal — called by DrawEntityDetail and Inspector cell section.
bool InvokeCellDetailText_(const Cell& c);   // returns true if text was non-empty
bool InvokeEntityEditor_  (Entity& e);        // returns true if drawer registered
bool InvokeCellEditor_    (Cell& c);

void DrawEntityDetail(World& world);

} // namespace dui
