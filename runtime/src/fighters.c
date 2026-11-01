/* The fighters' colours: see fighters.h.
 *
 * Streets of Rage 3's four (docs/ram-sor3.md) use the same line 0 as 2's; player 1's frame
 * is in tiles $4CE-$518, player 2's in $519-$563.
 *
 * Streets of Rage 2's four (docs/ram-sor2.md) share line 0 the same way: 1 white, 2 the
 * outline, 3-5 skin, 6-7 reds, 8-10 blues, 11-12 greys, 13-14 yellow and orange, each using
 * some of them; player 1's frame is in tiles $180-$1CF, player 2's in $1D0-$21F.
 *
 * Streets of Rage's three fighters share palette line 0 (docs/ram.md): entries 1-5 are everyone's
 * (white, the dark outline, the skin ramp), 6-7 Blaze's reds, 9-11 Axel's blues and
 * 12-15 greys and yellows that Axel (top, hair) and Adam (trousers, vest) both use. Each
 * player's current frame is in its own block of tiles ($580 for player 1, $5C0 for player
 * 2), so the renderer recolours a player's pixels with that player's table (vdp.c), and
 * one fighter's yellow hair can change while the other's yellow vest stays; even the
 * shared white and skin entries can differ between the two.
 *
 * A part is a ramp of entries, brightest first; one colour is picked for it, and each
 * entry gets the pick shaded to the brightness the game gives that entry
 * (vdp_outfit_color), so folds stay and the game's fades still darken the fighters. A
 * part can also pulse, run through the rainbow or flicker like flame.
 *
 * The page edits a copy of the settings: the preview shows it at once, and nothing in the
 * game changes until Apply. */
#include "fighters.h"

#include <stdlib.h>
#include <string.h>

#include "genesis.h"
#include "outfit.h"
#include "settings.h"
#include "ui.h"

typedef struct { const char *name, *about; int n; uint8_t idx[3]; uint16_t top; } Part;

#if GAME_SOR == 3
enum { AXEL, BLAZE, SKATE, ZAN };
const char *const fighter_names[FIGHTERS] = { "Axel", "Blaze", "Skate", "Zan" };
static const char *const ini_names[FIGHTERS] = { "axel", "blaze", "skate", "zan" };
static const Part parts[FIGHTERS][FIGHTER_PARTS] = {
    [AXEL] = {
        { "Top & hair", "His top and his hair: the game draws them with the same colours.", 2, { 13, 14 }, 0x00EE },
        { "Trousers & shoes", "The grey of his trousers and his shoes.", 2, { 11, 12 }, 0x0AAA },
        { "Gloves", "His gloves and the red on his shoes.", 2, { 6, 7 }, 0x002E },
        { "Skin", "His skin, from pale to deep.", 3, { 3, 4, 5 }, 0x06AE },
    },
    [BLAZE] = {
        { "Outfit & boots", "Her top, skirt and boots.", 2, { 11, 12 }, 0x0AAA },
        { "Hair & skin", "Her hair and her skin: the game draws them with the same colours.", 3, { 3, 4, 5 }, 0x06AE },
    },
    [SKATE] = {
        { "Top", "His vest.", 2, { 6, 7 }, 0x002E },
        { "Cap, shorts & skates", "His cap, his shorts and his skates.", 3, { 8, 9, 10 }, 0x0AAE },
        { "Wheels", "The wheels of his skates.", 2, { 13, 14 }, 0x00EE },
        { "Skin", "His skin, from pale to deep.", 3, { 3, 4, 5 }, 0x06AE },
    },
    [ZAN] = {
        { "Jacket", "His jacket.", 3, { 8, 9, 10 }, 0x0AAE },
        { "Trousers", "His trousers.", 2, { 11, 12 }, 0x0AAA },
        { "Shoes", "His shoes.", 2, { 6, 7 }, 0x002E },
        { "Head & hands", "His metal head and hands.", 3, { 3, 4, 5 }, 0x06AE },
    },
};
static const int part_count[FIGHTERS] = { 4, 2, 4, 4 };
#elif GAME_SOR == 2
enum { MAX, AXEL, BLAZE, SKATE };
const char *const fighter_names[FIGHTERS] = { "Max", "Axel", "Blaze", "Skate" };
static const char *const ini_names[FIGHTERS] = { "max", "axel", "blaze", "skate" };
static const Part parts[FIGHTERS][FIGHTER_PARTS] = {
    [MAX] = {
        { "Trousers", "His trousers.", 3, { 8, 9, 10 }, 0x0AAE },
        { "Boots", "His boots.", 2, { 6, 7 }, 0x002E },
        { "Belt & wristbands", "His belt and the bands on his wrists.", 2, { 11, 12 }, 0x0AAA },
        { "Skin", "His skin, from pale to deep.", 3, { 3, 4, 5 }, 0x06AE },
    },
    [AXEL] = {
        { "Hair", "His hair.", 2, { 13, 14 }, 0x00EE },
        { "Shirt", "His shirt's shading (the white stays).", 2, { 11, 12 }, 0x0AAA },
        { "Jeans & headband", "His jeans and his headband: the game draws them with the same blues.", 3, { 8, 9, 10 }, 0x0AAE },
        { "Gloves & shoes", "His fingerless gloves and the red on his shoes.", 2, { 6, 7 }, 0x002E },
        { "Skin", "His skin, from pale to deep.", 3, { 3, 4, 5 }, 0x06AE },
    },
    [BLAZE] = {
        { "Outfit", "Her top, skirt and shoes.", 2, { 6, 7 }, 0x002E },
        { "Trim & bands", "The yellow trim and the bands on her wrists.", 2, { 13, 14 }, 0x00EE },
        { "Hair & skin", "Her hair and her skin: the game draws them with the same colours.", 3, { 3, 4, 5 }, 0x06AE },
    },
    [SKATE] = {
        { "Top", "His top.", 2, { 13, 14 }, 0x00EE },
        { "Cap & skates", "His cap, his gloves and his skates.", 2, { 6, 7 }, 0x002E },
        { "Shorts", "His shorts' shading (the white stays).", 2, { 11, 12 }, 0x0AAA },
        { "Skin", "His skin, from pale to deep.", 3, { 3, 4, 5 }, 0x06AE },
    },
};
static const int part_count[FIGHTERS] = { 4, 5, 3, 4 };
#else
enum { AXEL, ADAM, BLAZE };
const char *const fighter_names[FIGHTERS] = { "Axel", "Adam", "Blaze" };
static const char *const ini_names[FIGHTERS] = { "axel", "adam", "blaze" };
static const Part parts[FIGHTERS][FIGHTER_PARTS] = {
    [AXEL] = {
        { "Hair", "His hair.", 2, { 14, 15 }, 0x02EE },
        { "Top & shoes", "His tank top and his shoes.", 3, { 1, 12, 13 }, 0x0EEE },
        { "Jeans & wristbands", "His jeans and the bands on his wrists.", 3, { 9, 10, 11 }, 0x0EA8 },
        { "Skin", "His skin, from pale to deep.", 3, { 3, 4, 5 }, 0x06AE },
    },
    [ADAM] = {
        { "Vest", "His vest.", 2, { 14, 15 }, 0x02EE },
        { "Trousers", "His trousers.", 2, { 12, 13 }, 0x0888 },
        { "Skin", "His skin, from pale to deep.", 3, { 3, 4, 5 }, 0x06AE },
        { "Shoes & details", "His shoes and the white in his eyes and highlights.", 1, { 1 }, 0x0EEE },
    },
    [BLAZE] = {
        { "Outfit", "Her top, skirt and boots.", 2, { 6, 7 }, 0x002C },
        { "Hair & skin", "Her hair and her skin: the game draws them with the same colours.", 3, { 3, 4, 5 }, 0x06AE },
        { "Details", "The white in her eyes, earrings and highlights.", 1, { 1 }, 0x0EEE },
    },
};
static const int part_count[FIGHTERS] = { 4, 4, 3 };
#endif

typedef struct { const char *name; uint8_t r, g, b; } Colour;   /* 3-bit channels, as the Genesis has */
static const Colour colours[] = {
    { "White", 7, 7, 7 }, { "Ivory", 7, 7, 5 }, { "Cream", 7, 7, 6 }, { "Sand", 7, 6, 4 }, { "Peach", 7, 5, 4 },
    { "Gold", 7, 5, 1 }, { "Yellow", 7, 7, 0 }, { "Lemon", 7, 7, 3 }, { "Amber", 7, 4, 1 }, { "Orange", 7, 4, 0 },
    { "Tangerine", 7, 3, 0 }, { "Rust", 5, 2, 0 }, { "Copper", 6, 3, 1 }, { "Crimson", 6, 0, 0 }, { "Red", 7, 0, 0 },
    { "Scarlet", 7, 1, 1 }, { "Cherry", 5, 0, 1 }, { "Maroon", 3, 0, 0 }, { "Coral", 7, 3, 3 }, { "Salmon", 7, 4, 3 },
    { "Rose", 7, 3, 5 }, { "Pink", 7, 5, 6 }, { "Hot pink", 7, 1, 5 }, { "Magenta", 7, 0, 6 }, { "Fuchsia", 6, 0, 7 },
    { "Lavender", 6, 5, 7 }, { "Lilac", 5, 4, 7 }, { "Violet", 5, 2, 7 }, { "Purple", 4, 0, 5 }, { "Plum", 3, 0, 3 },
    { "Indigo", 2, 1, 5 }, { "Royal blue", 1, 2, 7 }, { "Blue", 0, 0, 7 }, { "Cobalt", 0, 2, 6 }, { "Navy", 0, 1, 4 },
    { "Midnight", 0, 0, 2 }, { "Denim", 2, 3, 5 }, { "Sky", 3, 6, 7 }, { "Ice", 5, 7, 7 }, { "Cyan", 0, 7, 7 },
    { "Turquoise", 1, 6, 6 }, { "Teal", 0, 5, 5 }, { "Aqua", 3, 7, 6 }, { "Mint", 4, 7, 5 }, { "Jade", 2, 6, 4 },
    { "Emerald", 1, 6, 2 }, { "Green", 0, 6, 0 }, { "Forest", 0, 3, 1 }, { "Lime", 5, 7, 1 }, { "Chartreuse", 4, 7, 0 },
    { "Olive", 4, 5, 1 }, { "Army", 3, 4, 2 }, { "Khaki", 6, 6, 3 }, { "Tan", 6, 5, 3 }, { "Brown", 4, 2, 1 },
    { "Chocolate", 3, 1, 0 }, { "Silver", 5, 5, 6 }, { "Steel", 4, 5, 6 }, { "Grey", 4, 4, 4 }, { "Slate", 3, 3, 4 },
    { "Charcoal", 2, 2, 2 }, { "Onyx", 1, 1, 1 }, { "Black", 0, 0, 0 },
};
#define COLOURS ((int)(sizeof colours / sizeof *colours))

/* The animations offered (vdp_outfit_color's, without the shimmer, which needs the line). */
static const int anims[] = { OUTFIT_ANIM_NONE, OUTFIT_ANIM_PULSE, OUTFIT_ANIM_RAINBOW, OUTFIT_ANIM_FLAME };
static const char *const anim_names[] = { "Still", "Pulse", "Rainbow", "Flame" };
#define ANIMS ((int)(sizeof anims / sizeof *anims))

static uint16_t cram_of(int r, int g, int b) { return (uint16_t)(b << 9 | g << 5 | r << 1); }
static int lum_of(uint16_t c) { return ((c >> 1) & 7) + ((c >> 5) & 7) + ((c >> 9) & 7); }
static int anim_slot(int anim) { for (int k = 0; k < ANIMS; k++) if (anims[k] == anim) return k; return 0; }

/* ---- Settings ------------------------------------------------------------------------ */

void fighters_defaults(void) {
    for (int f = 0; f < FIGHTERS_MAX; f++)
        for (int k = 0; k < FIGHTER_PARTS; k++) { settings.fighter_color[f][k] = -1; settings.fighter_anim[f][k] = 0; }
}

bool fighters_ini_load(const char *key, const char *value) {
    for (int f = 0; f < FIGHTERS; f++) {
        char name[32], anim[40];
        snprintf(name, sizeof name, "fighter_%s", ini_names[f]);
        snprintf(anim, sizeof anim, "fighter_%s_anims", ini_names[f]);
        bool is_anim = !strcmp(key, anim);
        if (!is_anim && strcmp(key, name)) continue;
        const char *p = value;
        for (int k = 0; k < FIGHTER_PARTS && *p; k++) {
            char *end;
            long v = is_anim ? strtol(p, &end, 10) : strtol(p, &end, 16);
            if (is_anim) settings.fighter_anim[f][k] = end != p && v >= 0 && v < OUTFIT_ANIMS ? (int)v : 0;
            else settings.fighter_color[f][k] = end != p && v >= 0 && v <= 0xEEE ? (int)(v & 0xEEE) : -1;
            p = strchr(p, ',');
            if (!p) break;
            p++;
        }
        return true;
    }
    return false;
}

void fighters_ini_save(FILE *f) {
    fprintf(f, "# The fighters' colours, part by part as the pause menu lists them (Genesis CRAM\n"
               "# colours, - = their own). The animations: 0 still, 2 pulse, 3 rainbow, 4 flame.\n");
    for (int g = 0; g < FIGHTERS; g++) {
        fprintf(f, "fighter_%s=", ini_names[g]);
        for (int k = 0; k < FIGHTER_PARTS; k++) {
            if (settings.fighter_color[g][k] < 0) fprintf(f, "%s-", k ? "," : "");
            else fprintf(f, "%s%03X", k ? "," : "", settings.fighter_color[g][k]);
        }
        fprintf(f, "\nfighter_%s_anims=", ini_names[g]);
        for (int k = 0; k < FIGHTER_PARTS; k++) fprintf(f, "%s%d", k ? "," : "", settings.fighter_anim[g][k]);
        fprintf(f, "\n");
    }
}

/* The colour of entry idx of fighter f with the given picks: OUTFIT_KEEP for the game's. */
static uint16_t entry_colour(int f, int idx, const int colour[FIGHTER_PARTS], const int anim[FIGHTER_PARTS],
                             uint16_t now, unsigned frame) {
    for (int k = 0; k < part_count[f]; k++) {
        if (colour[k] < 0) continue;
        for (int j = 0; j < parts[f][k].n; j++)
            if (parts[f][k].idx[j] == idx)
                return vdp_outfit_color((uint16_t)colour[k], anim[k], now, lum_of(parts[f][k].top), 30, frame);
    }
    return OUTFIT_KEEP;
}

static void capture_live(int p, int f);

/* Which fighter player p plays, or -1 when not in play. */
static int fighter_of(int p) {
#if GAME_SOR == 3
    uint16_t obj = p ? 0xE000 : 0xDF00, state = (uint16_t)(bus.ram[obj] << 8 | bus.ram[obj + 1]);
    if (!state || state == 0x5A || state == 0x5C || state == 0x5E || bus.ram[0xFB13]) return -1;
    int f = bus.ram[obj + 0x1F] - 1;    /* 1 Axel, 2 Blaze, 3 Skate, 4 Zan */
    return f >= 0 && f < FIGHTERS ? f : -1;
#elif GAME_SOR == 2
    uint16_t obj = p ? 0xF000 : 0xEF00, state = (uint16_t)(bus.ram[obj] << 8 | bus.ram[obj + 1]);
    if (!state || state >= 0x30 || bus.ram[0xFD10]) return -1;     /* not in a round, or the demo */
    for (int f = 0; f < FIGHTERS; f++) {                            /* its name, at +$78 */
        size_t n = strlen(fighter_names[f]);
        bool same = true;
        for (size_t k = 0; k < n && same; k++) same = bus.ram[obj + 0x78 + k] == (uint8_t)(fighter_names[f][k] & ~0x20);
        if (same && bus.ram[obj + 0x78 + n] <= ' ') return f;
    }
    return -1;
#else
    uint16_t obj = p ? 0xB880 : 0xB800;
    int f = bus.ram[(uint16_t)(obj + 0x50)];
    return bus.ram[obj] && f < FIGHTERS ? f : -1;
#endif
}

int fighters_colours(uint16_t out[32]) {
    static unsigned frame;
    frame++;
    int any = 0;
    for (int i = 0; i < 32; i++) out[i] = OUTFIT_KEEP;
    for (int p = 0; p < 2; p++) {
        int f = fighter_of(p);
        if (f < 0) continue;
        if (GAME_SOR >= 2) capture_live(p, f);
        for (int idx = 1; idx < 16; idx++) {
            uint16_t c = entry_colour(f, idx, settings.fighter_color[f], settings.fighter_anim[f], vdp.cram[idx], frame);
            if (c != OUTFIT_KEEP) { out[p * 16 + idx] = c; any = 1; }
        }
    }
    return any;
}

/* ---- The previews ------------------------------------------------------------------- */
#if GAME_SOR >= 2
#define PREV_W 112                      /* the later games' sprites are bigger */
#define PREV_H 128
#else
#define PREV_W 72
#define PREV_H 96
#endif
#define PREVIEW_MAGIC 0x31475446u       /* "FTG1" */
static uint8_t prev_img[FIGHTERS][PREV_W * PREV_H];   /* colour entries of palette 0, 0 = none */
static int prev_w[FIGHTERS], prev_h[FIGHTERS];
static uint16_t prev_cram[16];
static bool have_cram;

/* The select screen shows the three side by side, Adam, Axel, Blaze, all in palette 0:
 * each sprite entry below the portraits is drawn into its fighter's picture by where it
 * stands, and the pictures are then cropped to what was drawn. */
void fighters_capture_select(void) {
#if GAME_SOR >= 2
    return;                             /* its previews come from play (capture_live) */
#else
    static const int order[3] = { ADAM, AXEL, BLAZE };
    static uint8_t img[FIGHTERS][PREV_W * 2 * PREV_H * 2];
    memset(img, 0, sizeof img);
    uint32_t sat = (vdp.reg[5] & 0x7E) << 9;
    int oy = 120, found = 0;
    for (int i = 0; i < 80; i++) {
        uint32_t e = (sat + i * 8) & 0xFFFF;
        int y = (vdp.vram[e] << 8 | vdp.vram[e + 1]) & 0x3FF, size = vdp.vram[e + 2];
        int attr = vdp.vram[e + 4] << 8 | vdp.vram[e + 5], x = (vdp.vram[e + 6] << 8 | vdp.vram[e + 7]) & 0x1FF;
        y -= 128; x -= 128;
        if (y < oy || y >= 224 || x < -16 || x >= 320 || (attr & 0x6000)) continue;
        int col = x < 107 ? 0 : x < 212 ? 1 : 2, f = order[col];
        int base_x = col * 106, hc = ((size >> 2) & 3) + 1, vc = (size & 3) + 1;
        for (int sy = 0; sy < vc * 8; sy++)
            for (int sx = 0; sx < hc * 8; sx++) {
                int cx = (attr & 0x0800) ? hc * 8 - 1 - sx : sx, cy = (attr & 0x1000) ? vc * 8 - 1 - sy : sy;
                int tile = ((attr & 0x7FF) + (cx >> 3) * vc + (cy >> 3)) & 0x7FF;
                uint8_t b = vdp.vram[(tile * 32 + (cy & 7) * 4 + ((cx & 7) >> 1)) & 0xFFFF];
                int c = (cx & 1) ? (b & 0xF) : (b >> 4);
                int px = x - base_x + sx, py = y - oy + sy;
                if (!c || px < 0 || py < 0 || px >= PREV_W * 2 || py >= PREV_H * 2) continue;
                uint8_t *d = &img[f][py * PREV_W * 2 + px];
                if (!*d) *d = (uint8_t)c;              /* earlier entries are in front */
            }
        found++;
    }
    if (!found) return;
    for (int f = 0; f < FIGHTERS; f++) {
        int x0 = PREV_W * 2, y0 = PREV_H * 2, x1 = -1, y1 = -1;
        for (int y = 0; y < PREV_H * 2; y++)
            for (int x = 0; x < PREV_W * 2; x++)
                if (img[f][y * PREV_W * 2 + x]) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
        prev_w[f] = prev_h[f] = 0;
        memset(prev_img[f], 0, sizeof prev_img[f]);
        if (x1 < 0) continue;
        int w = x1 - x0 + 1, h = y1 - y0 + 1;
        if (w > PREV_W) w = PREV_W;
        if (h > PREV_H) h = PREV_H;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) prev_img[f][y * PREV_W + x] = img[f][(y0 + y) * PREV_W * 2 + x0 + x];
        prev_w[f] = w; prev_h[f] = h;
    }
    if (!have_cram) { memcpy(prev_cram, vdp.cram, sizeof prev_cram); have_cram = true; }
#endif
}

/* Streets of Rage 2 and later: a still of fighter f the first time player p, playing them,
 * stands without a button pressed for half a second (state 2, not hit or thrown): the
 * sprite pieces drawn from that player's tiles, saved to fighters.cache at once. A frame
 * with little drawn (the fighter arriving, or blinking) is passed over. */
static char cache_dir[1024];
static bool cache_dir_known;

static void capture_live(int p, int f) {
    static int idle[2];
    if (prev_w[f]) { idle[p] = 0; return; }
    uint16_t obj = GAME_SOR == 3 ? (p ? 0xE000 : 0xDF00) : (p ? 0xF000 : 0xEF00);
    bool standing = bus.ram[obj] == 0 && bus.ram[obj + 1] == 2;
    idle[p] = pad_state[p] || !standing ? 0 : idle[p] + 1;
    if (idle[p] < 30) return;
    idle[p] = 0;
    static uint8_t img[PREV_H * 2][PREV_W * 2];
    memset(img, 0, sizeof img);
    int lo = p ? FIGHTER_TILES_P2 : FIGHTER_TILES_P1, hi = p ? FIGHTER_TILES_END : FIGHTER_TILES_P2;
    uint32_t sat = (vdp.reg[5] & 0x7E) << 9;
    int minx = 1 << 30, miny = 1 << 30, link = 0, n = 0;
    struct { int x, y, size, attr; } pieces[40];
    int count = 0;
    do {                                  /* the pieces in this player's tiles, in drawing order */
        uint32_t e = (sat + link * 8) & 0xFFFF;
        int y = ((vdp.vram[e] << 8 | vdp.vram[e + 1]) & 0x3FF) - 128, size = vdp.vram[e + 2];
        int attr = vdp.vram[e + 4] << 8 | vdp.vram[e + 5], x = ((vdp.vram[e + 6] << 8 | vdp.vram[e + 7]) & 0x1FF) - 128;
        link = vdp.vram[e + 3] & 0x7F;
        if ((attr & 0x7FF) >= lo && (attr & 0x7FF) < hi && !(attr & 0x6000) && count < 40) {
            pieces[count].x = x; pieces[count].y = y; pieces[count].size = size; pieces[count].attr = attr; count++;
            if (x < minx) minx = x;
            if (y < miny) miny = y;
        }
    } while (link && ++n < 80);
    if (count < 2) return;
    for (int i = 0; i < count; i++) {
        int hc = ((pieces[i].size >> 2) & 3) + 1, vc = (pieces[i].size & 3) + 1, attr = pieces[i].attr;
        for (int sy = 0; sy < vc * 8; sy++)
            for (int sx = 0; sx < hc * 8; sx++) {
                int cx = (attr & 0x0800) ? hc * 8 - 1 - sx : sx, cy = (attr & 0x1000) ? vc * 8 - 1 - sy : sy;
                int tile = ((attr & 0x7FF) + (cx >> 3) * vc + (cy >> 3)) & 0x7FF;
                uint8_t b = vdp.vram[(tile * 32 + (cy & 7) * 4 + ((cx & 7) >> 1)) & 0xFFFF];
                int c = (cx & 1) ? (b & 0xF) : (b >> 4);
                int px = pieces[i].x - minx + sx, py = pieces[i].y - miny + sy;
                if (c && px < PREV_W * 2 && py < PREV_H * 2 && !img[py][px]) img[py][px] = (uint8_t)c;   /* earlier pieces are in front */
            }
    }
    int x0 = PREV_W * 2, y0 = PREV_H * 2, x1 = -1, y1 = -1;
    for (int y = 0; y < PREV_H * 2; y++)
        for (int x = 0; x < PREV_W * 2; x++)
            if (img[y][x]) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    int drawn = 0;
    for (int y = 0; y < PREV_H * 2; y++) for (int x = 0; x < PREV_W * 2; x++) drawn += img[y][x] != 0;
    if (x1 < 0 || drawn < 900) return;
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if (w > PREV_W) { x0 += (w - PREV_W) / 2; w = PREV_W; }
    if (h > PREV_H) { y0 += h - PREV_H; h = PREV_H; }               /* keep the feet */
    memset(prev_img[f], 0, sizeof prev_img[f]);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) prev_img[f][y * PREV_W + x] = img[y0 + y][x0 + x];
    prev_w[f] = w; prev_h[f] = h;
    memcpy(prev_cram, vdp.cram, sizeof prev_cram);
    have_cram = true;
    if (cache_dir_known) fighters_preview_save(cache_dir);
}

void fighters_capture_colours(void) {
    memcpy(prev_cram, vdp.cram, sizeof prev_cram);
    have_cram = true;
}

static void cache_path(char *out, size_t n, const char *dir) { snprintf(out, n, "%sfighters.cache", dir ? dir : ""); }

bool fighters_preview_save(const char *dir) {
    bool any = false;
    for (int k = 0; k < FIGHTERS; k++) any |= prev_w[k] > 0;
    if (!any) return false;
    char path[1100];
    cache_path(path, sizeof path, dir);
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    uint32_t magic = PREVIEW_MAGIC;
    fwrite(&magic, 4, 1, f);
    for (int k = 0; k < FIGHTERS; k++) { fwrite(&prev_w[k], 4, 1, f); fwrite(&prev_h[k], 4, 1, f); fwrite(prev_img[k], 1, sizeof prev_img[k], f); }
    fwrite(prev_cram, 2, 16, f);
    fclose(f);
    return true;
}

bool fighters_preview_load(const char *dir) {
    snprintf(cache_dir, sizeof cache_dir, "%s", dir ? dir : "");
    cache_dir_known = true;
    char path[1100];
    cache_path(path, sizeof path, dir);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint32_t magic = 0;
    bool ok = fread(&magic, 4, 1, f) == 1 && magic == PREVIEW_MAGIC;
    for (int k = 0; ok && k < FIGHTERS; k++)
        ok = fread(&prev_w[k], 4, 1, f) == 1 && fread(&prev_h[k], 4, 1, f) == 1 && prev_w[k] >= 0 && prev_w[k] <= PREV_W &&
             prev_h[k] >= 0 && prev_h[k] <= PREV_H && fread(prev_img[k], 1, sizeof prev_img[k], f) == sizeof prev_img[k];
    ok = ok && fread(prev_cram, 2, 16, f) == 16;
    fclose(f);
    if (!ok) memset(prev_w, 0, sizeof prev_w);
    have_cram = ok;
    return ok;
}

bool fighters_have_preview(int f) { return f >= 0 && f < FIGHTERS && prev_w[f] > 0; }

static Rgb colour_rgb(uint16_t c) { return unpack(vdp_color_rgb(c)); }

static void draw_with(int f, const int colour[FIGHTER_PARTS], const int anim[FIGHTER_PARTS], float cx, float bottom,
                      float scale, float alpha, double time_s) {
    if (!fighters_have_preview(f)) return;
    unsigned frame = (unsigned)(time_s * 60);
    uint16_t table[16];
    for (int i = 0; i < 16; i++) {
        uint16_t c = entry_colour(f, i, colour, anim, prev_cram[i], frame);
        table[i] = c != OUTFIT_KEEP ? c : prev_cram[i];
    }
    int w = prev_w[f], h = prev_h[f];
    float x0 = cx - w * scale / 2, y0 = bottom - h * scale;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int v = prev_img[f][y * PREV_W + x];
            if (v) ui_fill_rect(x0 + x * scale, y0 + y * scale, scale, scale, colour_rgb(table[v]), alpha);
        }
}

void fighters_draw(int f, float cx, float bottom, float scale, float alpha, double time_s) {
    if (f >= 0 && f < FIGHTERS) draw_with(f, settings.fighter_color[f], settings.fighter_anim[f], cx, bottom, scale, alpha, time_s);
}

/* ---- The page: the game's HUD, a yellow frame on charcoal, red bars ------------------ */
#include "theme.h"
#define HEAD     "Impact"

#define PANEL_X 120
#define PANEL_Y 30
#define PANEL_W 1040
#define PANEL_H 660
#define BOX_X   (PANEL_X + 36)
#define BOX_Y   (PANEL_Y + 150)
#define BOX_W   360
#define BOX_H   400
#define TAB_X   (PANEL_X + 36)
#define TAB_Y   (PANEL_Y + 88)
#define TAB_W   (FIGHTERS > 3 ? 84 : 120)
#define LIST_X  (PANEL_X + 440)
#define LIST_R  (PANEL_X + PANEL_W - 44)
#define LIST_Y  (PANEL_Y + 150)
#define ROW_H   58
#define SWATCH_X (LIST_X + 210)
#define BTN_Y   (PANEL_Y + PANEL_H - 104)
#define BTN_H   50

enum { ITEM_APPLY = FIGHTER_PARTS, ITEM_RESET, ITEM_CANCEL, ITEMS };

static bool open_, mixing;
static int fighter, selected, channel;  /* channel being mixed: 0 red, 1 green, 2 blue */
static int draft[FIGHTERS][FIGHTER_PARTS], draft_anim[FIGHTERS][FIGHTER_PARTS];
static double now;

static void sound(int kind) { if (outfit_sound) outfit_sound(kind); }

void fighters_open(int f) {
    open_ = true;
    mixing = false;
    fighter = f >= 0 && f < FIGHTERS ? f : 0;
    selected = 0;
    memcpy(draft, settings.fighter_color, sizeof draft);
    memcpy(draft_anim, settings.fighter_anim, sizeof draft_anim);
}

bool fighters_is_open(void) { return open_; }

static float row_y(int r) { return LIST_Y + r * ROW_H; }
static float btn_x(int item) { return LIST_X + (item - ITEM_APPLY) * 164; }
static bool on_part(void) { return selected < part_count[fighter]; }

static int hit_test(float mx, float my) {
    for (int k = 0; k < FIGHTERS; k++)
        if (mx >= TAB_X + k * (TAB_W + 8) && mx < TAB_X + k * (TAB_W + 8) + TAB_W && my >= TAB_Y && my < TAB_Y + 40) return 100 + k;
    for (int r = 0; r < part_count[fighter]; r++)
        if (mx >= LIST_X - 14 && mx <= LIST_R + 10 && my >= row_y(r) && my < row_y(r) + ROW_H) return r;
    for (int b = ITEM_APPLY; b < ITEMS; b++)
        if (mx >= btn_x(b) && mx <= btn_x(b) + 150 && my >= BTN_Y && my <= BTN_Y + BTN_H) return b;
    return -1;
}

static int preset_of(int c) {
    for (int k = 0; k < COLOURS; k++) if (cram_of(colours[k].r, colours[k].g, colours[k].b) == c) return k;
    return -1;
}

static void cycle_part(int part, int dir) {
    int *c = &draft[fighter][part], at = *c < 0 ? -1 : preset_of(*c);
    if (at < 0 && *c >= 0) at = dir > 0 ? -1 : COLOURS;      /* from a mix: to the first or last colour */
    at += dir;
    if (at < -1) at = COLOURS - 1;
    if (at >= COLOURS) at = -1;
    *c = at < 0 ? -1 : cram_of(colours[at].r, colours[at].g, colours[at].b);
}

static void cycle_anim(int part, int dir) {
    int *a = &draft_anim[fighter][part];
    *a = anims[settings_cycle(anim_slot(*a), ANIMS, dir)];
    if (*a && draft[fighter][part] < 0) draft[fighter][part] = parts[fighter][part].top;   /* animate its usual colour */
    sound(1);
}

static void mix_part(int part, int dir) {
    int c = draft[fighter][part] < 0 ? parts[fighter][part].top : draft[fighter][part];
    int ch[3] = { (c >> 1) & 7, (c >> 5) & 7, (c >> 9) & 7 };
    ch[channel] = ch[channel] + dir < 0 ? 0 : ch[channel] + dir > 7 ? 7 : ch[channel] + dir;
    draft[fighter][part] = cram_of(ch[0], ch[1], ch[2]);
}

static void switch_fighter(int dir) {
    fighter = (fighter + dir + FIGHTERS) % FIGHTERS;
    mixing = false;
    if (selected < ITEM_APPLY && selected >= part_count[fighter]) selected = part_count[fighter] - 1;
    sound(0);
}

static bool activate(void) {
    if (on_part()) { mixing = !mixing; channel = 0; sound(1); return false; }
    if (selected == ITEM_RESET) {
        for (int k = 0; k < FIGHTER_PARTS; k++) { draft[fighter][k] = -1; draft_anim[fighter][k] = 0; }
        sound(1);
        return false;
    }
    open_ = false;
    mixing = false;
    if (selected == ITEM_CANCEL) { sound(2); return false; }
    memcpy(settings.fighter_color, draft, sizeof draft);
    memcpy(settings.fighter_anim, draft_anim, sizeof draft_anim);
    sound(1);
    return true;
}

static void change(int dir) {
    if (on_part()) {
        if (mixing) mix_part(selected, dir);
        else cycle_part(selected, dir);
        sound(1);
    } else {
        int b = selected + dir;
        if (b >= ITEM_APPLY && b < ITEMS) { selected = b; sound(0); }
    }
}

static void move(int dir) {
    if (mixing) { channel = (channel + 3 + dir) % 3; sound(0); return; }
    int n = part_count[fighter], to;
    if (selected >= ITEM_APPLY) to = dir < 0 ? n - 1 : selected;
    else { to = selected + dir; if (to >= n) to = ITEM_APPLY; if (to < 0) to = ITEM_CANCEL; }
    if (to != selected) { selected = to; sound(0); }
}

bool fighters_event(const SDL_Event *e, float mx, float my) {
    if (!open_) return false;
    switch (e->type) {
    case SDL_KEYDOWN:
        switch (e->key.keysym.sym) {
        case SDLK_ESCAPE: case SDLK_BACKSPACE:
            if (mixing) mixing = false; else open_ = false;
            sound(2);
            break;
        case SDLK_UP: move(-1); break;
        case SDLK_DOWN: case SDLK_TAB: move(1); break;
        case SDLK_LEFT: change(-1); break;
        case SDLK_RIGHT: change(1); break;
        case SDLK_q: case SDLK_PAGEUP: switch_fighter(-1); break;
        case SDLK_e: case SDLK_PAGEDOWN: switch_fighter(1); break;
        case SDLK_SPACE:
            if (on_part() && !mixing) { cycle_anim(selected, 1); break; }
            return activate();
        case SDLK_RETURN: case SDLK_KP_ENTER: return activate();
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        switch (e->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK:
            if (mixing) mixing = false; else open_ = false;
            sound(2);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: move(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: move(1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: change(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: change(1); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: switch_fighter(-1); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: switch_fighter(1); break;
        case SDL_CONTROLLER_BUTTON_X: case SDL_CONTROLLER_BUTTON_Y:
            if (on_part() && !mixing) cycle_anim(selected, 1);
            break;
        case SDL_CONTROLLER_BUTTON_A: return activate();
        }
        break;
    case SDL_MOUSEMOTION: {
        int h = hit_test(mx, my);
        if (h >= 0 && h < 100 && h != selected && !mixing) { selected = h; sound(0); }
        break;
    }
    case SDL_MOUSEBUTTONDOWN: {
        int h = hit_test(mx, my);
        if (h >= 100) { if (h - 100 != fighter) switch_fighter(h - 100 - fighter); break; }
        if (e->button.button == SDL_BUTTON_RIGHT) {
            if (h >= 0 && h < ITEM_APPLY) { selected = h; change(-1); }
            else { open_ = false; sound(2); }
        } else if (h >= 0) {
            if (h != selected) mixing = false;
            selected = h;
            if (on_part() && !mixing && mx >= SWATCH_X + 34 && mx < SWATCH_X + 140) cycle_anim(h, 1);
            else if (on_part() && !mixing) change(1);
            else return activate();
        }
        break;
    }
    }
    return false;
}

/* ---- Drawing ------------------------------------------------------------------------- */
static void frame_rect(float x, float y, float w, float h, float t, Rgb c, float a) {
    ui_fill_rect(x, y, w, t, c, a);
    ui_fill_rect(x, y + h - t, w, t, c, a);
    ui_fill_rect(x, y + t, t, h - 2 * t, c, a);
    ui_fill_rect(x + w - t, y + t, t, h - 2 * t, c, a);
}

static void shadow_text(const char *s, float px, float x, float y, Rgb c) {
    ui_draw_text(s, HEAD, px, 400, 3, x + px / 16, y + px / 16, C_DEEP, C_DEEP, .9f);
    ui_draw_text(s, HEAD, px, 400, 3, x, y, c, mix(c, C_AMBER, .35f), 1);
}

static void draw_bars(float x, float y, int c) {
    int ch[3] = { (c >> 1) & 7, (c >> 5) & 7, (c >> 9) & 7 };
    static const char *const names[3] = { "R", "G", "B" };
    static const uint32_t tints[3] = { 0xF06A5A, 0x6AD07A, 0x6A9AF0 };
    for (int k = 0; k < 3; k++) {
        float bx = x + k * 96;
        bool on = channel == k;
        Rgb lc = on ? rgb(1, 1, 1) : C_DIM;
        ui_draw_text(names[k], "Segoe UI Semibold", 14, 600, 0, bx, y - 9, lc, lc, 1);
        for (int s = 0; s < 7; s++)
            ui_fill_rect(bx + 16 + s * 10, y - 6, 7, 12, s < ch[k] ? rgb_hex(tints[k]) : rgb_hex(0x34343C), on ? 1 : .6f);
        if (on) ui_fill_rect(bx - 4, y + 10, 92, 2, C_YELLOW, .9f);
    }
}

static void button(float x, float y, float w, float h, const char *label, bool primary, bool focus, float t) {
    ui_fill_rect(x, y, w, h, primary ? C_RED : rgb_hex(0x23232A), .96f);
    frame_rect(x, y, w, h, focus ? 4 : 2, focus ? C_YELLOW : primary ? C_AMBER : rgb_hex(0x56565E), focus ? .75f + .25f * sinf(t * 6) : .9f);
    float tw = ui_text_width(label, HEAD, 22, 400, 3);
    shadow_text(label, 22, x + w / 2 - tw / 2, y + h / 2 - 14, focus ? C_YELLOW : rgb(1, 1, 1));
}

static const char *colour_label(int c) {
    if (c < 0) return "Own";
    int at = preset_of(c);
    return at >= 0 ? colours[at].name : "Custom";
}

void fighters_render(double time_s) {
    now = time_s;
    if (!open_) return;
    float t = (float)time_s;
    ui_fill_rect(0, 0, UI_BASE_W, UI_BASE_H, rgb(0, 0, 0), .65f);
    ui_fill_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, C_INK, .97f);
    frame_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 4, C_YELLOW, 1);
    shadow_text("FIGHTERS' COLOURS", 40, PANEL_X + 36, PANEL_Y + 24, C_YELLOW);
    ui_fill_rect(PANEL_X + 38, PANEL_Y + 72, 140, 3, C_RED, .9f);

    /* The fighters, as tabs. */
    for (int k = 0; k < FIGHTERS; k++) {
        float x = TAB_X + k * (TAB_W + 8);
        bool on = k == fighter;
        ui_fill_rect(x, TAB_Y, TAB_W, 40, on ? C_YELLOW : rgb_hex(0x24242B), on ? 1 : .9f);
        if (on) ui_fill_rect(x, TAB_Y + 36, TAB_W, 4, C_RED, 1);
        else frame_rect(x, TAB_Y, TAB_W, 40, 1, rgb_hex(0x4A4A52), 1);
        char up[16];
        snprintf(up, sizeof up, "%s", fighter_names[k]);
        for (char *p = up; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        float tw = ui_text_width(up, HEAD, 20, 400, 2);
        ui_draw_text(up, HEAD, 20, 400, 2, x + TAB_W / 2 - tw / 2, TAB_Y + 8, on ? C_INK : C_DIM, on ? C_INK : C_DIM, 1);
    }

    /* The preview: the fighter in the draft colours, under a street lamp's pool of light. */
    ui_fill_rect(BOX_X, BOX_Y, BOX_W, BOX_H, rgb_hex(0x0C0C10), 1);
    frame_rect(BOX_X, BOX_Y, BOX_W, BOX_H, 2, C_AMBER, .8f);
    float cx = BOX_X + BOX_W / 2.f, floor_y = BOX_Y + BOX_H - 30;
    for (int k = 0; k < 16; k++) {
        float w = 200 - k * 10, h = 24 - k * 1.2f;
        ui_fill_rect(cx - w / 2, floor_y - h / 2 + 4, w, h, rgb_hex(0xF0B060), .02f);
    }
    ui_draw_text("PREVIEW", "Segoe UI Semibold", 11.5f, 600, 3.5f, BOX_X + 18, BOX_Y + 14, C_RED, C_RED, 1);
    if (fighters_have_preview(fighter)) {
        float scale = floorf(fminf((BOX_W - 60) / (float)prev_w[fighter], (BOX_H - 70) / (float)prev_h[fighter]));
        scale = clampf(scale, 2, 5);
        draw_with(fighter, draft[fighter], draft_anim[fighter], cx, floor_y, scale, 1, time_s);
        /* As they are now, for comparison. */
        ui_draw_text("NOW", "Segoe UI Semibold", 11.5f, 600, 3.5f, BOX_X + 4, BOX_Y + BOX_H + 20, C_DIM, C_DIM, 1);
        fighters_draw(fighter, BOX_X + 90, BOX_Y + BOX_H + 12 + prev_h[fighter] * .9f, .9f, 1, time_s);
    } else {
        const char *l1 = "Start the game once from the launcher", *l2 = "to see the fighters here.";
        float w1 = ui_text_width(l1, "Segoe UI", 15, 400, 0), w2 = ui_text_width(l2, "Segoe UI", 15, 400, 0);
        ui_draw_text(l1, "Segoe UI", 15, 400, 0, cx - w1 / 2, BOX_Y + BOX_H / 2 - 22, C_DIM, C_DIM, 1);
        ui_draw_text(l2, "Segoe UI", 15, 400, 0, cx - w2 / 2, BOX_Y + BOX_H / 2, C_DIM, C_DIM, 1);
    }

    /* The parts. */
    for (int r = 0; r < part_count[fighter]; r++) {
        float y = row_y(r);
        bool focus = selected == r;
        if (focus) {
            ui_fill_rect(LIST_X - 14, y + 4, LIST_R - LIST_X + 24, ROW_H - 8, C_RED, .22f);
            ui_fill_rect(LIST_X - 14, y + 4, 5, ROW_H - 8, C_YELLOW, 1);
        }
        Rgb lc = focus ? rgb(1, 1, 1) : C_TEXT, vc = focus ? C_YELLOW : C_VALUE;
        float ty = y + ROW_H / 2.f - 11;
        ui_draw_text(parts[fighter][r].name, "Segoe UI", 17.5f, 400, 0, LIST_X, ty, lc, lc, 1);
        int c = draft[fighter][r] < 0 ? parts[fighter][r].top : draft[fighter][r];
        Rgb sw = colour_rgb((uint16_t)c);
        ui_fill_rect(SWATCH_X, y + ROW_H / 2.f - 13, 26, 26, sw, 1);
        frame_rect(SWATCH_X - 1, y + ROW_H / 2.f - 14, 28, 28, 1, focus ? C_YELLOW : C_DIM, 1);
        if (focus && mixing) {
            draw_bars(SWATCH_X + 48, y + ROW_H / 2.f, c);
            continue;
        }
        int a = draft_anim[fighter][r];
        if (a) ui_draw_text(anim_names[anim_slot(a)], "Segoe UI Semibold", 13, 600, 1, SWATCH_X + 38, y + ROW_H / 2.f - 8, C_AMBER, C_AMBER, 1);
        const char *v = colour_label(draft[fighter][r]);
        float vw = ui_text_width(v, "Segoe UI Semibold", 17.5f, 600, 0);
        ui_draw_text(v, "Segoe UI Semibold", 17.5f, 600, 0, LIST_R - 22 - vw, ty, vc, vc, 1);
        Rgb ac = focus ? C_YELLOW : rgb_hex(0x5E5A52);
        ui_draw_text("\xE2\x80\xB9", "Segoe UI Semibold", 20, 600, 0, LIST_R - 22 - vw - 18, ty - 3, ac, ac, 1);
        ui_draw_text("\xE2\x80\xBA", "Segoe UI Semibold", 20, 600, 0, LIST_R - 10, ty - 3, ac, ac, 1);
    }

    /* What the row does, or how to mix. */
    float ay = row_y(FIGHTER_PARTS) + 4;
    const char *about = on_part() ? (mixing ? "\xE2\x86\x91\xE2\x86\x93 red, green, blue    \xE2\x86\x90\xE2\x86\x92 less / more    Enter  done"
                                            : parts[fighter][selected].about) :
                        selected == ITEM_APPLY ? "Use these colours in the game, for whoever plays them." :
                        selected == ITEM_RESET ? "Back to this fighter's own colours (Apply to keep it)." : "Leave without changing anything.";
    ui_draw_text(about, "Segoe UI", 14, 400, 0, LIST_X, ay, rgb_hex(0xBDB7AA), rgb_hex(0xBDB7AA), 1);
    if (on_part() && !mixing)
        ui_draw_text("\xE2\x86\x90\xE2\x86\x92 colours    Space  animation    Enter  mix any of 512", "Segoe UI", 13, 400, 1, LIST_X, ay + 24,
                     C_DIM, C_DIM, 1);

    static const char *const labels[3] = { "APPLY", "RESET", "CANCEL" };
    for (int b = ITEM_APPLY; b < ITEMS; b++) button(btn_x(b), BTN_Y, 150, BTN_H, labels[b - ITEM_APPLY], b == ITEM_APPLY, selected == b, t);
    ui_draw_text("\xE2\x86\x91\xE2\x86\x93 select    \xE2\x86\x90\xE2\x86\x92 change    Q E  fighter    Space  animate    Enter  mix / choose    Esc  back",
                 "Segoe UI", 12, 400, 1, LIST_X, PANEL_Y + PANEL_H - 36, rgb_hex(0x77736A), rgb_hex(0x77736A), 1);
}
