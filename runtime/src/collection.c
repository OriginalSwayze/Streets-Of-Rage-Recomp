/* Streets of Rage Collection: the front end. Three cards, one per game; Play starts that
 * game's own program (games/<name>/<exe>, with its themed launcher, settings and saves in
 * its folder) and the collection comes back when it closes. A game that is not installed
 * (Setup skipped its ROM) shows as such.
 *
 * Each game writes card.png (its cover, from its ROM) next to its program when its
 * launcher captures its art; until then its card is drawn from text alone. */
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "png.h"
#include "ui.h"
/* The collection is not one game: its own night-street colours, then the neon drawing
   (theme.h, which is per game, is skipped). */
#define THEME_H
#define C_INK  rgb_hex(0x0A0C1A)
#define C_LINE rgb_hex(0x262B52)
#define C_TEXT rgb_hex(0xE6E8F4)
#define C_DIM  rgb_hex(0x7178A0)
#define FONT_SIGN  "Bahnschrift SemiBold Condensed"
#define FONT_UI    "Bahnschrift"
#define FONT_VALUE "Bahnschrift SemiBold"
#include "neon.h"

typedef struct {
    const char *dir, *exe, *title, *number, *tag, *year;
    uint32_t accent, deep;              /* each game's colours, for its card */
    bool installed;
    uint32_t *card;                     /* card.png, or NULL */
    int card_w, card_h;
} Game;

static Game games[3] = {
    { "sor1", "StreetsOfRage.exe", "STREETS OF RAGE", "", "Axel, Adam and Blaze take back the city.", "1991",
      0xFF4FB0, 0x4A0A2A, false, NULL, 0, 0 },
    { "sor2", "StreetsOfRage2.exe", "STREETS OF RAGE", "2", "Max and Skate join the fight to rescue Adam.", "1992",
      0x5CD8FF, 0x0A2A50, false, NULL, 0, 0 },
    { "sor3", "StreetsOfRage3.exe", "STREETS OF RAGE", "3", "Zan and the robots of Mr. X's syndicate.", "1994",
      0xFFA23C, 0x3A1406, false, NULL, 0, 0 },
};
#define GAMES 3

static char base[1024];
static int selected, hover = -1;
static float hl_x;                      /* the highlight, easing to the selection */
static char status[160];
static Uint64 status_until;

static void say(const char *s) {
    snprintf(status, sizeof status, "%s", s);
    status_until = SDL_GetTicks64() + 4000;
}

static void find_games(void) {
    for (int g = 0; g < GAMES; g++) {
        char p[1200];
        snprintf(p, sizeof p, "%sgames\\%s\\%s", base, games[g].dir, games[g].exe);
        games[g].installed = GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
        free(games[g].card);
        games[g].card = NULL;
        snprintf(p, sizeof p, "%sgames\\%s\\card.png", base, games[g].dir);
        games[g].card = png_read(p, &games[g].card_w, &games[g].card_h);
    }
}

/* ---- Drawing --------------------------------------------------------------------- */
#define CARD_W 340
#define CARD_H 470
#define CARD_Y 150
#define GAP    40
static float card_x(int g) { return (UI_BASE_W - (GAMES * CARD_W + (GAMES - 1) * GAP)) / 2.f + g * (CARD_W + GAP); }



/* An image scaled to cover the box (cropping its sides), in layout units. */
static void draw_image(const uint32_t *img, int iw, int ih, float x, float y, float w, float h, float bright) {
    int px0 = (int)ui_x(x), py0 = (int)ui_y(y), px1 = (int)ui_x(x + w), py1 = (int)ui_y(y + h);
    float k = fmaxf((float)(px1 - px0) / iw, (float)(py1 - py0) / ih);
    float sx0 = (iw - (px1 - px0) / k) / 2;
    for (int py = py0 < 0 ? 0 : py0; py < py1 && py < ui_h; py++) {
        int sy = (int)((py - py0) / k);
        if (sy >= ih) sy = ih - 1;
        for (int px = px0 < 0 ? 0 : px0; px < px1 && px < ui_w; px++) {
            int sx = (int)(sx0 + (px - px0) / k);
            if (sx < 0 || sx >= iw) continue;
            uint32_t c = img[sy * iw + sx];
            Rgb v = scale(unpack(c & 0xFFFFFF), bright);
            ui_px[py * ui_w + px] = pack(v);
        }
    }
}

static void draw_card(int g, float t) {
    const Game *gm = &games[g];
    bool on = g == selected;
    float x = card_x(g), y = CARD_Y - (on ? 10 : 0);
    Rgb accent = rgb_hex(gm->accent), deep = rgb_hex(gm->deep);
    ui_fill_rect(x, y, CARD_W, CARD_H, mix(deep, C_INK, .7f), .95f);
    neon_tube(x, y, CARD_W, CARD_H, accent, on ? .9f * neon_flicker(t, g) : .28f);
    /* The cover, or the game's colours. */
    float ix = x + 14, iy = y + 14, iw = CARD_W - 28, ih = 250;
    if (gm->card) draw_image(gm->card, gm->card_w, gm->card_h, ix, iy, iw, ih, gm->installed ? (on ? 1 : .7f) : .3f);
    else {
        for (int k = 0; k < 10; k++)
            ui_fill_rect(ix, iy + k * ih / 10, iw, ih / 10 + 1, mix(deep, rgb_hex(0x000000), k / 12.f), 1);
        float pulse = on ? .5f + .5f * sinf(t * 2) : 0;
        ui_draw_text(gm->number[0] ? gm->number : "I", "Impact", 170, 400, 0, ix + iw / 2 - ui_text_width(gm->number[0] ? gm->number : "I", "Impact", 170, 400, 0) / 2,
                     iy + 20, mix(accent, rgb_hex(0xFFFFFF), pulse * .3f), scale(accent, .55f), gm->installed ? .85f : .3f);
    }
    neon_rect_frame(ix, iy, iw, ih, 1.5f, accent, on ? .7f : .25f);
    /* Title and number, lit on the card chosen. */
    float ty = iy + ih + 22, fade = gm->installed ? 1 : .45f;
    float tw = ui_text_width(gm->title, "Impact", 34, 400, 2);
    if (on && gm->installed) {
        neon_text(gm->title, "Impact", 34, 400, 2, x + 22, ty, rgb_hex(0xF2F0FF), 1);
        if (gm->number[0]) neon_text(gm->number, "Impact", 44, 400, 0, x + 30 + tw, ty - 9, accent, 1);
    } else {
        ui_draw_text(gm->title, "Impact", 34, 400, 2, x + 22, ty, C_TEXT, C_DIM, fade * .8f);
        if (gm->number[0]) ui_draw_text(gm->number, "Impact", 44, 400, 0, x + 30 + tw, ty - 9, accent, scale(accent, .7f), fade * .8f);
    }
    neon_bar(x + 22, ty + 46, 60, 3, accent, on ? 1 : .4f);
    ui_draw_text(gm->tag, FONT_UI, 16, 400, 0, x + 22, ty + 60, C_TEXT, C_TEXT, gm->installed ? .9f : .45f);
    const char *state = gm->installed ? (on ? "PLAY" : gm->year) : "NOT INSTALLED";
    float bx = x + 22, by = y + CARD_H - 62, bw = CARD_W - 44, bh = 42;
    float sw = ui_text_width(state, FONT_SIGN, 26, 600, 5);
    if (gm->installed && on) {
        ui_fill_rect(bx, by, bw, bh, accent, .08f);
        neon_tube(bx, by, bw, bh, accent, .9f);
        neon_text(state, FONT_SIGN, 26, 600, 5, x + CARD_W / 2 - sw / 2, by + 5, accent, 1);
    } else {
        Rgb sc = gm->installed ? C_DIM : rgb_hex(0xFF5A6E);
        ui_draw_text(state, FONT_SIGN, 26, 600, 5, x + CARD_W / 2 - sw / 2, by + 5, sc, sc, 1);
    }
    if (!gm->installed && on)
        ui_draw_text("Run Setup again with its ROM to add it.", FONT_UI, 14, 400, 0, x + 22, y + CARD_H - 92, C_DIM, C_DIM, 1);
}

static void render(int w, int h, float t) {
    ui_begin(w, h);
    for (int y = 0; y < ui_h; y++) {    /* night sky to street */
        Rgb c = mix(rgb_hex(0x10122A), rgb_hex(0x05060C), (float)y / ui_h);
        uint32_t v = pack(c);
        for (int x = 0; x < ui_w; x++) ui_px[y * ui_w + x] = v;
    }
    /* The selected game's colour glows behind its card. */
    float target = card_x(selected) + CARD_W / 2.f;
    if (hl_x == 0) hl_x = target;
    Rgb glow = rgb_hex(games[selected].accent);
    for (int k = 0; k < 24; k++) {
        float r = 360 - k * 14;
        ui_panel(hl_x - r, CARD_Y + CARD_H / 2.f - r * .7f, r * 2, r * 1.4f, r * .7f, glow, glow, .012f, glow, 0);
    }
    neon_text("STREETS OF RAGE", "Impact", 58, 400, 6, 70, 38, rgb_hex(0xFF4FB0), neon_flicker(t, 9));
    neon_bar(72, 108, 200, 4, rgb_hex(0x5CD8FF), 1);
    neon_text("COLLECTION", FONT_SIGN, 30, 600, 14, 290, 94, rgb_hex(0x5CD8FF), 1);
    for (int g = 0; g < GAMES; g++) draw_card(g, t);
    const char *help = "\xE2\x86\x90 \xE2\x86\x92  choose      Enter  play      Esc  quit";
    ui_draw_text(help, "Segoe UI Semibold", 15, 600, 1, UI_BASE_W / 2.f - ui_text_width(help, "Segoe UI Semibold", 15, 600, 1) / 2, 668,
                 C_DIM, C_DIM, 1);
    if (status[0] && SDL_GetTicks64() < status_until)
        ui_draw_text(status, FONT_VALUE, 16, 600, 0, 70, 640, rgb_hex(0xFF4FB0), rgb_hex(0xFF4FB0), 1);
}

/* ---- Playing ------------------------------------------------------------------------ */
static Uint64 back_at;
static bool armed = true;               /* input counts (false just after a game ends) */

/* Returns false when the game's window was closed (its X): then the collection closes too. */
static bool play(SDL_Window *win, int g) {
    if (!games[g].installed) { say("That game is not installed: run Setup again and pick its ROM."); return true; }
    char exe[1200], dir[1200], cmd[1400];
    snprintf(dir, sizeof dir, "%sgames\\%s", base, games[g].dir);
    snprintf(exe, sizeof exe, "%s\\%s", dir, games[g].exe);
    snprintf(cmd, sizeof cmd, "\"%s\" --collection", exe);
    STARTUPINFOA si = { .cb = sizeof si };
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(exe, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) { say("Could not start the game."); return true; }
    SDL_HideWindow(win);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    /* Back to the collection only when the player chose to (Back, Quit: exit code 0). The
     * window's X (EXIT_WINDOW_CLOSED, launcher.h), ending the game in Task Manager (1) or a
     * crash close the collection too: it coming back looked like the launcher reopening
     * by itself, however often it was ended (issue #9). */
    if (code != 0) return false;
    find_games();                       /* it may have made its card */
    SDL_ShowWindow(win);
    SDL_RaiseWindow(win);
    /* Whatever was pressed to leave the game (A or Start on the pad, Enter, the click that
     * closed it or ended it in Task Manager) is still down, or waiting in the queue: it
     * would start a game again at once. Nothing counts until everything is let go. */
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    back_at = SDL_GetTicks64();
    armed = false;
    return true;
}

/* Back from a game: input counts again once every key, button and mouse button is up, and
 * a moment has passed. */
static bool all_released(SDL_GameController *pad) {
    int n = 0;
    const Uint8 *keys = SDL_GetKeyboardState(&n);
    for (int k = 0; k < n; k++) if (keys[k]) return false;
    if (SDL_GetMouseState(NULL, NULL)) return false;
    for (int b = 0; pad && b < SDL_CONTROLLER_BUTTON_MAX; b++)
        if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)b)) return false;
    return SDL_GetTicks64() - back_at > 300;
}

int main(int argc, char **argv) {
    char *bp = SDL_GetBasePath();
    snprintf(base, sizeof base, "%s", bp ? bp : "");
    if (bp) SDL_free(bp);
    const char *shot = NULL;            /* --shot F.png: render one frame and save it (tests) */
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
    find_games();
    for (int g = 0; g < GAMES; g++) if (games[g].installed) { selected = g; break; }
    if (shot) {
        hl_x = card_x(selected) + CARD_W / 2.f;
        render(1280, 720, 1);
        return png_write(shot, ui_px, ui_w, ui_h, ui_w) ? 1 : 0;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) return 1;
    SDL_Window *win = SDL_CreateWindow("Streets of Rage Collection", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *tex = NULL;
    int tw = 0, th = 0;
    SDL_GameController *pad = NULL;
    bool running = true;
    Uint64 t0 = SDL_GetPerformanceCounter(), last = t0;
    while (running) {
        SDL_Event e;
        if (!armed) armed = all_released(pad);
        while (SDL_PollEvent(&e)) {
            int move = 0;
            bool go = false;
            if (!armed && e.type != SDL_QUIT && e.type != SDL_CONTROLLERDEVICEADDED) continue;
            switch (e.type) {
            case SDL_QUIT: running = false; break;
            case SDL_CONTROLLERDEVICEADDED: if (!pad) pad = SDL_GameControllerOpen(e.cdevice.which); break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
                else if (e.key.keysym.sym == SDLK_LEFT || e.key.keysym.sym == SDLK_a) move = -1;
                else if (e.key.keysym.sym == SDLK_RIGHT || e.key.keysym.sym == SDLK_d) move = 1;
                else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) go = true;
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) move = -1;
                else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) move = 1;
                else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_A || e.cbutton.button == SDL_CONTROLLER_BUTTON_START) go = true;
                else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) running = false;
                break;
            case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: {
                int ww, wh, mx = e.type == SDL_MOUSEMOTION ? e.motion.x : e.button.x, my = e.type == SDL_MOUSEMOTION ? e.motion.y : e.button.y;
                SDL_GetWindowSize(win, &ww, &wh);
                float lx = (mx * (float)tw / ww - ui_ox) / ui_s, ly = (my * (float)th / wh - ui_oy) / ui_s;
                hover = -1;
                for (int g = 0; g < GAMES; g++)
                    if (lx >= card_x(g) && lx < card_x(g) + CARD_W && ly >= CARD_Y - 10 && ly < CARD_Y + CARD_H) hover = g;
                if (hover >= 0) {
                    if (e.type == SDL_MOUSEBUTTONDOWN && hover == selected) go = true;
                    selected = hover;
                }
                break;
            }
            }
            if (move) selected = (selected + move + GAMES) % GAMES;
            if (go && !play(win, selected)) running = false;
        }
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - last) / SDL_GetPerformanceFrequency(), t = (float)(now - t0) / SDL_GetPerformanceFrequency();
        last = now;
        float target = card_x(selected) + CARD_W / 2.f;
        hl_x += (target - hl_x) * (1 - expf(-dt * 12));
        int w, h;
        SDL_GetRendererOutputSize(ren, &w, &h);
        render(w, h, t);
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
    return 0;
}
