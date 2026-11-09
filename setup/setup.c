/* Streets of Rage Collection Setup: the installer.
 *
 * A wizard in the collection's look: welcome, the install folder, the player's own ROMs
 * (each checked by its SHA-1 as it is picked, any game may be skipped), the install, done.
 * The collection's source travels inside Setup.exe, a zip appended to the program with an
 * 8-byte length and the tag SORPAYLD after it (the release workflow appends it); the install
 * unpacks it into <folder>\source and runs tools\install.ps1 there, hidden, showing its
 * progress lines ("@@ <percent> <what>", "@@error <why>"). The ROMs are copied into
 * <folder>\roms, so the games keep working if the originals move.
 *
 * Setup.exe copies itself into the folder and registers the collection under
 * HKCU\...\Uninstall: Modify runs it again (to add a game or rebuild), Uninstall runs it with
 * --uninstall. Run from a source folder without a payload, --source DIR uses that folder in
 * place (for testing). --shot PAGE F.png renders a page to a PNG; --install-test unpacks and
 * builds into --folder without a window, shortcuts or registry entry, printing the progress. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <bcrypt.h>
#include <commdlg.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "png.h"
#include "ui.h"

#define APP_NAME   "Streets of Rage Collection"
#define UNINST_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\StreetsOfRageCollection"
#define VERSION    "2.6.2"

/* ---- The games and their ROMs --------------------------------------------------------- */
typedef struct {
    const char *name, *title, *rom_name, *exe;
    uint32_t accent;
    char problem[160];                  /* why the last pick was not taken */
    bool installed;                     /* already in the folder */
} Game;

static Game games[3] = {
    { "sor1", "Streets of Rage", "Streets of Rage (World)", "StreetsOfRage.exe", 0xF8D838, "", false },
    { "sor2", "Streets of Rage 2", "Streets of Rage 2 (USA)", "StreetsOfRage2.exe", 0x7CCBFF, "", false },
    { "sor3", "Streets of Rage 3", "Streets of Rage 3 (USA)", "StreetsOfRage3.exe", 0xFFB848, "", false },
};

/* The games page's rows: one per game, and under Streets of Rage 3 its two optional
 * editions, each picked on its own row (Bare Knuckle III is built on top of the USA ROM). */
enum { ROW_SOR1, ROW_SOR2, ROW_SOR3, ROW_BK3, ROW_EN, ROW_COUNT };
static const int row_game[ROW_COUNT] = { 0, 1, 2, 2, 2 };
static const char *const row_title[ROW_COUNT] = { "Streets of Rage", "Streets of Rage 2", "Streets of Rage 3", "Bare Knuckle III", "Bare Knuckle III in English" };
static const char *const row_about[ROW_COUNT] = {
    "Streets of Rage (World), also sold as Bare Knuckle.",
    "Streets of Rage 2 (USA).",
    "Streets of Rage 3 (USA). Needed for the two optional rows below.",
    "Bare Knuckle III (Japan): the Japanese original, played as an edition of Streets of Rage 3.",
    "Twilight Translations' patch, bk3_100.ips or its .zip. Setup applies it to Bare Knuckle III.",
};
static char row_problem[ROW_COUNT][200];   /* why the last pick on that row was not taken */
static bool row_installed[ROW_COUNT];

/* Every cartridge a game builds from (games/<name>/game.json: its own, then its other
 * editions, each built into the same game). A game may have several picked. */
typedef struct {
    int game;
    const char *id, *sha1;
    bool main;                          /* the game's own (its file keeps the game's name) */
    char picked[MAX_PATH];              /* the ROM file to install (a zip's ROM is unpacked first) */
    char shown[MAX_PATH];               /* what the player picked */
} Rom;

static Rom roms[] = {
    { 0, "main", "3d74dbc81f3472a5bde45bf265e636a72a314667", true, "", "" },
    { 1, "main", "8b656eec9692d88bbbb84787142aa732b44ce0be", true, "", "" },
    { 2, "us", "40a33dd6f9dab0aff26c7525c9b8f342482c7af6", true, "", "" },
    { 2, "jp", "b9b2b4a98a9d8f4c49aa1e5395e2279339517fdb", false, "", "" },   /* Bare Knuckle III (Japan) */
    { 2, "en", "d1dc85abe0889ff8e880edcaf908a6b195b75952", false, "", "" },   /* with Twilight Translations' English patch 1.00 */
};
#define ROMS ((int)(sizeof roms / sizeof *roms))

static bool game_picked(int g) {
    for (int r = 0; r < ROMS; r++) if (roms[r].game == g && roms[r].picked[0]) return true;
    return false;
}

/* The patch picked for the English Bare Knuckle III (see make_english). */
static char ips_file[MAX_PATH], ips_shown[MAX_PATH];

static int rom_index(const char *id);

/* Skip on a row: its own pick (Streets of Rage 3's row keeps the optional editions). */
static void row_skip(int row) {
    if (row < ROW_SOR3) {
        for (int r = 0; r < ROMS; r++) if (roms[r].game == row) roms[r].picked[0] = roms[r].shown[0] = 0;
    } else if (row == ROW_SOR3) {
        roms[rom_index("us")].picked[0] = roms[rom_index("us")].shown[0] = 0;
    } else {
        roms[rom_index("en")].picked[0] = roms[rom_index("en")].shown[0] = 0;
        if (row == ROW_BK3) roms[rom_index("jp")].picked[0] = roms[rom_index("jp")].shown[0] = 0;
        else ips_file[0] = 0;
    }
    row_problem[row][0] = 0;
}

/* The row a cartridge is shown on. */
static int row_of_rom(int r) {
    if (roms[r].game < 2) return roms[r].game;
    return !strcmp(roms[r].id, "jp") ? ROW_BK3 : !strcmp(roms[r].id, "en") ? ROW_EN : ROW_SOR3;
}

/* Bare Knuckle III picked, but no Streets of Rage 3 (USA) to build it on. */
static bool bk3_needs_us(void) {
    bool bk3 = roms[rom_index("jp")].picked[0] || roms[rom_index("en")].picked[0] || ips_file[0];
    return bk3 && !roms[rom_index("us")].picked[0] && !games[2].installed;
}

static bool sha1_file(const char *path, char out[41]) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE h = NULL;
    bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, NULL, 0) == 0 &&
              BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0) == 0;
    static unsigned char buf[1 << 16];
    size_t n;
    while (ok && (n = fread(buf, 1, sizeof buf, f)) > 0) ok = BCryptHashData(h, buf, (ULONG)n, 0) == 0;
    unsigned char d[20];
    ok = ok && BCryptFinishHash(h, d, 20, 0) == 0;
    if (h) BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    fclose(f);
    if (ok) for (int k = 0; k < 20; k++) sprintf(out + k * 2, "%02x", d[k]);
    return ok;
}

/* Runs a command line hidden and waits; its exit code, or -1. */
static int run_hidden(const char *cmd, const char *dir) {
    STARTUPINFOA si = { .cb = sizeof si, .dwFlags = STARTF_USESHOWWINDOW, .wShowWindow = SW_HIDE };
    PROCESS_INFORMATION pi;
    char line[4096];
    snprintf(line, sizeof line, "%s", cmd);
    if (!CreateProcessA(NULL, line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, dir, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return (int)code;
}

static const char *tar_exe(void) {
    static char p[MAX_PATH];
    GetSystemDirectoryA(p, MAX_PATH);
    strcat(p, "\\tar.exe");             /* Windows' own: Git's reads C:\ as a host */
    return p;
}

static int rom_index(const char *id) {
    for (int r = 0; r < ROMS; r++) if (roms[r].game == 2 && !strcmp(roms[r].id, id)) return r;
    return -1;
}

/* Bare Knuckle III in English: Twilight Translations' patch (bk3_100.ips, or its zip), which
 * Setup applies to the Japanese ROM once it has both, in either order (make_english). */

/* Applies an IPS patch to `rom` (n bytes, grown up to cap); the new size, or 0 if it is not one. */
static size_t ips_apply(uint8_t *rom, size_t n, size_t cap, const uint8_t *p, size_t len) {
    if (len < 8 || memcmp(p, "PATCH", 5)) return 0;
    size_t i = 5;
    while (i + 3 <= len && memcmp(p + i, "EOF", 3)) {
        if (i + 5 > len) return 0;
        size_t off = (size_t)p[i] << 16 | p[i + 1] << 8 | p[i + 2], size = (size_t)p[i + 3] << 8 | p[i + 4];
        i += 5;
        if (size) {                     /* the bytes */
            if (i + size > len || off + size > cap) return 0;
            memcpy(rom + off, p + i, size);
            i += size;
        } else {                        /* a run of one byte */
            if (i + 3 > len) return 0;
            size = (size_t)p[i] << 8 | p[i + 1];
            if (off + size > cap) return 0;
            memset(rom + off, p[i + 2], size);
            i += 3;
        }
        if (off + size > n) n = off + size;
    }
    if (i + 6 <= len) n = (size_t)p[i + 3] << 16 | p[i + 4] << 8 | p[i + 5];   /* a size to cut to */
    return n;
}

static uint8_t *read_all(const char *path, size_t *n, size_t extra) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = size > 0 ? malloc((size_t)size + extra) : NULL;
    if (buf && fread(buf, 1, (size_t)size, f) != (size_t)size) { free(buf); buf = NULL; }
    fclose(f);
    *n = buf ? (size_t)size : 0;
    return buf;
}

/* With the patch and the Japanese ROM both picked: the English ROM, made in the temp folder. */
static void make_english(void) {
    int jp = rom_index("jp"), en = rom_index("en");
    if (!ips_file[0]) return;
    if (!roms[jp].picked[0]) {
        snprintf(row_problem[ROW_EN], sizeof row_problem[ROW_EN], "Patch taken. Now pick Bare Knuckle III (Japan) on the row above.");
        return;
    }
    size_t n, plen;
    const size_t cap = 8u << 20;
    uint8_t *rom = read_all(roms[jp].picked, &n, cap), *patch = read_all(ips_file, &plen, 0);
    if (rom && n > cap) n = 0;
    size_t out = rom && patch && n ? ips_apply(rom, n, cap, patch, plen) : 0;
    char tmp[MAX_PATH], sum[41] = "";
    GetTempPathA(MAX_PATH, tmp);
    snprintf(tmp + strlen(tmp), MAX_PATH - strlen(tmp), "sor_setup_bk3_en.md");
    FILE *f = out ? fopen(tmp, "wb") : NULL;
    bool ok = f && fwrite(rom, 1, out, f) == out;
    if (f) fclose(f);
    free(rom); free(patch);
    if (ok && sha1_file(tmp, sum) && !strcmp(sum, roms[en].sha1)) {
        snprintf(roms[en].picked, MAX_PATH, "%s", tmp);
        snprintf(roms[en].shown, MAX_PATH, "%s", ips_shown);
        row_problem[ROW_EN][0] = 0;
    } else {
        snprintf(row_problem[ROW_EN], sizeof row_problem[ROW_EN], "Not the patch this takes: Twilight Translations' 1.00, bk3_100.ips.");
        ips_file[0] = 0;
    }
}

/* A ROM picked for slot `slot`: checked, unpacked from a zip if need be, and put in the
 * slot of the game it really is (beside that game's other editions). A patch (.ips, or a zip
 * with one) is kept for the Japanese Bare Knuckle III. */
static void take_rom(int slot, const char *path) {
    char file[MAX_PATH], sum[41] = "";
    snprintf(file, sizeof file, "%s", path);
    const char *dot = strrchr(path, '.');
    if (dot && !_stricmp(dot, ".ips")) {
        snprintf(ips_file, sizeof ips_file, "%s", path);
        snprintf(ips_shown, sizeof ips_shown, "%s", path);
        make_english();
        return;
    }
    if (dot && !_stricmp(dot, ".zip")) {
        char tmp[MAX_PATH], cmd[2 * MAX_PATH + 64];
        GetTempPathA(MAX_PATH, tmp);
        static int zips;                /* a folder each: a game may take several */
        snprintf(tmp + strlen(tmp), MAX_PATH - strlen(tmp), "sor_setup_%d_%d", slot, zips++);
        CreateDirectoryA(tmp, NULL);
        snprintf(cmd, sizeof cmd, "\"%s\" -xf \"%s\" -C \"%s\"", tar_exe(), path, tmp);
        file[0] = 0;
        if (run_hidden(cmd, NULL) == 0) {
            const char *exts[3] = { "*.md", "*.bin", "*.gen" };
            for (int e = 0; e < 3 && !file[0]; e++) {
                char pat[MAX_PATH];
                WIN32_FIND_DATAA fd;
                snprintf(pat, sizeof pat, "%s\\%s", tmp, exts[e]);
                HANDLE fh = FindFirstFileA(pat, &fd);
                if (fh != INVALID_HANDLE_VALUE) { snprintf(file, sizeof file, "%s\\%s", tmp, fd.cFileName); FindClose(fh); }
            }
            if (!file[0]) {             /* a patch's zip (bk3_100.zip keeps it in a folder) */
                ips_file[0] = 0;
                char pat[MAX_PATH];
                WIN32_FIND_DATAA fd;
                const char *where[2] = { "%s\\*.ips", "%s\\*\\*.ips" };
                for (int k = 0; k < 2 && !ips_file[0]; k++) {
                    snprintf(pat, sizeof pat, where[k], tmp);
                    if (k) {            /* one folder down: find the folder first */
                        char dir[MAX_PATH];
                        snprintf(dir, sizeof dir, "%s\\*", tmp);
                        HANDLE dh = FindFirstFileA(dir, &fd);
                        if (dh == INVALID_HANDLE_VALUE) break;
                        do {
                            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
                            char sub[MAX_PATH];
                            WIN32_FIND_DATAA fi;
                            snprintf(sub, sizeof sub, "%s\\%s\\*.ips", tmp, fd.cFileName);
                            HANDLE fh = FindFirstFileA(sub, &fi);
                            if (fh != INVALID_HANDLE_VALUE) { snprintf(ips_file, sizeof ips_file, "%s\\%s\\%s", tmp, fd.cFileName, fi.cFileName); FindClose(fh); }
                        } while (!ips_file[0] && FindNextFileA(dh, &fd));
                        FindClose(dh);
                    } else {
                        HANDLE fh = FindFirstFileA(pat, &fd);
                        if (fh != INVALID_HANDLE_VALUE) { snprintf(ips_file, sizeof ips_file, "%s\\%s", tmp, fd.cFileName); FindClose(fh); }
                    }
                }
                if (ips_file[0]) { snprintf(ips_shown, sizeof ips_shown, "%s", path); make_english(); return; }
            }
        }
        if (!file[0]) { snprintf(row_problem[slot], sizeof row_problem[slot], "No ROM or patch in that zip."); return; }
    }
    if (!sha1_file(file, sum)) { snprintf(row_problem[slot], sizeof row_problem[slot], "Could not read that file."); return; }
    for (int r = 0; r < ROMS; r++)
        if (!strcmp(sum, roms[r].sha1)) {
            snprintf(roms[r].picked, MAX_PATH, "%s", file);
            snprintf(roms[r].shown, MAX_PATH, "%s", path);
            row_problem[row_of_rom(r)][0] = 0;
            row_problem[slot][0] = 0;
            if (!strcmp(roms[r].id, "jp")) make_english();   /* a patch waiting for it */
            return;
        }
    static const char *const wants[ROW_COUNT] = {
        "Streets of Rage (World)", "Streets of Rage 2 (USA)", "Streets of Rage 3 (USA)", "Bare Knuckle III (Japan)",
        "bk3_100.ips or bk3_100.zip",
    };
    snprintf(row_problem[slot], sizeof row_problem[slot], "Not the right file: this takes %s, unchanged.", wants[slot]);
}

/* ---- Pages ---------------------------------------------------------------------------- */
enum { PAGE_WELCOME, PAGE_FOLDER, PAGE_GAMES, PAGE_INSTALL, PAGE_DONE, PAGE_FAILED, PAGE_UNINSTALL, PAGE_REMOVED };
static int page;
static char folder[MAX_PATH], source_dir[MAX_PATH];
static bool desktop_icon = true, launch_after = true, keep_saves = true, reinstall;
static int focus;                       /* the focused control on the page */
static char exe_path[MAX_PATH];

/* The install's progress, from the installer thread. */
static SDL_mutex *lock;
static struct { int percent; char what[200]; char log[6][200]; int nlog; bool done, failed; char error[300]; } prog;
static HANDLE child;

#define C_YELLOW rgb_hex(0xF8D838)
#define C_RED    rgb_hex(0xE0381C)
#define C_INK    rgb_hex(0x15151A)
#define C_TEXT   rgb_hex(0xE9E5DC)
#define C_DIM    rgb_hex(0x8E8A82)
#define HEAD     "Impact"
#define BODY     "Segoe UI"

typedef struct { float x, y, w, h; int id; } Button;
static Button buttons[16];
static int nbuttons;
static float mouse_x, mouse_y;

static bool hovering(const Button *b) { return mouse_x >= b->x && mouse_x < b->x + b->w && mouse_y >= b->y && mouse_y < b->y + b->h; }

static void shadow_text(const char *s, const char *face, float px, float tr, float x, float y, Rgb top, Rgb bottom) {
    ui_draw_text(s, face, px, 400, tr, x + 3, y + 3, rgb(0, 0, 0), rgb(0, 0, 0), .7f);
    ui_draw_text(s, face, px, 400, tr, x, y, top, bottom, 1);
}

static float wrap(const char *s, float x, float y, float w, float px, Rgb c, float a) {
    char line[400] = "", word[200];
    const char *p = s;
    while (*p) {
        int n = 0;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && *p != '\n' && n < 199) word[n++] = *p++;
        word[n] = 0;
        char trial[600];
        snprintf(trial, sizeof trial, "%s%s%s", line, line[0] ? " " : "", word);
        if (ui_text_width(trial, BODY, px, 400, 0) > w && line[0]) {
            ui_draw_text(line, BODY, px, 400, 0, x, y, c, c, a);
            y += px * 1.45f;
            snprintf(line, sizeof line, "%s", word);
        } else snprintf(line, sizeof line, "%s", trial);
        if (*p == '\n') { ui_draw_text(line, BODY, px, 400, 0, x, y, c, c, a); y += px * 1.45f; line[0] = 0; p++; }
    }
    if (line[0]) { ui_draw_text(line, BODY, px, 400, 0, x, y, c, c, a); y += px * 1.45f; }
    return y;
}

/* A button; `kind` 0 plain, 1 primary (red), 2 disabled. */
static void button(float x, float y, float w, float h, const char *label, int kind, int id) {
    Button *b = &buttons[nbuttons < 16 ? nbuttons++ : 15];
    *b = (Button){ x, y, w, h, id };
    bool hot = kind != 2 && (hovering(b) || focus == id);
    Rgb fill = kind == 1 ? C_RED : kind == 2 ? rgb_hex(0x1C1C22) : rgb_hex(0x23232A);
    if (hot) fill = add(fill, rgb(1, 1, 1), .08f);
    ui_panel(x, y, w, h, 3, add(fill, rgb(1, 1, 1), .05f), fill, 1, hot ? C_YELLOW : rgb_hex(0x56565E), 1);
    float tw = ui_text_width(label, HEAD, 22, 400, 3);
    Rgb tc = kind == 2 ? rgb_hex(0x5A5A60) : rgb_hex(0xF2EEE6);
    ui_draw_text(label, HEAD, 22, 400, 3, x + w / 2 - tw / 2, y + h / 2 - 15, tc, tc, 1);
}

static void checkbox(float x, float y, const char *label, bool on, int id) {
    Button *b = &buttons[nbuttons < 16 ? nbuttons++ : 15];
    *b = (Button){ x, y, 30 + ui_text_width(label, BODY, 17, 400, 0) + 10, 26, id };
    bool hot = hovering(b) || focus == id;
    ui_panel(x, y + 2, 22, 22, 3, rgb_hex(0x23232A), rgb_hex(0x1A1A20), 1, hot ? C_YELLOW : rgb_hex(0x6A6A72), 1);
    if (on) ui_fill_rect(x + 6, y + 8, 10, 10, C_YELLOW, 1);
    ui_draw_text(label, BODY, 17, 400, 0, x + 32, y + 2, C_TEXT, C_TEXT, 1);
}

enum { B_NEXT = 1, B_BACK, B_CANCEL, B_BROWSE_DIR, B_ROM0, B_SKIP0 = B_ROM0 + ROW_COUNT, B_DESKTOP = B_SKIP0 + ROW_COUNT, B_LAUNCH, B_KEEP, B_FINISH, B_REMOVE };

static const char *steps[4] = { "WELCOME", "FOLDER", "GAMES", "INSTALL" };

static int games_ready(void) { int n = 0; for (int g = 0; g < 3; g++) n += game_picked(g); return bk3_needs_us() ? 0 : n; }

static void draw_frame(void) {
    for (int y = 0; y < ui_h; y++) {
        uint32_t v = pack(mix(rgb_hex(0x16121E), rgb_hex(0x08080B), (float)y / ui_h));
        for (int x = 0; x < ui_w; x++) ui_px[y * ui_w + x] = v;
    }
    ui_panel(60, 40, 1160, 640, 6, rgb_hex(0x17171D), rgb_hex(0x101014), 1, C_YELLOW, .9f);
    shadow_text("STREETS OF RAGE", HEAD, 44, 4, 100, 64, rgb_hex(0xFFE36A), rgb_hex(0xF0A030));
    float tw = ui_text_width("STREETS OF RAGE", HEAD, 44, 400, 4);
    ui_draw_text("COLLECTION SETUP", HEAD, 18, 400, 4, 118 + tw, 86, C_TEXT, C_DIM, 1);
    ui_fill_rect(102, 122, 160, 3, C_RED, 1);
    if (page <= PAGE_INSTALL) {         /* the steps, along the top right */
        for (int k = 0; k < 4; k++) {
            float x = 812 + k * 94;
            bool on = k == page, past = k < page;
            ui_fill_rect(x, 80, 86, 4, on ? C_YELLOW : past ? rgb_hex(0x8A7A30) : rgb_hex(0x3A3A42), 1);
            ui_draw_text(steps[k], HEAD, 14, 400, 3, x, 90, on ? C_YELLOW : C_DIM, on ? C_YELLOW : C_DIM, 1);
        }
    }
}

static void draw_page(float t) {
    nbuttons = 0;
    draw_frame();
    float x = 102, y = 160, w = 1076;
    switch (page) {
    case PAGE_WELCOME:
        shadow_text(reinstall ? "WELCOME BACK" : "WELCOME", HEAD, 34, 3, x, y, C_YELLOW, rgb_hex(0x8A140A));
        y = wrap(reinstall
                 ? "The collection is installed. Setup can add a game you skipped, or build them again from your ROMs. Your settings, saves and save states stay as they are."
                 : "This installs Streets of Rage 1, 2 and 3 as native Windows programs, with widescreen, higher resolutions and frame rates, an AI partner, cheats, fighter colours and more, in one collection.",
                 x, y + 62, w, 19, C_TEXT, 1);
        y = wrap("No part of any game comes with it. You pick your own ROMs of the cartridges (Streets of Rage (World), Streets of Rage 2 (USA), Streets of Rage 3 (USA), and if you have it Bare Knuckle III (Japan)): each one is translated into a PC program on this computer. You can skip a game you do not have and add it later by running Setup again.",
                 x, y + 14, w, 19, C_TEXT, 1);
        wrap("Setup downloads the build tools it needs from their publishers (Node.js, CMake, Ninja and LLVM-MinGW, about 260 MB, checked against pinned SHA-256s) into the install folder, unless they are already on this PC. Nothing is installed into Windows itself, and no administrator rights are needed.",
             x, y + 14, w, 17, C_DIM, 1);
        button(1010, 596, 170, 52, "NEXT", 1, B_NEXT);
        button(820, 596, 170, 52, "CANCEL", 0, B_CANCEL);
        break;
    case PAGE_FOLDER:
        shadow_text("INSTALL FOLDER", HEAD, 34, 3, x, y, C_YELLOW, rgb_hex(0x8A140A));
        y = wrap("The collection, its build tools and each game's settings, saves and save states go here. Around 1.5 GB with all three games.", x, y + 62, w, 19, C_TEXT, 1);
        ui_panel(x, y + 20, 860, 52, 4, rgb_hex(0x0E0E12), rgb_hex(0x0E0E12), 1, rgb_hex(0x4A4A52), 1);
        ui_draw_text(folder, BODY, 18, 400, 0, x + 16, y + 34, C_TEXT, C_TEXT, 1);
        button(x + 880, y + 20, 196, 52, "BROWSE", 0, B_BROWSE_DIR);
        button(1010, 596, 170, 52, "NEXT", 1, B_NEXT);
        button(820, 596, 170, 52, "BACK", 0, B_BACK);
        break;
    case PAGE_GAMES: {
        shadow_text("YOUR GAMES", HEAD, 34, 3, x, y, C_YELLOW, rgb_hex(0x8A140A));
        wrap("Pick a ROM for each game you have (.md, .bin, .gen or .zip): each is checked as you pick it. Skip any you do not have.", x, y + 50, w, 17, C_TEXT, 1);
        static const float row_y[ROW_COUNT] = { 232, 306, 380, 458, 522 };
        for (int row = 0; row < ROW_COUNT; row++) {
            Game *gm = &games[row_game[row]];
            bool sub = row >= ROW_BK3;    /* Streets of Rage 3's optional editions, indented under it */
            float rx = sub ? x + 48 : x, rw = sub ? w - 48 : w, cy = row_y[row], rh = sub ? 58 : 68;
            int rom = row == ROW_SOR3 ? rom_index("us") : row == ROW_BK3 ? rom_index("jp") : row == ROW_EN ? rom_index("en") : row;
            bool picked = roms[rom].picked[0] || (row == ROW_EN && ips_file[0]);
            Rgb accent = rgb_hex(gm->accent);
            if (sub) {                    /* a bracket from Streets of Rage 3's row */
                ui_fill_rect(x + 22, cy - 8, 3, rh / 2 + 8, mix(accent, rgb_hex(0x000000), .35f), 1);
                ui_fill_rect(x + 22, cy + rh / 2, 26, 3, mix(accent, rgb_hex(0x000000), .35f), 1);
            }
            ui_panel(rx, cy, rw, rh, 5, rgb_hex(0x1C1C23), rgb_hex(0x141418), 1, picked ? accent : rgb_hex(0x3A3A42), picked ? .8f : 1);
            ui_fill_rect(rx, cy, 6, rh, sub ? mix(accent, rgb_hex(0x000000), .35f) : accent, 1);
            shadow_text(row_title[row], HEAD, sub ? 20 : 26, 2, rx + 22, cy + (sub ? 7 : 8), rgb_hex(0xFFFFFF), mix(accent, rgb_hex(0xFFFFFF), .3f));
            if (sub) {
                float tw = ui_text_width(row_title[row], HEAD, 20, 400, 2);
                ui_draw_text("OPTIONAL", HEAD, 13, 400, 3, rx + 36 + tw, cy + 13, C_DIM, C_DIM, 1);
            }
            const char *state;
            Rgb sc;
            char buf[2 * MAX_PATH + 40];
            if (row_problem[row][0]) { state = row_problem[row]; sc = rgb_hex(0xF09A86); }
            else if (row == ROW_SOR3 && bk3_needs_us()) { state = "Pick Streets of Rage 3 (USA) here too: Bare Knuckle III is built on top of it."; sc = rgb_hex(0xF09A86); }
            else if (picked) {
                const char *shown = roms[rom].picked[0] ? roms[rom].shown : ips_shown;
                const char *base = strrchr(shown, '\\');
                if (!base) base = strrchr(shown, '/');
                snprintf(buf, sizeof buf, "\xE2\x9C\x93 %s", base ? base + 1 : shown);
                state = buf; sc = rgb_hex(0x9FDCA6);
            }
            else if (row_installed[row]) { state = "Installed. Pick it again to rebuild, or leave it as it is."; sc = C_DIM; }
            else { state = row_about[row]; sc = C_DIM; }
            ui_draw_text(state, BODY, sub ? 14.5f : 15.5f, 400, 0, rx + 22, cy + (sub ? 34 : 40), sc, sc, 1);
            float bh = sub ? 38 : 44, by = cy + (rh - bh) / 2;
            button(x + w - 320, by, 150, bh, "BROWSE", 0, B_ROM0 + row);
            button(x + w - 160, by, 140, bh, "SKIP", picked ? 0 : 2, B_SKIP0 + row);
        }
        int ready = games_ready();
        button(1010, 596, 170, 52, "INSTALL", (ready || reinstall) && !bk3_needs_us() ? 1 : 2, B_NEXT);
        button(820, 596, 170, 52, "BACK", 0, B_BACK);
        break;
    }
    case PAGE_INSTALL: case PAGE_DONE: case PAGE_FAILED: {
        SDL_LockMutex(lock);
        bool done = page == PAGE_DONE, failed = page == PAGE_FAILED;
        shadow_text(done ? "READY TO PLAY" : failed ? "THE INSTALL STOPPED" : "INSTALLING", HEAD, 34, 3, x, y,
                    failed ? rgb_hex(0xF09A86) : C_YELLOW, rgb_hex(0x8A140A));
        if (!done) {
            float bar = prog.percent / 100.f;
            ui_panel(x, y + 70, w, 30, 4, rgb_hex(0x0E0E12), rgb_hex(0x0E0E12), 1, rgb_hex(0x4A4A52), 1);
            ui_fill_rect(x + 3, y + 73, (w - 6) * bar, 24, failed ? rgb_hex(0x7A2A20) : C_RED, 1);
            if (!failed) {                 /* a sheen running along the bar while it works */
                float sx = x + 3 + fmodf(t * 260, (w - 6) * bar + 1);
                ui_fill_rect(sx, y + 73, 40, 24, rgb(1, .8f, .6f), .18f);
            }
            char pct[16];
            snprintf(pct, sizeof pct, "%d%%", prog.percent);
            ui_draw_text(pct, HEAD, 20, 400, 2, x + w - 60, y + 112, C_YELLOW, C_YELLOW, 1);
            ui_draw_text(failed ? prog.error : prog.what, BODY, 19, 400, 0, x, y + 112, failed ? rgb_hex(0xF09A86) : C_TEXT,
                         failed ? rgb_hex(0xF09A86) : C_TEXT, 1);
            for (int k = 0; k < prog.nlog; k++)
                ui_draw_text(prog.log[k], "Consolas", 14, 400, 0, x, y + 170 + k * 22, rgb_hex(0x6E6A62), rgb_hex(0x6E6A62), 1);
            if (failed) {
                button(1010, 596, 170, 52, "CLOSE", 1, B_CANCEL);
                button(820, 596, 170, 52, "BACK", 0, B_BACK);
            } else button(1010, 596, 170, 52, "CANCEL", 0, B_CANCEL);
        } else {
            y = wrap("The collection is installed. Open it from the Start menu (and the desktop, if you like). Each game has its own launcher with its settings; your saves live with each game.", x, y + 62, w, 19, C_TEXT, 1);
            checkbox(x, y + 24, "Put a shortcut on the desktop", desktop_icon, B_DESKTOP);
            checkbox(x, y + 64, "Start the collection now", launch_after, B_LAUNCH);
            button(1010, 596, 170, 52, "FINISH", 1, B_FINISH);
        }
        SDL_UnlockMutex(lock);
        break;
    }
    case PAGE_UNINSTALL:
        shadow_text("REMOVE THE COLLECTION", HEAD, 34, 3, x, y, C_YELLOW, rgb_hex(0x8A140A));
        y = wrap("This removes Streets of Rage Collection from this PC: the games, the build tools and the shortcuts. Your ROM files elsewhere are not touched.", x, y + 62, w, 19, C_TEXT, 1);
        ui_draw_text(folder, BODY, 17, 400, 0, x, y + 10, C_DIM, C_DIM, 1);
        checkbox(x, y + 54, "Keep my settings, saves and save states (in the folder's games\\ folder)", keep_saves, B_KEEP);
        button(1010, 596, 170, 52, "REMOVE", 1, B_REMOVE);
        button(820, 596, 170, 52, "CANCEL", 0, B_CANCEL);
        break;
    case PAGE_REMOVED:
        shadow_text("REMOVED", HEAD, 34, 3, x, y, C_YELLOW, rgb_hex(0x8A140A));
        wrap(keep_saves ? "Streets of Rage Collection is removed. Your settings and saves are still in the folder's games\\ folder; run Setup again to pick up where you left off."
                        : "Streets of Rage Collection is removed.", x, y + 62, w, 19, C_TEXT, 1);
        button(1010, 596, 170, 52, "CLOSE", 1, B_CANCEL);
        break;
    }
}

/* ---- Windows' pickers ------------------------------------------------------------------ */
static void browse_folder(SDL_Window *win) {
    BROWSEINFOA bi = { 0 };
    bi.lpszTitle = "Where to install Streets of Rage Collection";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST id = SHBrowseForFolderA(&bi);
    if (!id) return;
    char p[MAX_PATH];
    if (SHGetPathFromIDListA(id, p)) {
        size_t n = strlen(p);
        /* A folder of its own inside the one picked, unless it already is one. */
        if (n < 20 || _stricmp(p + n - strlen(APP_NAME), APP_NAME))
            snprintf(folder, sizeof folder, "%s%s%s", p, p[n - 1] == '\\' ? "" : "\\", APP_NAME);
        else snprintf(folder, sizeof folder, "%s", p);
    }
    CoTaskMemFree(id);
    (void)win;
}

static void browse_rom(int g) {
    char file[MAX_PATH] = "";
    OPENFILENAMEA of = { .lStructSize = sizeof of };
    of.lpstrFilter = "Genesis ROMs and patches (*.md;*.bin;*.gen;*.zip;*.ips)\0*.md;*.bin;*.gen;*.zip;*.ips\0All files (*.*)\0*.*\0";
    of.lpstrFile = file;
    of.nMaxFile = MAX_PATH;
    char title[96];
    static const char *const what[ROW_COUNT] = { "Your Streets of Rage (World) ROM", "Your Streets of Rage 2 (USA) ROM",
        "Your Streets of Rage 3 (USA) ROM", "Your Bare Knuckle III (Japan) ROM", "Twilight Translations' patch: bk3_100.ips or bk3_100.zip" };
    snprintf(title, sizeof title, "%s", what[g]);
    of.lpstrTitle = title;
    if (g == ROW_EN) of.lpstrFilter = "The English patch (*.ips;*.zip)\0*.ips;*.zip\0All files (*.*)\0*.*\0";
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&of)) take_rom(g, file);
}

/* ---- The install ----------------------------------------------------------------------- */
static bool print_progress;

static void progress_line(const char *s) {
    if (print_progress) { printf("%s\n", s); fflush(stdout); }
    SDL_LockMutex(lock);
    if (!strncmp(s, "@@error ", 8)) { prog.failed = true; snprintf(prog.error, sizeof prog.error, "%s", s + 8); }
    else if (!strncmp(s, "@@ ", 3)) {
        char *end;
        long p = strtol(s + 3, &end, 10);
        prog.percent = p < 0 ? 0 : p > 100 ? 100 : (int)p;
        snprintf(prog.what, sizeof prog.what, "%s", *end == ' ' ? end + 1 : end);
    } else if (s[0]) {
        if (prog.nlog == 6) { memmove(prog.log, prog.log + 1, sizeof prog.log[0] * 5); prog.nlog = 5; }
        snprintf(prog.log[prog.nlog++], sizeof prog.log[0], "%.150s", s);
    }
    SDL_UnlockMutex(lock);
}

/* roms\<game>.md for a game's own cartridge, roms\<game>-<id>.md for another edition. */
static bool copy_rom(int r, char *out) {
    char dir[MAX_PATH];
    snprintf(dir, sizeof dir, "%s\\roms", folder);
    CreateDirectoryA(dir, NULL);
    if (roms[r].main) snprintf(out, MAX_PATH, "%s\\%s.md", dir, games[roms[r].game].name);
    else snprintf(out, MAX_PATH, "%s\\%s-%s.md", dir, games[roms[r].game].name, roms[r].id);
    if (!_stricmp(out, roms[r].picked)) return true;
    return CopyFileA(roms[r].picked, out, FALSE) != 0;
}

static int SDLCALL install_thread(void *arg) {
    (void)arg;
    char src[MAX_PATH], cmd[8 * MAX_PATH + 512];
    SHCreateDirectoryExA(NULL, folder, NULL);
    /* The source: the payload unpacked into <folder>\source, or a folder given for testing. */
    if (source_dir[0]) snprintf(src, sizeof src, "%s", source_dir);
    else {
        progress_line("@@ 1 Unpacking the collection");
        snprintf(src, sizeof src, "%s\\source", folder);
        FILE *self = fopen(exe_path, "rb");
        char tag[9] = "";
        uint64_t size = 0;
        bool ok = self && !_fseeki64(self, -16, SEEK_END) && fread(&size, 8, 1, self) == 1 && fread(tag, 1, 8, self) == 8 &&
                  !memcmp(tag, "SORPAYLD", 8) && size > 0;
        char zip[MAX_PATH];
        snprintf(zip, sizeof zip, "%s\\source.zip", folder);
        FILE *out = ok ? fopen(zip, "wb") : NULL;
        if (out && !_fseeki64(self, -16 - (long long)size, SEEK_END)) {
            static char buf[1 << 16];
            uint64_t left = size;
            while (left && ok) {
                size_t n = fread(buf, 1, left < sizeof buf ? (size_t)left : sizeof buf, self);
                ok = n > 0 && fwrite(buf, 1, n, out) == n;
                left -= n;
            }
        } else ok = false;
        if (out) fclose(out);
        if (self) fclose(self);
        if (ok) {
            SHCreateDirectoryExA(NULL, src, NULL);
            /* -m: the files get the time of unpacking, not the zip's. The zip's times are the
             * release server's clock with no time zone, so right after a release they can lie in
             * the future here, and ninja then finds its build file older than its sources every
             * time ("manifest 'build.ninja' still dirty after 100 tries"). */
            snprintf(cmd, sizeof cmd, "\"%s\" -m -xf \"%s\" -C \"%s\" --strip-components=1", tar_exe(), zip, src);
            ok = run_hidden(cmd, NULL) == 0;
        }
        DeleteFileA(zip);
        if (!ok) { progress_line("@@error Could not unpack the collection (Setup.exe may be damaged: download it again)."); return 1; }
    }
    /* The ROMs, copied in, for the installer. */
    char copies[ROMS][MAX_PATH];
    for (int r = 0; r < ROMS; r++) {
        copies[r][0] = 0;
        if (roms[r].picked[0] && !copy_rom(r, copies[r])) { progress_line("@@error Could not copy a ROM into the install folder."); return 1; }
    }
    /* -SorN "<rom>", or with several editions "<id>=<rom>|<id>=<rom>" (the game's own first). */
    int n = snprintf(cmd, sizeof cmd, "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%s\\tools\\install.ps1\" -Install \"%s\"", src, folder);
    for (int g = 0; g < 3; g++) {
        if (!game_picked(g)) continue;
        n += snprintf(cmd + n, sizeof cmd - n, " -Sor%d \"", g + 1);
        bool first = true;
        for (int r = 0; r < ROMS; r++) {
            if (roms[r].game != g || !copies[r][0]) continue;
            n += snprintf(cmd + n, sizeof cmd - n, "%s%s=%s", first ? "" : "|", roms[r].id, copies[r]);
            first = false;
        }
        n += snprintf(cmd + n, sizeof cmd - n, "\"");
    }
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE rd, wr;
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si = { .cb = sizeof si, .dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW, .wShowWindow = SW_HIDE };
    si.hStdOutput = si.hStdError = wr;
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, src, &si, &pi)) {
        CloseHandle(rd); CloseHandle(wr);
        progress_line("@@error Could not start PowerShell.");
        return 1;
    }
    CloseHandle(wr);
    child = pi.hProcess;
    char buf[4096], line[1024];
    int ln = 0;
    DWORD got;
    while (ReadFile(rd, buf, sizeof buf, &got, NULL) && got)
        for (DWORD k = 0; k < got; k++) {
            if (buf[k] == '\n' || ln == (int)sizeof line - 1) { line[ln] = 0; if (ln && line[ln - 1] == '\r') line[ln - 1] = 0; progress_line(line); ln = 0; }
            else line[ln++] = buf[k];
        }
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    child = NULL;
    SDL_LockMutex(lock);
    if (code && !prog.failed) { prog.failed = true; snprintf(prog.error, sizeof prog.error, "The install stopped (code %lu).", (unsigned long)code); }
    prog.done = true;
    SDL_UnlockMutex(lock);
    return 0;
}

/* ---- Shortcuts and the Installed apps entry --------------------------------------------- */
static void shortcut(const char *lnk, const char *target, const char *dir) {
    IShellLinkA *sl;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkA, (void **)&sl))) return;
    IShellLinkA_SetPath(sl, target);
    IShellLinkA_SetWorkingDirectory(sl, dir);
    IShellLinkA_SetDescription(sl, APP_NAME);
    IPersistFile *pf;
    if (SUCCEEDED(IShellLinkA_QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) {
        WCHAR w[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, lnk, -1, w, MAX_PATH);
        IPersistFile_Save(pf, w, TRUE);
        IPersistFile_Release(pf);
    }
    IShellLinkA_Release(sl);
}

static void link_path(int csidl, char *out) {
    char base[MAX_PATH] = "";
    SHGetFolderPathA(NULL, csidl, NULL, 0, base);
    snprintf(out, MAX_PATH, "%s\\%s.lnk", base, APP_NAME);
}

static void finish_install(void) {
    char app[MAX_PATH], lnk[MAX_PATH], setup[MAX_PATH], cmd[MAX_PATH + 32];
    snprintf(app, sizeof app, "%s\\%s.exe", folder, APP_NAME);
    link_path(CSIDL_PROGRAMS, lnk);
    shortcut(lnk, app, folder);
    link_path(CSIDL_DESKTOPDIRECTORY, lnk);
    if (desktop_icon) shortcut(lnk, app, folder);
    else DeleteFileA(lnk);
    snprintf(setup, sizeof setup, "%s\\Setup.exe", folder);
    if (_stricmp(setup, exe_path)) CopyFileA(exe_path, setup, FALSE);
    HKEY k;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, UNINST_KEY, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) == ERROR_SUCCESS) {
        const char *strs[][2] = { { "DisplayName", APP_NAME }, { "DisplayVersion", VERSION }, { "Publisher", "OriginalSwayze" },
                                  { "InstallLocation", folder }, { "DisplayIcon", app } };
        for (int i = 0; i < 5; i++) RegSetValueExA(k, strs[i][0], 0, REG_SZ, (const BYTE *)strs[i][1], (DWORD)strlen(strs[i][1]) + 1);
        snprintf(cmd, sizeof cmd, "\"%s\" --uninstall", setup);
        RegSetValueExA(k, "UninstallString", 0, REG_SZ, (const BYTE *)cmd, (DWORD)strlen(cmd) + 1);
        snprintf(cmd, sizeof cmd, "\"%s\"", setup);
        RegSetValueExA(k, "ModifyPath", 0, REG_SZ, (const BYTE *)cmd, (DWORD)strlen(cmd) + 1);
        RegCloseKey(k);
    }
    if (launch_after) ShellExecuteA(NULL, "open", app, NULL, folder, SW_SHOWNORMAL);
}

/* Deletes a folder and everything in it (SHFileOperation wants a double-null-ended path). */
static void remove_tree(const char *path) {
    char p[MAX_PATH + 2] = "";
    snprintf(p, MAX_PATH, "%s", path);
    p[strlen(p) + 1] = 0;
    SHFILEOPSTRUCTA op = { .wFunc = FO_DELETE, .pFrom = p, .fFlags = FOF_NO_UI };
    SHFileOperationA(&op);
}

static void uninstall(void) {
    char lnk[MAX_PATH], p[MAX_PATH];
    link_path(CSIDL_PROGRAMS, lnk); DeleteFileA(lnk);
    link_path(CSIDL_DESKTOPDIRECTORY, lnk); DeleteFileA(lnk);
    RegDeleteKeyA(HKEY_CURRENT_USER, UNINST_KEY);
    const char *parts[] = { "source", "roms" };
    for (int i = 0; i < 2; i++) { snprintf(p, sizeof p, "%s\\%s", folder, parts[i]); remove_tree(p); }
    snprintf(p, sizeof p, "%s\\%s.exe", folder, APP_NAME); DeleteFileA(p);
    for (int g = 0; g < 3; g++) {       /* the programs; with keep_saves, everything else in games\ stays */
        snprintf(p, sizeof p, "%s\\games\\%s\\%s", folder, games[g].name, games[g].exe);
        DeleteFileA(p);
    }
    if (!keep_saves) { snprintf(p, sizeof p, "%s\\games", folder); remove_tree(p); }
    /* Setup.exe itself, once this program has closed: a hidden cmd waits, then deletes it
     * (and the folder, if nothing is left in it). */
    char cmd[3 * MAX_PATH];
    snprintf(cmd, sizeof cmd, "cmd.exe /c ping -n 3 127.0.0.1 >nul & del /f /q \"%s\\Setup.exe\" & rmdir \"%s\"", folder, folder);
    STARTUPINFOA si = { .cb = sizeof si, .dwFlags = STARTF_USESHOWWINDOW, .wShowWindow = SW_HIDE };
    PROCESS_INFORMATION pi;
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
}

/* ---- Actions ------------------------------------------------------------------------------ */
static bool act(int id, SDL_Window *win) {
    if (id == B_CANCEL) {
        if (page == PAGE_INSTALL && child) TerminateProcess(child, 1);
        return false;
    }
    if (id == B_NEXT) {
        if (page == PAGE_GAMES) {
            if (!games_ready()) return true;
            prog = (typeof(prog)){ 0 };
            page = PAGE_INSTALL;
            SDL_CreateThread(install_thread, "install", NULL);
        } else if (page < PAGE_GAMES) page++;
    } else if (id == B_BACK) {
        if (page == PAGE_FAILED) page = PAGE_GAMES;
        else if (page > PAGE_WELCOME) page--;
    } else if (id == B_BROWSE_DIR) browse_folder(win);
    else if (id >= B_ROM0 && id < B_ROM0 + ROW_COUNT) browse_rom(id - B_ROM0);
    else if (id >= B_SKIP0 && id < B_SKIP0 + ROW_COUNT) row_skip(id - B_SKIP0);
    else if (id == B_DESKTOP) desktop_icon = !desktop_icon;
    else if (id == B_LAUNCH) launch_after = !launch_after;
    else if (id == B_KEEP) keep_saves = !keep_saves;
    else if (id == B_FINISH) { finish_install(); return false; }
    else if (id == B_REMOVE) { uninstall(); page = PAGE_REMOVED; }
    return true;
}

static void default_folder(void) {
    HKEY k;
    DWORD n = sizeof folder;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, UNINST_KEY, 0, KEY_READ, &k) == ERROR_SUCCESS) {
        bool ok = RegQueryValueExA(k, "InstallLocation", NULL, NULL, (BYTE *)folder, &n) == ERROR_SUCCESS && folder[0];
        RegCloseKey(k);
        if (ok) { reinstall = true; return; }
    }
    char base[MAX_PATH] = "";
    SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, base);
    snprintf(folder, sizeof folder, "%s\\Programs\\%s", base, APP_NAME);
}

static void find_installed(void) {
    for (int g = 0; g < 3; g++) {
        char p[MAX_PATH];
        snprintf(p, sizeof p, "%s\\games\\%s\\%s", folder, games[g].name, games[g].exe);
        games[g].installed = GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
        row_installed[g] = games[g].installed;
    }
    /* The optional editions: their ROMs, kept in the folder's roms\ (copy_rom). */
    static const char *const kept[2] = { "roms\\sor3-jp.md", "roms\\sor3-en.md" };
    for (int k = 0; k < 2; k++) {
        char p[MAX_PATH];
        snprintf(p, sizeof p, "%s\\%s", folder, kept[k]);
        row_installed[ROW_BK3 + k] = games[2].installed && GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
    }
}

int main(int argc, char **argv) {
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    default_folder();
    const char *shot = NULL;
    bool install_test = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--uninstall")) page = PAGE_UNINSTALL;
        else if (!strcmp(argv[i], "--source") && i + 1 < argc) snprintf(source_dir, sizeof source_dir, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--folder") && i + 1 < argc) snprintf(folder, sizeof folder, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--rom") && i + 2 < argc) { int g = atoi(argv[++i]) - 1; if (g >= 0 && g < ROW_COUNT) take_rom(g, argv[++i]); else i++; }
        else if (!strcmp(argv[i], "--shot") && i + 2 < argc) { page = atoi(argv[++i]); shot = argv[++i]; }
        else if (!strcmp(argv[i], "--install-test")) install_test = true;
    }
    find_installed();
    lock = SDL_CreateMutex();
    if (install_test) {
        print_progress = true;
        install_thread(NULL);
        return prog.failed ? 1 : 0;
    }
    if (shot) {
        ui_begin(1280, 720);
        if (page == PAGE_INSTALL) { prog.percent = 42; snprintf(prog.what, sizeof prog.what, "Streets of Rage 2: recompiling and building (a few minutes)"); prog.nlog = 2;
                                    snprintf(prog.log[0], 200, "[214/347] Building C object CMakeFiles/recomp.dir/generated/sor2/recomp_07.c.obj");
                                    snprintf(prog.log[1], 200, "[215/347] Building C object CMakeFiles/recomp.dir/generated/sor2/recomp_08.c.obj"); }
        draw_page(1);
        return png_write(shot, ui_px, ui_w, ui_h, ui_w) ? 1 : 0;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) return 1;
    SDL_Window *win = SDL_CreateWindow(page == PAGE_UNINSTALL ? APP_NAME " - Remove" : APP_NAME " Setup", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, 1120, 630, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *tex = NULL;
    int tw = 0, th = 0;
    bool running = true;
    Uint64 t0 = SDL_GetPerformanceCounter();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                if (page == PAGE_INSTALL && child) TerminateProcess(child, 1);
                running = false;
            } else if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN) {
                int ww, wh, mx = e.type == SDL_MOUSEMOTION ? e.motion.x : e.button.x, my = e.type == SDL_MOUSEMOTION ? e.motion.y : e.button.y;
                SDL_GetWindowSize(win, &ww, &wh);
                mouse_x = (mx * (float)tw / ww - ui_ox) / ui_s;
                mouse_y = (my * (float)th / wh - ui_oy) / ui_s;
                if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
                    for (int k = 0; k < nbuttons; k++)
                        if (hovering(&buttons[k])) { if (!act(buttons[k].id, win)) running = false; break; }
            } else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key = e.key.keysym.sym;
                if (key == SDLK_TAB || key == SDLK_RIGHT || key == SDLK_DOWN || key == SDLK_LEFT || key == SDLK_UP) {
                    int at = 0, dir = key == SDLK_LEFT || key == SDLK_UP || (key == SDLK_TAB && (SDL_GetModState() & KMOD_SHIFT)) ? -1 : 1;
                    for (int k = 0; k < nbuttons; k++) if (buttons[k].id == focus) at = k;
                    if (nbuttons) focus = buttons[(at + dir + nbuttons) % nbuttons].id;
                } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    int id = focus;
                    bool there = false;
                    for (int k = 0; k < nbuttons; k++) there |= buttons[k].id == id;
                    if (!there) id = page == PAGE_DONE ? B_FINISH : page == PAGE_UNINSTALL ? B_REMOVE : B_NEXT;
                    if (!act(id, win)) running = false;
                } else if (key == SDLK_ESCAPE && page != PAGE_INSTALL) running = false;
            } else if (e.type == SDL_DROPFILE && page == PAGE_GAMES) {    /* a ROM dropped on the window */
                take_rom(0, e.drop.file);
                SDL_free(e.drop.file);
            }
        }
        SDL_LockMutex(lock);
        if (page == PAGE_INSTALL && prog.done) page = prog.failed ? PAGE_FAILED : PAGE_DONE;
        SDL_UnlockMutex(lock);
        int w, h;
        SDL_GetRendererOutputSize(ren, &w, &h);
        ui_begin(w, h);
        draw_page((float)(SDL_GetPerformanceCounter() - t0) / SDL_GetPerformanceFrequency());
        if (!tex || tw != ui_w || th != ui_h) {
            if (tex) SDL_DestroyTexture(tex);
            tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, ui_w, ui_h);
            tw = ui_w; th = ui_h;
        }
        SDL_UpdateTexture(tex, NULL, ui_px, ui_w * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
    SDL_Quit();
    CoUninitialize();
    return 0;
}
