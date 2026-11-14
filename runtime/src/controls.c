/* Controls page: one row per action with its keyboard key and controller button.
 * Enter (or A, or a click) on a row waits for the next key or controller button, which
 * becomes that action's key or button; Esc cancels. Navigation itself always uses the
 * arrows / D-pad, Enter / A and Esc / B, so a binding can never lock you out.
 *
 * Tabs at the top pick whose bindings these are: player 1's, player 2's (#33) and, with a
 * third player on, player 3's. Left / Right, LB / RB or a click switch them. Players 2
 * and 3 have the pad's actions only: rewind, the menu and fast-forward are player 1's. */
#include "controls.h"

#include <stdio.h>

#include "input.h"
#include "settings.h"
#include "theme.h"
#include "neon.h"
#include "ui.h"

#define PANEL_X 280
#define PANEL_Y 52
#define PANEL_W 720
#define PANEL_H 616
#define LIST_X  (PANEL_X + 44)
#define LIST_R  (PANEL_X + PANEL_W - 44)
#define KEY_X   (PANEL_X + 330)
#define PAD_X   (PANEL_X + 510)
#define LIST_Y  (PANEL_Y + 128)
#define ROW_H   (IN_COUNT > 11 ? 26 : 33)   /* Streets of Rage 3's X, Y and Z rows too */
#define TEXT_Y  (IN_COUNT > 11 ? 3 : 7)
#define TAB_Y   (PANEL_Y + 34)
#define TAB_H   30
#define TAB_PX  15

static bool open_, capturing;
static int selected, player;
static double now, captured_until;      /* "Saved" flash on the row just bound */

static int players(void) { return settings.third_player ? 3 : 2; }
static int actions(void) { return player ? IN_PAD_COUNT : IN_COUNT; }
static int row_reset(void) { return actions(); }
static int row_back(void) { return actions() + 1; }
static int rows(void) { return actions() + 2; }

void controls_open(void) { open_ = true; capturing = false; selected = 0; player = 0; }
bool controls_is_open(void) { return open_; }
void controls_show_player(int p) { player = p > 0 && p < players() ? p : 0; selected = 0; }

static float row_y(int i) { return LIST_Y + i * ROW_H + (i >= row_reset() ? 14 : 0); }

static int hit_test(float mx, float my) {
    for (int i = 0; i < rows(); i++) {
        float y = row_y(i);
        if (mx >= LIST_X - 14 && mx <= LIST_R + 10 && my >= y && my < y + ROW_H) return i;
    }
    return -1;
}

/* The tabs, right-aligned in the heading's row: each one's left edge and width. */
static const char *tab_label(int p) { static const char *const l[] = { "PLAYER 1", "PLAYER 2", "PLAYER 3" }; return l[p]; }
static void tab_box(int p, float *x, float *w) {
    float right = LIST_R + 10;
    for (int k = players() - 1; k >= 0; k--) {
        float tw = ui_text_width(tab_label(k), FONT_SIGN, TAB_PX, 600, 3) + 28;
        right -= tw;
        if (k == p) { *x = right; *w = tw; return; }
        right -= 8;
    }
}

static int tab_hit(float mx, float my) {
    if (my < TAB_Y || my >= TAB_Y + TAB_H) return -1;
    for (int p = 0; p < players(); p++) {
        float x, w;
        tab_box(p, &x, &w);
        if (mx >= x && mx < x + w) return p;
    }
    return -1;
}

static void switch_player(int p) {
    int n = players();
    p = (p + n) % n;
    if (p == player) return;
    bool on_back = selected == row_back(), on_reset = selected == row_reset();
    player = p;
    /* The same row in the other player's list: Reset and Back move with the list's end. */
    if (on_back) selected = row_back();
    else if (on_reset || selected >= actions()) selected = row_reset();
}

static bool activate(void) {
    if (selected == row_back()) { open_ = false; return false; }
    if (selected == row_reset()) { input_player_defaults(player); captured_until = now + 1; return true; }
    capturing = true;
    return false;
}

bool controls_event(const SDL_Event *e, float mx, float my) {
    if (!open_) return false;
    if (player >= players()) player = 0;
    if (capturing) {
        /* The next key or button is the new binding; Esc (or the Guide button) cancels. */
        if (e->type == SDL_KEYDOWN && !e->key.repeat) {
            capturing = false;
            if (e->key.keysym.scancode == SDL_SCANCODE_ESCAPE) return false;
            input_bind_key(player, selected, e->key.keysym.scancode);
            captured_until = now + 1;
            return true;
        }
        if (e->type == SDL_CONTROLLERBUTTONDOWN) {
            capturing = false;
            if (e->cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) return false;
            input_bind_button(player, selected, e->cbutton.button);
            captured_until = now + 1;
            return true;
        }
        int trigger = input_trigger_pressed(e);   /* a pad's C or Z may be a trigger (#31) */
        if (trigger >= 0) {
            capturing = false;
            input_bind_button(player, selected, trigger);
            captured_until = now + 1;
            return true;
        }
        if (e->type == SDL_MOUSEBUTTONDOWN) capturing = false;
        return false;
    }
    switch (e->type) {
    case SDL_KEYDOWN:
        switch (e->key.keysym.sym) {
        case SDLK_ESCAPE: case SDLK_BACKSPACE: open_ = false; break;
        case SDLK_UP: selected = (selected + rows() - 1) % rows(); break;
        case SDLK_DOWN: case SDLK_TAB: selected = (selected + 1) % rows(); break;
        case SDLK_LEFT: switch_player(player - 1); break;
        case SDLK_RIGHT: switch_player(player + 1); break;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: return activate();
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        switch (e->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK: open_ = false; break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: selected = (selected + rows() - 1) % rows(); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: selected = (selected + 1) % rows(); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: switch_player(player - 1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: switch_player(player + 1); break;
        case SDL_CONTROLLER_BUTTON_A: return activate();
        }
        break;
    case SDL_MOUSEMOTION: {
        int h = hit_test(mx, my);
        if (h >= 0) selected = h;
        break;
    }
    case SDL_MOUSEBUTTONDOWN: {
        int t = tab_hit(mx, my);
        if (t >= 0 && e->button.button == SDL_BUTTON_LEFT) { switch_player(t); break; }
        int h = hit_test(mx, my);
        if (h >= 0 && e->button.button == SDL_BUTTON_LEFT) { selected = h; return activate(); }
        if (e->button.button == SDL_BUTTON_RIGHT) open_ = false;
        break;
    }
    }
    return false;
}

void controls_render(double time_s) {
    now = time_s;
    if (!open_) return;
    if (player >= players()) player = 0;
    ui_fill_rect(0, 0, UI_BASE_W, UI_BASE_H, rgb(0, 0, 0), .55f);
    /* Navy glass in a neon tube, the heading lit like a sign. */
    Rgb yellow = C_YELLOW, red = C_RED;
    ui_fill_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, C_INK, .95f);
    neon_tube(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, red, .75f);
    neon_text("CONTROLS", "Impact", 42, 400, 6, LIST_X - 1, PANEL_Y + 22, yellow, neon_flicker((float)time_s, 6));
    neon_bar(LIST_X, PANEL_Y + 76, 110, 3, red, 1);
    /* Whose bindings: the shown player's tab lit, the others dim. */
    for (int p = 0; p < players(); p++) {
        float x, w;
        tab_box(p, &x, &w);
        bool on = p == player;
        if (on) {
            ui_fill_rect(x, TAB_Y, w, TAB_H, red, .1f);
            neon_tube(x, TAB_Y, w, TAB_H, red, .85f);
        }
        Rgb tc = on ? yellow : C_DIM;
        ui_draw_text(tab_label(p), FONT_SIGN, TAB_PX, 600, 3, x + 14, TAB_Y + 6, tc, tc, 1);
    }
    Rgb head = red;
    ui_draw_text("KEYBOARD", FONT_SIGN, 15, 600, 4, KEY_X, LIST_Y - 28, head, head, 1);
    ui_draw_text("CONTROLLER", FONT_SIGN, 15, 600, 4, PAD_X, LIST_Y - 28, head, head, 1);

    for (int i = 0; i < rows(); i++) {
        float y = row_y(i);
        bool focus = selected == i;
        if (focus) {
            ui_fill_rect(LIST_X - 14, y + 1, LIST_R - LIST_X + 24, ROW_H - 2, red, .08f);
            neon_tube(LIST_X - 14, y + 1, LIST_R - LIST_X + 24, ROW_H - 2, red, .8f);
        }
        Rgb lc = focus ? rgb_hex(0xFFFFFF) : C_TEXT;
        if (i == row_reset()) { ui_draw_text("Reset to defaults", FONT_UI, 17, 400, 0, LIST_X, y + TEXT_Y, lc, lc, 1); continue; }
        if (i == row_back()) { ui_draw_text("Back", FONT_UI, 17, 400, 0, LIST_X, y + TEXT_Y, lc, lc, 1); continue; }
        ui_draw_text(input_labels[i], FONT_UI, 17, 400, 0, LIST_X, y + TEXT_Y, lc, lc, 1);
        if (focus && capturing) {
            /* Blinking prompt in place of the two bindings. */
            float a = .55f + .45f * (float)((int)(now * 2.5) & 1);
            ui_draw_text("Press a key or a controller button\xE2\x80\xA6   Esc cancels", FONT_VALUE, 15, 600, 0,
                         KEY_X, y + TEXT_Y + 2, rgb_hex(0xF3DDA4), rgb_hex(0xF3DDA4), a);
            continue;
        }
        Rgb vc = focus ? yellow : C_VALUE;
        if (focus && now < captured_until) vc = rgb_hex(0x8FD19E);
        const char *key = input_key_name(player, i);
        Rgb kc = key[0] == '(' && !focus ? C_DIM : vc;   /* "(none)": no key yet */
        ui_draw_text(key, FONT_VALUE, 16.5f, 600, 0, KEY_X, y + TEXT_Y, kc, kc, 1);
        ui_draw_text(input_button_name(player, i), FONT_VALUE, 16.5f, 600, 0, PAD_X, y + TEXT_Y, vc, vc, 1);
    }
    const char *keys = player
        ? "\xE2\x86\x91\xE2\x86\x93 select     \xE2\x86\x90\xE2\x86\x92 player     Enter  change     Esc  back     "
          "Buttons are for this player's own controller"
        : "\xE2\x86\x91\xE2\x86\x93 select     \xE2\x86\x90\xE2\x86\x92 player     Enter  change     Esc  back     "
          "Esc, F-keys and the menus' own keys always work";
    ui_draw_text(keys, "Segoe UI", 12, 400, 1, LIST_X, PANEL_Y + PANEL_H - 38, C_DIM, C_DIM, 1);
}
