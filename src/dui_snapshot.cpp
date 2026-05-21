#include "dui_snapshot.h"
#include "dui_pins.h"
#include "dui_log.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// Pull in Win32 for the file dialogs without poisoning ImPlot macros.
#pragma push_macro("SetProp")
#pragma push_macro("GetProp")
#undef SetProp
#undef GetProp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#pragma pop_macro("SetProp")
#pragma pop_macro("GetProp")

namespace dui {

// ---- Serialisation ----------------------------------------------------------

static void write_str_escaped(FILE* f, const char* s) {
    fputc('"', f);
    for (; *s; ++s) {
        if      (*s == '"')  fputs("\\\"", f);
        else if (*s == '\\') fputs("\\\\", f);
        else if (*s == '\n') fputs("\\n",  f);
        else                 fputc(*s, f);
    }
    fputc('"', f);
}

bool SaveWorldSnapshot(const World& w, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) { LogError(u8"snapshot: cannot open '%s' for write", path); return false; }

    fprintf(f, "{\n");
    fprintf(f, "  \"active_map_id\": %u,\n",  w.active_map_id);
    fprintf(f, "  \"selected_id\": %llu,\n",  static_cast<unsigned long long>(w.selected_id));
    fprintf(f, "  \"follower_id\": %llu,\n",    static_cast<unsigned long long>(w.follower_id));

    // entities
    fprintf(f, "  \"entities\": [\n");
    for (size_t i = 0; i < w.entities.size(); ++i) {
        const Entity& e = w.entities[i];
        fprintf(f, "    { \"id\": %llu, \"map_id\": %u, \"type\": %u, "
                   "\"fx\": %.6g, \"fy\": %.6g, \"vx\": %.6g, \"vy\": %.6g, "
                   "\"radius\": %.6g, \"color\": \"0x%08X\", \"label\": ",
                static_cast<unsigned long long>(e.id),
                e.map_id, static_cast<unsigned>(e.type),
                e.fx, e.fy, e.vx, e.vy, e.radius,
                static_cast<unsigned>(e.color));
        write_str_escaped(f, e.label);
        fprintf(f, " }%s\n", (i + 1 < w.entities.size()) ? "," : "");
    }
    fprintf(f, "  ],\n");

    // cells
    fprintf(f, "  \"cells\": [\n");
    for (size_t i = 0; i < w.cells.size(); ++i) {
        const Cell& c = w.cells[i];
        fprintf(f, "    { \"map_id\": %u, \"x\": %d, \"y\": %d, \"type\": %u, "
                   "\"color\": \"0x%08X\", \"label\": ",
                c.map_id, c.x, c.y, static_cast<unsigned>(c.type),
                static_cast<unsigned>(c.color));
        write_str_escaped(f, c.label);
        fprintf(f, " }%s\n", (i + 1 < w.cells.size()) ? "," : "");
    }
    fprintf(f, "  ],\n");

    // pins
    const auto& pins = ListPins();
    fprintf(f, "  \"pins\": [\n");
    for (size_t i = 0; i < pins.size(); ++i) {
        const Pin& p = pins[i];
        fprintf(f, "    { \"map_id\": %u, \"wx\": %.6g, \"wy\": %.6g, "
                   "\"color\": \"0x%08X\", \"text\": ",
                p.map_id, p.wx, p.wy, static_cast<unsigned>(p.color));
        write_str_escaped(f, p.text.c_str());
        fprintf(f, " }%s\n", (i + 1 < pins.size()) ? "," : "");
    }
    fprintf(f, "  ]\n");

    fprintf(f, "}\n");
    fclose(f);
    Log(u8"snapshot saved: %s", path);
    return true;
}

// ---- Parsing ----------------------------------------------------------------
// Minimal hand-rolled parser for the fixed schema we produce above.

struct Parser {
    const char* p;
    const char* end;

    bool eof()  const { return p >= end; }
    char peek() const { return eof() ? '\0' : *p; }
    void skip_ws() {
        while (!eof() && (peek()==' '||peek()=='\t'||peek()=='\n'||peek()=='\r')) ++p;
    }
    bool consume(char c) { skip_ws(); if (peek()==c) { ++p; return true; } return false; }
    bool expect(char c)  { if (!consume(c)) { return false; } return true; }

    // Read a JSON string into buf (up to bufsz-1 chars). Returns false on error.
    bool read_string(char* buf, int bufsz) {
        skip_ws();
        if (peek() != '"') return false;
        ++p;
        int n = 0;
        while (!eof() && peek() != '"') {
            char c = *p++;
            if (c == '\\' && !eof()) {
                c = *p++;
                if      (c == '"')  c = '"';
                else if (c == '\\') c = '\\';
                else if (c == 'n')  c = '\n';
            }
            if (n < bufsz - 1) buf[n++] = c;
        }
        buf[n] = '\0';
        expect('"');
        return true;
    }

    // Read a JSON string into a std::string.
    bool read_string(std::string& out) {
        char buf[256];
        if (!read_string(buf, sizeof(buf))) return false;
        out = buf;
        return true;
    }

    // Read a number as double.
    bool read_number(double& out) {
        skip_ws();
        char* endp = nullptr;
        out = strtod(p, &endp);
        if (endp == p) return false;
        p = endp;
        return true;
    }

    // Expect the literal key "name": (with surrounding quotes + colon).
    bool expect_key(const char* name) {
        skip_ws();
        if (peek() != '"') return false;
        ++p;
        size_t nlen = strlen(name);
        if (static_cast<size_t>(end - p) < nlen) return false;
        if (strncmp(p, name, nlen) != 0) { p -= 1; return false; }
        p += nlen;
        if (!expect('"')) return false;
        return expect(':');
    }

    uint32_t parse_color_hex() {
        // Expects "0xABCDEF12" (quoted hex)
        skip_ws();
        if (peek() != '"') return 0;
        ++p;
        skip_ws();
        unsigned v = 0;
        if (peek() == '0') { ++p; if (peek()=='x'||peek()=='X') ++p; }
        while (!eof()) {
            char c = peek();
            if      (c >= '0' && c <= '9') { v = v*16 + (c-'0');      ++p; }
            else if (c >= 'a' && c <= 'f') { v = v*16 + (c-'a'+10);   ++p; }
            else if (c >= 'A' && c <= 'F') { v = v*16 + (c-'A'+10);   ++p; }
            else break;
        }
        expect('"');
        return static_cast<uint32_t>(v);
    }
};

bool LoadWorldSnapshot(World& w, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { LogError(u8"snapshot: cannot open '%s' for read", path); return false; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024 * 1024) { fclose(f); return false; }

    std::vector<char> buf(static_cast<size_t>(sz) + 1);
    size_t rd = fread(buf.data(), 1, static_cast<size_t>(sz), f);
    fclose(f);
    buf[rd] = '\0';

    Parser ps{ buf.data(), buf.data() + rd };

    // We parse key by key in expected order; any mismatch is a hard fail.
    auto fail = [&](const char* msg) -> bool {
        LogError(u8"snapshot parse error: %s", msg);
        return false;
    };

    if (!ps.expect('{')) return fail("expected {");

    // active_map_id
    if (!ps.expect_key("active_map_id")) return fail("active_map_id");
    double v = 0; if (!ps.read_number(v)) return fail("active_map_id value");
    uint32_t new_active_map_id = static_cast<uint32_t>(v);
    ps.consume(',');

    // selected_id
    if (!ps.expect_key("selected_id")) return fail("selected_id");
    if (!ps.read_number(v)) return fail("selected_id value");
    uint64_t new_selected_id = static_cast<uint64_t>(v);
    ps.consume(',');

    // follower_id
    if (!ps.expect_key("follower_id")) return fail("follower_id");
    if (!ps.read_number(v)) return fail("follower_id value");
    uint64_t new_follower_id = static_cast<uint64_t>(v);
    ps.consume(',');

    // entities
    if (!ps.expect_key("entities")) return fail("entities key");
    if (!ps.expect('[')) return fail("entities [");
    std::vector<Entity> new_entities;
    ps.skip_ws();
    while (ps.peek() != ']') {
        if (!ps.expect('{')) return fail("entity {");
        Entity e;
        if (!ps.expect_key("id"))      return fail("entity.id");
        if (!ps.read_number(v)) return fail("entity.id val");
        e.id = static_cast<uint64_t>(v); ps.consume(',');

        if (!ps.expect_key("map_id"))  return fail("entity.map_id");
        if (!ps.read_number(v)) return fail("entity.map_id val");
        e.map_id = static_cast<uint32_t>(v); ps.consume(',');

        if (!ps.expect_key("type"))    return fail("entity.type");
        if (!ps.read_number(v)) return fail("entity.type val");
        e.type = static_cast<uint8_t>(v); ps.consume(',');

        if (!ps.expect_key("fx"))      return fail("entity.fx");
        if (!ps.read_number(v)) return fail("entity.fx val");
        e.fx = static_cast<float>(v); ps.consume(',');
        if (!ps.expect_key("fy"))      return fail("entity.fy");
        if (!ps.read_number(v)) return fail("entity.fy val");
        e.fy = static_cast<float>(v); ps.consume(',');
        e.SetPos(e.fx, e.fy);

        if (!ps.expect_key("vx"))      return fail("entity.vx");
        if (!ps.read_number(v)) return fail("entity.vx val");
        e.vx = static_cast<float>(v); ps.consume(',');
        if (!ps.expect_key("vy"))      return fail("entity.vy");
        if (!ps.read_number(v)) return fail("entity.vy val");
        e.vy = static_cast<float>(v); ps.consume(',');

        if (!ps.expect_key("radius"))  return fail("entity.radius");
        if (!ps.read_number(v)) return fail("entity.radius val");
        e.radius = static_cast<float>(v); ps.consume(',');

        if (!ps.expect_key("color"))   return fail("entity.color");
        e.color = ps.parse_color_hex(); ps.consume(',');

        if (!ps.expect_key("label"))   return fail("entity.label");
        char lb[16]; ps.read_string(lb, sizeof(lb));
        std::snprintf(e.label, sizeof(e.label), "%s", lb);

        if (!ps.expect('}')) return fail("entity }");
        new_entities.push_back(e);
        ps.consume(',');
        ps.skip_ws();
    }
    ps.expect(']');
    ps.consume(',');

    // cells
    if (!ps.expect_key("cells")) return fail("cells key");
    if (!ps.expect('[')) return fail("cells [");
    std::vector<Cell> new_cells;
    ps.skip_ws();
    while (ps.peek() != ']') {
        if (!ps.expect('{')) return fail("cell {");
        Cell c;
        if (!ps.expect_key("map_id"))  return fail("cell.map_id");
        if (!ps.read_number(v)) return fail("cell.map_id val");
        c.map_id = static_cast<uint32_t>(v); ps.consume(',');

        if (!ps.expect_key("x"))       return fail("cell.x");
        if (!ps.read_number(v)) return fail("cell.x val");
        c.x = static_cast<int>(v); ps.consume(',');
        if (!ps.expect_key("y"))       return fail("cell.y");
        if (!ps.read_number(v)) return fail("cell.y val");
        c.y = static_cast<int>(v); ps.consume(',');

        if (!ps.expect_key("type"))    return fail("cell.type");
        if (!ps.read_number(v)) return fail("cell.type val");
        c.type = static_cast<uint8_t>(v); ps.consume(',');

        if (!ps.expect_key("color"))   return fail("cell.color");
        c.color = ps.parse_color_hex(); ps.consume(',');

        if (!ps.expect_key("label"))   return fail("cell.label");
        char lb[12]; ps.read_string(lb, sizeof(lb));
        std::snprintf(c.label, sizeof(c.label), "%s", lb);

        if (!ps.expect('}')) return fail("cell }");
        new_cells.push_back(c);
        ps.consume(',');
        ps.skip_ws();
    }
    ps.expect(']');
    ps.consume(',');

    // pins
    if (!ps.expect_key("pins")) return fail("pins key");
    if (!ps.expect('[')) return fail("pins [");
    std::vector<Pin> new_pins;
    ps.skip_ws();
    while (ps.peek() != ']') {
        if (!ps.expect('{')) return fail("pin {");
        Pin p;
        if (!ps.expect_key("map_id"))  return fail("pin.map_id");
        if (!ps.read_number(v)) return fail("pin.map_id val");
        p.map_id = static_cast<uint32_t>(v); ps.consume(',');

        if (!ps.expect_key("wx"))      return fail("pin.wx");
        if (!ps.read_number(v)) return fail("pin.wx val");
        p.wx = static_cast<float>(v); ps.consume(',');
        if (!ps.expect_key("wy"))      return fail("pin.wy");
        if (!ps.read_number(v)) return fail("pin.wy val");
        p.wy = static_cast<float>(v); ps.consume(',');

        if (!ps.expect_key("color"))   return fail("pin.color");
        p.color = ps.parse_color_hex(); ps.consume(',');

        if (!ps.expect_key("text"))    return fail("pin.text");
        ps.read_string(p.text);

        if (!ps.expect('}')) return fail("pin }");
        new_pins.push_back(std::move(p));
        ps.consume(',');
        ps.skip_ws();
    }
    ps.expect(']');

    // Commit everything
    w.entities       = std::move(new_entities);
    w.cells          = std::move(new_cells);
    w.active_map_id  = new_active_map_id;
    w.selected_id    = new_selected_id;
    w.selected_ids.clear();
    if (new_selected_id) w.selected_ids.push_back(new_selected_id);
    w.follower_id      = new_follower_id;
    w.sel_cell_valid = false;

    ClearAllPins();
    for (auto& pin : new_pins)
        AddPin(pin.map_id, pin.wx, pin.wy, pin.text.c_str(), pin.color);

    Log(u8"snapshot loaded: %s", path);
    return true;
}

// ---- File dialogs -----------------------------------------------------------

static bool open_save_dialog(wchar_t* out_path, int out_len) {
    out_path[0] = L'\0';
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"JSON Snapshot\0*.json\0All Files\0*.*\0";
    ofn.lpstrFile   = out_path;
    ofn.nMaxFile    = static_cast<DWORD>(out_len);
    ofn.lpstrDefExt = L"json";
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    return GetSaveFileNameW(&ofn) != 0;
}

static bool open_load_dialog(wchar_t* out_path, int out_len) {
    out_path[0] = L'\0';
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"JSON Snapshot\0*.json\0All Files\0*.*\0";
    ofn.lpstrFile   = out_path;
    ofn.nMaxFile    = static_cast<DWORD>(out_len);
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&ofn) != 0;
}

bool SaveWorldSnapshotDialog(const World& w) {
    wchar_t path[MAX_PATH] = {};
    if (!open_save_dialog(path, MAX_PATH)) return false;
    char narrow[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, sizeof(narrow), nullptr, nullptr);
    return SaveWorldSnapshot(w, narrow);
}

bool LoadWorldSnapshotDialog(World& w) {
    wchar_t path[MAX_PATH] = {};
    if (!open_load_dialog(path, MAX_PATH)) return false;
    char narrow[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, sizeof(narrow), nullptr, nullptr);
    return LoadWorldSnapshot(w, narrow);
}

} // namespace dui
