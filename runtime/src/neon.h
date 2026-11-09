/* The night-street look shared by the launcher, the pause menu and its pages: dark navy
 * panels framed by neon tubes, sign lettering that glows, and the odd flicker of a sign
 * on its way out. Everything draws with ui.h in layout units. */
#ifndef NEON_H
#define NEON_H

#include <math.h>

#include "theme.h"
#include "ui.h"

/* A frame of `thick` units inside (x, y, w, h). */
static inline void neon_rect_frame(float x, float y, float w, float h, float thick, Rgb c, float a) {
    ui_fill_rect(x, y, w, thick, c, a);
    ui_fill_rect(x, y + h - thick, w, thick, c, a);
    ui_fill_rect(x, y + thick, thick, h - 2 * thick, c, a);
    ui_fill_rect(x + w - thick, y + thick, thick, h - 2 * thick, c, a);
}

/* A neon tube around (x, y, w, h): a soft halo outside, the coloured glass, and a hot white
 * core along it. `a` dims the whole tube (0 off, 1 lit). */
static inline void neon_tube(float x, float y, float w, float h, Rgb c, float a) {
    static const float spread[4] = { 7, 5, 3, 1.5f }, halo[4] = { .05f, .085f, .14f, .24f };
    for (int k = 0; k < 4; k++) neon_rect_frame(x - spread[k], y - spread[k], w + 2 * spread[k], h + 2 * spread[k], spread[k] < 2 ? 1.5f : 2, c, halo[k] * a);
    neon_rect_frame(x, y, w, h, 2.5f, c, .95f * a);
    neon_rect_frame(x + .75f, y + .75f, w - 1.5f, h - 1.5f, 1, mix(c, rgb(1, 1, 1), .7f), .55f * a);
}

/* A straight tube from (x, y), `w` long and `h` thick (a rule under a heading, a bar). */
static inline void neon_bar(float x, float y, float w, float h, Rgb c, float a) {
    ui_fill_rect(x - 4, y - 4, w + 8, h + 8, c, .06f * a);
    ui_fill_rect(x - 2, y - 2, w + 4, h + 4, c, .12f * a);
    ui_fill_rect(x, y, w, h, c, .95f * a);
    if (h >= 2) ui_fill_rect(x + 1, y + h / 2 - .5f, w - 2, 1, mix(c, rgb(1, 1, 1), .7f), .6f * a);
}

/* Sign lettering: a glow around the letters in `c`, then the letters, whiter at the core. */
static inline void neon_text(const char *s, const char *face, float px, int weight, float tracking, float x, float y, Rgb c, float a) {
    float r = px / 15;
    for (int k = 0; k < 8; k++) {
        float ang = k * .7853982f;
        ui_draw_text(s, face, px, weight, tracking, x + cosf(ang) * r * 2.2f, y + sinf(ang) * r * 2.2f, c, c, .09f * a);
        ui_draw_text(s, face, px, weight, tracking, x + cosf(ang) * r, y + sinf(ang) * r, c, c, .18f * a);
    }
    Rgb core = mix(c, rgb(1, 1, 1), .45f);
    ui_draw_text(s, face, px, weight, tracking, x, y, core, c, a);
}

/* The panel behind a group of controls: deep navy glass. */
static inline void neon_panel(float x, float y, float w, float h, float a) {
    ui_fill_rect(x, y, w, h, C_INK, .9f * a);
    neon_rect_frame(x, y, w, h, 1, C_LINE, .9f * a);
}

/* A sign's brightness at time t: steady, with now and then a stutter (each `seed` its own). */
static inline float neon_flicker(float t, int seed) {
    float p = fmodf(t * .37f + seed * .618f, 9.0f);   /* once in about 24 s */
    if (p > .35f) return 1;
    return sinf(p * 90) > 0 ? 1 : .35f;
}

#endif
