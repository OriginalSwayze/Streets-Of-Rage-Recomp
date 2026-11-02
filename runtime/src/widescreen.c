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
#include "genesis.h"
#include "recomp_rt.h"

#define VDP_DATA  0xC00000
#define VDP_CTRL  0xC00004
#define BLANK_CELL 0x0780               /* tile entry sub_00D962 fills empty planes with */

static int pad;                         /* extra columns kept valid on each side */
uint32_t patch_cull_left, patch_cull_widen;
uint32_t patch_sor_x_lo = 0x60, patch_sor_x_hi = 0x1E0;
int patch_sor_wide, patch_sor_reach;
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

/* The extra columns only show real content while a map layer is active (gameplay and
 * in-engine scenes). Logos, the title, menus and story scenes are drawn for 320 pixels,
 * so their sides are masked to black. */
void widescreen_frame(void) {
    vdp_opts.mask_left = 0;
    if (GAME_SOR == 3) {
        /* Streets of Rage 3: a round's camera ($FFF710) starts at the level's left end, X 0;
         * left of it the planes hold nothing of the level, so that part of the left side is
         * black until the camera has moved past the widescreen columns. */
        uint16_t s = rd16(0xFFDF00);
        bool round = s && s != 0x5C && s != 0x5E && s != 0x5A && !rd8(0xFFFB13);
        int cam = (int16_t)rd16(0xFFF710);
        if (round && cam < vdp_opts.extra_width) vdp_opts.mask_left = vdp_opts.extra_width - (cam > 0 ? cam : 0);
    }
    if (GAME_SOR != 1 && !GAME_WIDESCREEN) { vdp_opts.mask_sides = false; return; }
    if (!GAME_WIDESCREEN) {
        /* Streets of Rage: the sides show the street only during a game, not the demo, and
         * not on the round's title (mode $28/$2A: its words slide in from past the original
         * edges), the elevator of round 7 or round 8 (rooms drawn for 320 pixels, with the
         * city or the next room beside them). The round is $FFFF02, from 0. */
        uint8_t mode = rd8(0xFFFF01);
        bool game = (rd8(0xFFFF18) & 3) && !rd8(0xFFFF2A);
        vdp_opts.mask_sides = !game || mode == 0x28 || mode == 0x2A || rd16(0xFFFF02) >= 6;
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
        patch_sor_wide = patch_sor_reach = extra_width;
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
