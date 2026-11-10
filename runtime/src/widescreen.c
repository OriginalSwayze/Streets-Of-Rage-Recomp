/* Widescreen: the game's tile-map streaming, rewritten in C to keep a wider band of
 * columns valid in VRAM.
 *
 * Each scrolling layer has a 0x28-byte struct ($FF1716 for plane A, $FF173E for plane B):
 *   +00 camera x (pixels)        +04 camera y (pixels)
 *   +08 left map column          +0A top map row
 *   +0C VRAM offset of the top-left cell (column * 2 + row * $80, 64x32 plane)
 *   +16 map pointer (long)       +1A VRAM nametable base
 *   +1C pending column steps     +1E pending row steps (sign = direction)
 *   +20 map width (cells)        +22 map height (cells)    +24 log2 of the map row stride
 *   +26 dirty flags (VBlank)     +27 bit 0 = layer has a map, bit 1 = map fits the whole plane
 *
 * The original routines keep 41 columns valid: the left map column and the 40 to its
 * right. These versions keep `pad` more columns on each side (up to 11, so the band
 * still fits the 64-column plane), and draw cells outside the map as the blank tile the
 * game itself uses for empty planes. Each one replaces a whole subroutine: it does the
 * same work as the original, then returns to the caller. Registers the original leaves
 * in a known state (the VDP port pointers) are set the same way; the others were
 * scratch for the original too. */
#include <string.h>
#include "genesis.h"
#include "recomp_rt.h"

#define VDP_DATA  0xC00000
#define VDP_CTRL  0xC00004
#define BLANK_CELL 0x0780               /* tile entry sub_00D962 fills empty planes with */

static int pad;                         /* extra columns kept valid on each side */
uint32_t patch_cull_left, patch_cull_widen;
uint32_t patch_sor_x_lo = 0x60, patch_sor_x_hi = 0x1E0;
int patch_sor_wide, patch_sor_reach, patch_sor_reach_right, patch_sor_reach_left;
int patch_sor_cull_left, patch_sor_cull_right, patch_sor_x_cap = 0x200;
int patch_sprite_cap = 0x50;
int patch_cam_margin;

static int16_t L16(uint32_t layer, int off) { return (int16_t)rd16(layer + off); }

static void vram_cell(uint32_t base, int vrow, int vcol, uint32_t v) {
    uint32_t a = (base + (uint32_t)(((vrow & 31) * 64 + (vcol & 63)) * 2)) & 0xFFFF;
    wr16(VDP_CTRL, 0x4000 | (a & 0x3FFF));
    wr16(VDP_CTRL, a >> 14);
    wr16(VDP_DATA, v);
}

/* Map cell (col, row): rows wrap like the original; columns outside the map are blank. */
static uint32_t map_cell(uint32_t layer, int col, int row) {
    int w = L16(layer, 0x20), h = L16(layer, 0x22);
    if (col < 0 || col >= w) return BLANK_CELL;
    row %= h;
    if (row < 0) row += h;
    return rd16(rd32(layer + 0x16) + ((uint32_t)row << L16(layer, 0x24)) + (uint32_t)col * 2);
}

static void draw_column(uint32_t layer, int col, int vcol) {
    int top = L16(layer, 0x0A), vtop = (L16(layer, 0x0C) >> 7) & 31;
    uint32_t base = (uint16_t)L16(layer, 0x1A);
    for (int i = 0; i < 29; i++) vram_cell(base, vtop + i, vcol, map_cell(layer, col, top + i));
}

static void draw_row(uint32_t layer, int row, int vrow) {
    int left = L16(layer, 0x08), vleft = (L16(layer, 0x0C) >> 1) & 63;
    uint32_t base = (uint16_t)L16(layer, 0x1A);
    for (int k = -pad; k <= 40 + pad; k++) vram_cell(base, vrow, vleft + k, map_cell(layer, left + k, row));
}

static uint32_t return_to_caller(int cycles) {
    USE_CYCLES(cycles);
    REG_PC = pop32();
    return REG_PC;
}

static void port_registers(void) { A(2) = VDP_DATA; A(4) = VDP_CTRL; }

/* The original raises the interrupt mask to 7 and then clears two bits of it, so the
 * caller always gets mask 1 back. */
static void interrupt_mask_after_vdp_access(void) { m68ki_set_sr((m68ki_get_sr() | 0x0700) & 0xF9FF); }

/* sub_0023CC: scroll one column (VBlank; called while +1C != 0). */
static uint32_t scroll_column(uint32_t pc) {
    (void)pc;
    uint32_t layer = A(3);
    int w = L16(layer, 0x20), steps = L16(layer, 0x1C);
    int left = L16(layer, 0x08), c = L16(layer, 0x0C);
    int target, vtarget;
    if (steps >= 0) {
        wr16(layer + 0x1C, (uint32_t)(steps - 1));
        if (++left >= w) left -= w;
        c = (((c & 0x7E) + 2) & 0x7E) | (c & 0xF80);
        target = left + 40 + pad;
        vtarget = ((c & 0x7E) >> 1) + 40 + pad;
    } else {
        wr16(layer + 0x1C, (uint32_t)(steps + 1));
        if (--left < 0) left += w;
        c = (((c & 0x7E) - 2) & 0x7E) | (c & 0xF80);
        target = left - pad;
        vtarget = ((c & 0x7E) >> 1) - pad;
    }
    wr16(layer + 0x08, (uint32_t)left);
    wr16(layer + 0x0C, (uint32_t)c);
    draw_column(layer, target, vtarget);
    port_registers();
    return return_to_caller(2700);
}

/* sub_002512: scroll one row (VBlank; called while +1E != 0). */
static uint32_t scroll_row(uint32_t pc) {
    (void)pc;
    uint32_t layer = A(3);
    int h = L16(layer, 0x22), steps = L16(layer, 0x1E);
    int top = L16(layer, 0x0A), c = L16(layer, 0x0C);
    int target, vtarget;
    if (steps >= 0) {
        wr16(layer + 0x1E, (uint32_t)(steps - 1));
        if (++top >= h) top -= h;
        c = (((c & 0xF80) + 0x80) & 0xF80) | (c & 0x7E);
        target = top + 0x1C;
        vtarget = (c >> 7) + 0x1C;
    } else {
        wr16(layer + 0x1E, (uint32_t)(steps + 1));
        if (--top < 0) top += h;
        c = (((c & 0xF80) - 0x80) & 0xF80) | (c & 0x7E);
        target = top;
        vtarget = c >> 7;
    }
    wr16(layer + 0x0A, (uint32_t)top);
    wr16(layer + 0x0C, (uint32_t)c);
    draw_row(layer, target, vtarget);
    port_registers();
    return return_to_caller(2200 + pad * 100);
}

/* Fills the columns visible with zero scroll (0..40+pad and the last `pad`) with the blank cell. */
static void clear_plane(uint32_t base) {
    for (int row = 0; row < 29; row++)
        for (int k = -pad; k <= 40 + pad; k++) vram_cell(base, row, k, BLANK_CELL);
}

/* sub_00D7C0: set up a layer for a new area and draw it. */
static uint32_t init_layer(uint32_t pc) {
    (void)pc;
    uint32_t layer = A(3);
    static const int zeroed[] = { 0x0E, 0x10, 0x0C, 0x08, 0x0A, 0x1C, 0x1E };
    for (unsigned i = 0; i < sizeof zeroed / sizeof *zeroed; i++) wr16(layer + zeroed[i], 0);
    uint32_t base = (uint16_t)L16(layer, 0x1A);
    port_registers();
    if (!(rd8(layer + 0x27) & 1)) {     /* no map: clear the plane (loc_00D95E) */
        clear_plane(base);
        interrupt_mask_after_vdp_access();
        return return_to_caller(41 * 29 * 12 + 200);
    }
    int w = L16(layer, 0x20), h = L16(layer, 0x22);
    int left = L16(layer, 0x00) >> 3, top = L16(layer, 0x04) >> 3;
    left %= w; if (left < 0) left += w;
    top %= h;  if (top < 0) top += h;
    wr16(layer + 0x08, (uint32_t)left);
    wr16(layer + 0x0A, (uint32_t)top);
    bool whole_plane = rd8(layer + 0x27) & 2;
    if (!whole_plane && h == 32 && (uint16_t)w <= 64) { wr8(layer + 0x27, rd8(layer + 0x27) | 2); whole_plane = true; }
    if (whole_plane) {
        /* The map fits the plane: draw all of it. A 64-wide map wraps around the plane
         * like the original draws it; a narrower one is blank past its right edge. */
        for (int row = 0; row < 32; row++)
            for (int k = 0; k < 64; k++)
                vram_cell(base, row, k, map_cell(layer, w == 64 ? (left + k) % 64 : left + k, top + row));
    } else {
        for (int row = 0; row < 29; row++)
            for (int k = -pad; k <= 40 + pad; k++) vram_cell(base, row, k, map_cell(layer, left + k, top + row));
    }
    interrupt_mask_after_vdp_access();
    return return_to_caller(41 * 29 * 40 + 400);
}

/* sub_00D950: clear the plane at d0 with the blank cell (keeps registers). */
static uint32_t clear_plane_at(uint32_t pc) {
    (void)pc;
    clear_plane((uint16_t)D(0));
    interrupt_mask_after_vdp_access();
    return return_to_caller(41 * 29 * 12 + 300);
}

/* sub_00DC26: redraw a changed map rectangle (doors, switches...), clipped to the
 * columns kept in VRAM. In: d1/d2 map column/row, d3/d4 width-1/height-1, a1 source
 * cells (stride = map width). Maps that fill the plane take the original path. */
static uint32_t redraw_rect(uint32_t pc) {
    (void)pc;
    uint32_t layer = A(3);
    if (rd8(layer + 0x27) & 2) return 0xDD14;
    int col = (int16_t)D(1) - L16(layer, 0x08), row = (int16_t)D(2) - L16(layer, 0x0A);
    int cols = (int16_t)D(3) + 1, rows = (int16_t)D(4) + 1;
    uint32_t src = A(1), stride = (uint32_t)(uint16_t)L16(layer, 0x20) * 2;
    if (col < -pad) { cols += col + pad; src += (uint32_t)(-pad - col) * 2; col = -pad; }
    if (cols > 42 + pad - col) cols = 42 + pad - col;
    if (row < 0) { rows += row; src += (uint32_t)(-row) << L16(layer, 0x24); row = 0; }
    if (rows > 30 - row) rows = 30 - row;
    if (cols <= 0 || rows <= 0) return return_to_caller(100);
    int vleft = (L16(layer, 0x0C) >> 1) & 63, vtop = (L16(layer, 0x0C) >> 7) & 31;
    uint32_t base = (uint16_t)L16(layer, 0x1A);
    for (int r = 0; r < rows; r++)
        for (int k = 0; k < cols; k++) vram_cell(base, vtop + row + r, vleft + col + k, rd16(src + r * stride + (uint32_t)k * 2));
    port_registers();
    interrupt_mask_after_vdp_access();
    return return_to_caller(rows * cols * 30 + 300);
}

/* Streets of Rage 2 and 3: the wide view at the ends of a map. `before` and `after` are how
 * far the wide view would show past the map's left and right ends (0 or less inside it).
 * Rather than show what the planes hold there (leftovers of other scenes), the view slides
 * back inside the map, taking its width from the other side (vdp_opts.view_slide); the HUD,
 * on the window plane, stays. What it cannot cover stays masked (blurred or black, hd_post).
 *
 * Sliding left (at a map's end) shows columns the game drew while the camera came, which the
 * 512-pixel planes keep for 512 - 344 - 2 x the widescreen width past the camera's left
 * (the right column drawers reach 344 + that width). Sliding right (at a map's start) shows
 * columns the row drawers filled when the area began (to 456 or 512). Sprites are drawn as
 * far as the view goes (patch_sor_cull_left/right); past hardware X $1FF (sliding right) the
 * VDP reads a 10th bit of X, which the games write whole. The players still stop at the
 * map's end. */
/* Streets of Rage 3: places drawn for 320 pixels only, which keep to them in widescreen (the
 * sides masked, the players inside the original edges). Things wait just past the original
 * edges there, or the planes hold other graphics beside the picture. By round ($FFFB05, from 0),
 * area ($FFFB03) and camera X ($FFF710). */
static const struct { uint8_t round, area; int16_t cam_lo, cam_hi; } sor3_narrow[] = {
    { 0, 1, 640, 0x7FFF },  /* round 1, the deck at sunset: the boat's other end, its hull beside */
    { 2, 1, 0, 0x7FFF },    /* round 3, the bulldozer: its back waits on the left */
    { 2, 2, 0, 0x7FFF },    /* round 3, the lift up the girders, and the roof it stops at */
    { 3, 0, 0, 0x7FFF },    /* round 4, the subway: the carts wait beside the screen */
    { 3, 1, 0, 0x7FFF },    /* round 4, the room with the cart */
    { 3, 2, 0, 0x7FFF },    /* round 4, the shrine: Yamato runs off past the original edge */
};

static bool sor3_keeps_narrow(int cam) {
    int r = rd8(0xFFFB05), a = rd8(0xFFFB03);
    for (size_t k = 0; k < sizeof sor3_narrow / sizeof *sor3_narrow; k++)
        if (sor3_narrow[k].round == r && sor3_narrow[k].area == a && cam >= sor3_narrow[k].cam_lo && cam <= sor3_narrow[k].cam_hi)
            return true;
    return false;
}

/* A talk inside a round (round 6's General Petrov, the roof after the round's boss): the game
 * runs only the talk then. Its list of active objects ($FFF72C: their slots' addresses, in
 * drawing order, 0 after the last) holds no player's ($DF00, $E000, and $E100 for a third),
 * where in play a player is in it (but for a frame now and then). The talk's picture is
 * drawn for 320 pixels, and in the wide sides the street went on, or the view had slid at
 * a map's end (#20). */
static bool sor3_talk(void) {
    uint16_t first = rd16(0xFFF72C);
    if (!first) return false;           /* an empty list: the round loading */
    for (uint32_t k = 0; k < 64; k++) {
        uint16_t slot = rd16(0xFFF72C + 2 * k);
        if (!slot) break;
        if (slot == 0xDF00 || slot == 0xE000 || slot == 0xE100) return false;
    }
    return true;
}

static void slide_view(int before, int after) {
    int extra = vdp_opts.extra_width;
    patch_sor_cull_left = patch_sor_cull_right = patch_sor_reach_left = extra;
    patch_sor_x_cap = 0x200;
    patch_sor_reach_right = extra - (after < 0 ? 0 : after < extra ? after : extra);
    vdp_opts.view_slide = 0;
    vdp_opts.sprite_x_wide = false;
    if (!extra || (before > 0 && after > 0)) {
        vdp_opts.mask_left = before > 0 ? (before < extra ? before : extra) : 0;
        vdp_opts.mask_right = after > 0 ? (after < extra ? after : extra) : 0;
        return;
    }
    if (after > 0) {
        int room = 512 - 344 - 2 * extra, d = after < extra ? after : extra;
        if (d > room) d = room > 0 ? room : 0;
        if (d > -before) d = -before;           /* not past the map's start either */
        vdp_opts.view_slide = d;
        patch_sor_cull_left = extra + d;
        vdp_opts.mask_right = (after < extra ? after : extra) - d;
    } else if (before > 0) {
        int d = before < extra ? before : extra;
        if (d > -after) d = -after;
        vdp_opts.view_slide = -d;
        patch_sor_cull_right = extra + d;
        patch_sor_x_cap = 0x200 + d + 16;
        vdp_opts.sprite_x_wide = true;
        vdp_opts.mask_left = (before < extra ? before : extra) - d;
    }
}

/* The extra columns only show real content while a map layer is active (gameplay and
 * in-engine scenes). Logos, the title, menus and story scenes are drawn for 320 pixels,
 * so their sides are masked. */
void widescreen_frame(void) {
    vdp_opts.mask_left = vdp_opts.mask_right = 0;
    vdp_opts.view_slide = 0;
    vdp_opts.still_planes = false;
    vdp_opts.sprite_x_wide = false;
    if (GAME_SOR == 3) {
        /* Streets of Rage 3: each area's camera ($FFF710) starts at its map's left end, X 0.
         * The map is a table of 256-pixel blocks, 16 a row (plane A's at $FF8C00, RAM offsets;
         * zero past the map's end, the first block being at offset 0 too): it ends after its
         * last block (round 1's first area: 8 blocks, the camera stopping at $6C0). Its HUD is
         * plane B's top 40 lines (scrolled 0; the street's background is below them). The
         * game mode ($FFFB01) is $0E in play; the round's title, the bonus tally and the
         * story scenes between rounds are other modes, drawn for 320 pixels. */
        uint16_t s = rd16(0xFFDF00);
        bool round = s && s != 0x5C && s != 0x5E && s != 0x5A && !rd8(0xFFFB13) && rd8(0xFFFB01) == 0x0E
                     && vdp_screen_width() == 320;   /* Bare Knuckle III's talks in a round are 256 wide */
        int cam = (int16_t)rd16(0xFFF710), extra = vdp_opts.extra_width;
        /* The map is the plane that moves with the camera: each plane's camera is at $FF8890
         * (A) and $FF8880 (B). Mostly plane A; in round 3's bar plane A is a foreground going by
         * faster, its table a pattern repeated 16 blocks, and the bar is plane B's 7 blocks; in
         * round 5's rooms plane A's table is empty and each room is plane B's two blocks.
         * Reading plane A's there showed the round's start again past the bar's end, and had
         * the rooms masked at their start and slid at a 256-pixel end. */
        int blocks = 0, blocks_a = 0;
        bool moves[2] = { false, false };
        for (int table = 0; table < 2; table++) {
            int n = 0, pcam = (int16_t)rd16(table ? 0xFF8880 : 0xFF8890);
            for (int row = 0; row < 16 && round; row++)
                for (int k = 15; k >= n; k--)
                    if (rd16((table ? 0xFF8A00 : 0xFF8C00) + (uint32_t)(row * 32 + k * 2))) { n = k + 1; break; }
            if (!table) blocks_a = n;
            moves[table] = pcam - cam <= 8 && cam - pcam <= 8;
            if (moves[table] && n && !blocks) blocks = n;     /* plane A first: round 2's bar has both, B's a repeated aquarium */
        }
        if (!blocks) blocks = blocks_a;     /* neither moves with it: as before */
        /* The HUD is the top 40 lines of the plane that is not the map: plane B's mostly, plane
         * A's where plane B is the map (the bar, round 5's rooms). */
        vdp_opts.hud_b_lines = 40;
        vdp_opts.hud_plane = moves[1] && !moves[0] ? 0 : 1;
        /* Strips that hold still while the camera moves are 320-wide pictures (the ship
         * deck's edge, the boat's graphics beside it): vdp.c mirrors or clears their sides. */
        vdp_opts.still_planes = round;
        vdp_opts.still_cam = cam;
        /* The section's camera limit ($FF8864): when it lies in the map's last block, the
         * map ends where the camera stops (limit + 320), short of a whole block when the
         * room is (round 3's bar: two blocks, the camera held at 96, the art ending at 416). */
        int end = (blocks ? blocks : 1) * 256, limit = (int16_t)rd16(0xFF8864);
        if (limit >= 0 && limit + 320 > end - 256 && limit + 320 < end) end = limit + 320;
        /* A room of one screen (the camera held near where the area began, at its limit) whose art
         * the wide view cannot fill without a black side (round 7's 205): it keeps to 4:3,
         * even on both sides. */
        static int seen_area = -1, cam_at_start;
        static bool cam_moved;
        int here = rd8(0xFFFB05) << 8 | rd8(0xFFFB03);
        if (!round) seen_area = -1;
        else if (here != seen_area) { seen_area = here; cam_at_start = cam; cam_moved = false; }
        else if (cam - cam_at_start > 128 || cam_at_start - cam > 128) cam_moved = true;   /* settling in is not moving */
        /* Bare Knuckle III DX's motorbike stages (7A's and 7B's first scenes: round $FFFB05 6
         * and 7, area 0): the road scrolls by itself and has no end to stop at. The map and its
         * limit there are not the street's, and the view slid, blacking out the HUD's strip. */
        bool bike = round && !strcmp(game->id, "dx") && rd8(0xFFFB05) >= 6 && rd8(0xFFFB03) == 0;
        bool narrow = round && !bike && sor3_keeps_narrow(cam);
        if (bike) slide_view(-extra, -extra);
        else if (round && !narrow) {
            slide_view(extra - cam, cam + 320 + extra - end);
            narrow = !cam_moved && cam == limit && (vdp_opts.mask_left > 0 || vdp_opts.mask_right > 0);
        }
        if (narrow) {
            slide_view(extra, extra);       /* both sides masked */
            patch_sor_reach_left = 0;
        } else if (!round) slide_view(-extra, -extra);
        /* A talk: held for a few frames (a player thrown is out of the list for one), and only
         * the picture changes, so that play is the same either way. */
        static int talk_frames;
        talk_frames = round && sor3_talk() ? (talk_frames < 8 ? talk_frames + 1 : 8) : 0;
        if (talk_frames >= 4) {
            vdp_opts.mask_left = vdp_opts.mask_right = extra;
            vdp_opts.view_slide = 0;
            vdp_opts.sprite_x_wide = false;
        }
    }
    if (GAME_SOR == 2) {
        /* Streets of Rage 2: each area of a stage scrolls to a limit per section (the table at
         * $003996: by stage, $FFFC42, a list of camera X limits, negated, two entries a
         * section, $FFFC44). The camera ($FFFC22, negated) stops there; at an area's end (the
         * stage's last section, or one before a new area with a smaller limit) its map ends
         * at the limit + 320 (past it, at the end of round 2, part of the truck showed). Each
         * area's map starts at camera X 0 (round 1's bar and the alley after it start there
         * again). */
        uint16_t s = rd16(0xFFEF00);
        int stage = rd16(0xFFFC42), section = rd16(0xFFFC44), extra = vdp_opts.extra_width;
        int cam = -(int16_t)rd16(0xFFFC22), before = -extra, after = -extra;
        /* $32: the round's title card. The stage's bonus tally keeps the player's state; its
         * text attribute ($FFFC12, $8760 in play) is $A760 then. */
        bool tally = rd8(0xFFFC12) & 0x20;
        bool round = extra && s && s != 0x32 && s != 0x34 && s != 0x36 && !tally;
        if (round) before = extra - cam;
        if (round && !rd8(0xFFFD10) && stage >= 0 && stage < 16 && !(stage & 1) && section >= 0 && section < 64) {
            uint32_t ptr = 0x003996 + (uint32_t)stage, table = ptr + (uint32_t)(int16_t)rd16(ptr);
            uint32_t next_stage = stage < 14 ? ptr + 2 + (uint32_t)(int16_t)rd16(ptr + 2) : 0;
            uint32_t at = table + (uint32_t)section * 4, next = at + 8;
            int limit = -(int16_t)rd16(at);
            /* Round 8's rooms are one screen each, the camera held at 96 section after section:
             * the limit repeats, and the room ends there too. */
            bool end = (next_stage && next >= next_stage) || -(int16_t)rd16(next) <= limit;
            if (end) after = cam + extra - limit;
            /* A room whose camera never left its limit (round 8's) starts there too: its
             * map is the one screen. */
            if (end && limit < 320 && cam == limit && section > 0 && -(int16_t)rd16(at - 8) == limit) before = extra - (cam - limit);
        }
        /* Still strips: as in Streets of Rage 3 (the truck's inside, plane A holding still). */
        vdp_opts.still_planes = round;
        vdp_opts.still_cam = cam;
        slide_view(before, after);
    }
    if (GAME_SOR != 1 && !GAME_WIDESCREEN) {
        /* Streets of Rage 2 and 3: the sides show only with a player in the game. The intro
         * and the other story scenes are drawn for 320 pixels, with what comes next waiting
         * beside them (Mr. X's hands in 2's). A player slot is in use from its first state
         * except the ones for "not in" ($34/$36 in 2, and $32 its round title card, where $30 is a
         * swing; $5A-$5E in 3, and only in play mode, $0E). */
        bool playing = false;
        for (int k = 0; k < 2; k++) {
            uint16_t s = rd16(GAME_SOR == 3 ? 0xFFDF00 + k * 0x100 : 0xFFEF00 + k * 0x100);
            playing |= GAME_SOR == 3 ? s && s != 0x5A && s != 0x5C && s != 0x5E : s && s != 0x32 && s != 0x34 && s != 0x36;
        }
        if (GAME_SOR == 3 && rd8(0xFFFB01) != 0x0E) playing = false;
        if (vdp_screen_width() == 256) playing = false;    /* a scene drawn 256 wide (H32) */
        if (GAME_SOR == 2 && (rd8(0xFFFC12) & 0x20)) playing = false;   /* the bonus tally */
        vdp_opts.mask_sides = GAME_SOR >= 2 && !playing;
        return;
    }
    if (!GAME_WIDESCREEN) {
        /* Streets of Rage: the sides show the street only during a game, not the demo, and
         * not on the round's title (mode $28/$2A: its words slide in from past the original
         * edges), the elevator of round 7 or round 8 (rooms drawn for 320 pixels, with the
         * city or the next room beside them). The round is $FFFF02, from 0. */
        uint8_t mode = rd8(0xFFFF01);
        bool game = (rd8(0xFFFF18) & 3) && !rd8(0xFFFF2A);
        /* The title's words are still on their way out for about 70 frames after play begins:
         * "UND" and "STA" stood in the sides. */
        static int since_title = 1000;
        if (mode == 0x28 || mode == 0x2A) since_title = 0;
        else if (since_title < 1000) since_title++;
        vdp_opts.mask_sides = !game || since_title < 80 || rd16(0xFFFF02) >= 6;
        /* Behind black sides the players keep to the original 320 pixels. */
        patch_sor_reach = vdp_opts.mask_sides ? 0 : patch_sor_wide;
        /* The HUD is the window plane, 320 pixels wide: the street goes on beside it. */
        vdp_opts.window_narrow = true;
        /* No streaming of our own: the game keeps the 512-pixel planes filled from 96
         * pixels left of the camera to 416 right of it (sub_018B3C, from the street maps
         * at $FF0000 and $FF2000), which covers the widescreen sides. */
        return;
    }
    vdp_opts.mask_sides = !((rd8(0xFF1716 + 0x27) | rd8(0xFF173E + 0x27)) & 1);
}

void widescreen_patches(int extra_width) {
    if (!GAME_WIDESCREEN) {
        /* Streets of Rage: draw sprite pieces in the side areas too (32 pixels past them
         * for pieces partly in view), up to hardware X $1FF, past which X wraps around. */
        int lo = 0x60 - extra_width, hi = 0x1E0 + extra_width;
        patch_sor_x_lo = (uint32_t)(lo < 0 ? 0 : lo);
        patch_sor_x_hi = (uint32_t)(hi > 0x200 ? 0x200 : hi);
        /* The players walk to the new edges, enemies walk in from beyond them and count as
         * on screen across the whole width. */
        patch_sor_wide = patch_sor_reach = patch_sor_reach_right = patch_sor_reach_left = extra_width;
        patch_sor_cull_left = patch_sor_cull_right = extra_width;
        return;
    }
    pad = (extra_width + 7) / 8;
    if (pad > 11) pad = 11;
    bool on = extra_width > 0;
    /* Sprite culling: keep pieces in the side areas. The right side stops at hardware
     * X = $1FF (screen 383); past that the 9-bit X would wrap to the left edge. */
    int right = extra_width < 63 ? extra_width : 63;
    patch_cull_left = (uint32_t)extra_width;
    patch_cam_margin = extra_width;
    patch_cull_widen = (uint32_t)(extra_width + right);
    machine_override(0x0023CC, on ? scroll_column : NULL);
    machine_override(0x002512, on ? scroll_row : NULL);
    machine_override(0x00D7C0, on ? init_layer : NULL);
    machine_override(0x00D950, on ? clear_plane_at : NULL);
    machine_override(0x00DC26, on ? redraw_rect : NULL);
}

/* Streets of Rage: the level table places an object (sorWide.tableSpawn), type byte and world
 * X. Most stand where the level puts them (scenery, enemies waiting in doorways), but an enemy
 * (types below $40) put just past an edge of the original 320 pixels, to walk in, would show
 * in the widescreen sides: it goes as far past the new edge. */
uint32_t sor_table_x(uint32_t type, uint32_t x) {
    int w = patch_sor_wide, sx = (int16_t)x - (int16_t)rd16(0xFFE002);
    if (!w || type >= 0x40) return x;
    if (sx > 320 && sx <= 320 + w + 64) return (x + (uint32_t)w) & 0xFFFF;
    if (sx < 0 && sx >= -w - 64) return (x - (uint32_t)w) & 0xFFFF;
    return x;
}
