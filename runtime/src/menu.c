/* In-game menu: pauses the game over a blurred, darkened copy of the last frame, on the
 * launcher's stone tablet with one tab per spirit (GAME, DISPLAY, EFFECTS, MORE). The card
 * on the right follows the selected row: the save slot's thumbnail, a live before/after
 * preview of a picture option on the paused frame (preview.c), or what the option does.
 * Rows glide and slide like the launcher's, embers drift over the game, and every choice
 * makes the game's own menu sound (played by main.c through menu_sound). */
#include "menu.h"
#include "fighters.h"
#include "controls.h"
#include "outfit.h"
#include "preview.h"
#include "launcher_scene.h"
#include "foil.h"
#include "combat.h"
#include "automap.h"
#include "genesis.h"
#include "savestate.h"
#include "settings.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { ITEM_ACTION, ITEM_SLOT, ITEM_OPTION } ItemKind;
enum { ACT_RESUME, ACT_SAVE, ACT_LOAD, ACT_RESET, ACT_QUIT, ACT_CONTROLS, ACT_OUTFIT, ACT_MAP, ACT_STATS,
       ACT_FIGHTER0, ACT_FIGHTER1, ACT_FIGHTER2, ACT_FIGHTER3 };
typedef struct { ItemKind kind; const char *label; int id; } Item;

#define TABS 6
#define MAX_ROWS 10
static const char *const tab_names[TABS] = { "GAME", "DISPLAY", "EFFECTS", "COLOURS", "CHEATS", "MORE" };
static const Item tab_items[TABS][MAX_ROWS] = {
    { { ITEM_ACTION, "Resume", ACT_RESUME }, { ITEM_SLOT, "Save state", ACT_SAVE }, { ITEM_SLOT, "Load state", ACT_LOAD },
      { ITEM_OPTION, "Player 2", OPT_PLAYER2 }, { ITEM_ACTION, "Stats", ACT_STATS },
      { ITEM_OPTION, "Rewind", OPT_REWIND }, { ITEM_OPTION, "Fast-forward", OPT_FAST },
      { ITEM_ACTION, "Reset game", ACT_RESET }, { ITEM_ACTION, "Quit game", ACT_QUIT } },
    { { ITEM_OPTION, "Widescreen", OPT_WIDESCREEN }, { ITEM_OPTION, "Aspect ratio", OPT_ASPECT },
      { ITEM_OPTION, "Integer scaling", OPT_INTEGER }, { ITEM_OPTION, "Internal resolution", OPT_INTERNAL },
      { ITEM_OPTION, "Scaler", OPT_SCALER }, { ITEM_OPTION, "Filter", OPT_FILTER },
      { ITEM_OPTION, "Sharpening", OPT_SHARPEN }, { ITEM_OPTION, "High frame rate", OPT_SMOOTH },
      { ITEM_OPTION, "Sprite flicker fix", OPT_FLICKER }, { ITEM_OPTION, "Slowdown fix", OPT_OVERCLOCK } },
    { { ITEM_OPTION, "Lighting", OPT_LIGHTING }, { ITEM_OPTION, "HDR look", OPT_HDR },
      { ITEM_OPTION, "Combat info", OPT_COMBAT }, { ITEM_OPTION, "Play timer", OPT_TIMER },
      { ITEM_OPTION, "Widescreen sides", OPT_SIDES } },
#if GAME_SOR == 3
    { { ITEM_ACTION, "Axel", ACT_FIGHTER0 }, { ITEM_ACTION, "Blaze", ACT_FIGHTER1 }, { ITEM_ACTION, "Skate", ACT_FIGHTER2 },
      { ITEM_ACTION, "Zan", ACT_FIGHTER3 } },
#elif GAME_SOR == 2
    { { ITEM_ACTION, "Max", ACT_FIGHTER0 }, { ITEM_ACTION, "Axel", ACT_FIGHTER1 }, { ITEM_ACTION, "Blaze", ACT_FIGHTER2 },
      { ITEM_ACTION, "Skate", ACT_FIGHTER3 } },
#else
    { { ITEM_ACTION, "Axel", ACT_FIGHTER0 }, { ITEM_ACTION, "Adam", ACT_FIGHTER1 }, { ITEM_ACTION, "Blaze", ACT_FIGHTER2 } },
#endif
#if GAME_SOR == 3
    { { ITEM_OPTION, "Infinite health", OPT_CHEAT_HEALTH }, { ITEM_OPTION, "Infinite lives", OPT_CHEAT_LIVES },
      { ITEM_OPTION, "Attack power", OPT_CHEAT_ATTACK }, { ITEM_OPTION, "Hidden options", OPT_CHEAT_UNLOCK } },
#else
    { { ITEM_OPTION, "Infinite health", OPT_CHEAT_HEALTH }, { ITEM_OPTION, "Infinite lives", OPT_CHEAT_LIVES },
      { ITEM_OPTION, GAME_SOR == 2 ? "Free specials" : "Infinite specials", OPT_CHEAT_SPECIALS }, { ITEM_OPTION, "Infinite time", OPT_CHEAT_TIME },
      { ITEM_OPTION, "Attack power", OPT_CHEAT_ATTACK }, { ITEM_OPTION, "Hidden options", OPT_CHEAT_UNLOCK } },
#endif
    { { ITEM_OPTION, "Volume", OPT_VOLUME }, { ITEM_OPTION, "Audio", OPT_AUDIO }, { ITEM_OPTION, "Bass", OPT_BASS },
      { ITEM_OPTION, "Music files", OPT_MUSIC_FILES },
      { ITEM_ACTION, "Controls", ACT_CONTROLS }, { ITEM_OPTION, "Rumble", OPT_RUMBLE },
      { ITEM_OPTION, "Texture pack", OPT_TEXPACK }, { ITEM_OPTION, "Dump tiles", OPT_DUMP },
#if GAME_EDITION_COUNT > 1
      { ITEM_OPTION, "Edition", OPT_EDITION },
#endif
    },
};
static const int tab_count[TABS] = { 9, 10, 5, FIGHTERS, GAME_SOR == 3 ? 4 : 6, 8 + (GAME_EDITION_COUNT > 1) };

/* ---- Look: the game's HUD, a yellow frame on charcoal, red bars (as the launcher) ------- */
#include "theme.h"
#define HEAD     "Impact"

static void frame_rect(float x, float y, float w, float h, float thick, Rgb c, float a) {
    ui_fill_rect(x, y, w, thick, c, a);
    ui_fill_rect(x, y + h - thick, w, thick, c, a);
    ui_fill_rect(x, y + thick, thick, h - 2 * thick, c, a);
    ui_fill_rect(x + w - thick, y + thick, thick, h - 2 * thick, c, a);
}

static void hud_box(float x, float y, float w, float h, float alpha) {
    ui_fill_rect(x, y, w, h, C_INK, .92f * alpha);
    frame_rect(x, y, w, h, 4, C_YELLOW, alpha);
    frame_rect(x + 4, y + 4, w - 8, h - 8, 2, rgb(0, 0, 0), .8f * alpha);
}

static void shadow_text(const char *s, const char *face, float px, int weight, float tracking, float x, float y,
                        Rgb c, Rgb shade, float alpha) {
    float d = px / 16;
    ui_draw_text(s, face, px, weight, tracking, x + d, y + d, shade, shade, alpha * .9f);
    ui_draw_text(s, face, px, weight, tracking, x, y, c, mix(c, C_AMBER, .35f), alpha);
}

/* What each option and action does, for the card on the right. */
static const char *about(const Item *it) {
    if (it->kind == ITEM_ACTION)
        switch (it->id) {
        case ACT_RESUME: return "Back to the game.";
        case ACT_RESET: return "Starts the game over from the beginning. Unsaved progress is lost.";
        case ACT_QUIT: return "Closes the game. Unsaved progress is lost; the game's own saves stay.";
        case ACT_CONTROLS: return "The keyboard key and controller button for every action.";
            case ACT_STATS: return NULL;
        case ACT_FIGHTER0: case ACT_FIGHTER1: case ACT_FIGHTER2: case ACT_FIGHTER3:
            return "Their hair, clothes and skin in any of the Genesis' 512 colours, still or animated, with a preview. The colours follow the fighter, whoever plays them.";
        }
    if (it->kind == ITEM_SLOT) return NULL;
    switch (it->id) {
    case OPT_WIDESCREEN: return "Shows more of the street at the sides, and the enemies walking in from beyond the old edges. Off is the original 4:3 picture.";
    case OPT_ASPECT: return "The shape of the widescreen picture.";
    case OPT_SIDES: return "Where the widescreen sides have nothing of the game (story scenes drawn for 4:3, past the end of a street): the picture's own edge, blurred and dimmed, or black.";
    case OPT_INTEGER: return "Scales by whole numbers only, so every game pixel is the same size. That leaves black borders on most screens: on a 1080p screen the picture is 4×, with a band all round. Off, the picture fills the screen.";
    case OPT_INTERNAL: return "Draws each layer at a higher resolution, rounding the edges of the pixel art. Native is the original look; 6x (2544 x 1344) is for 1440p and 4K screens and needs a fast CPU.";
    case OPT_SCALER: return "How the layers are scaled for the internal resolution: Edge rounds corners, Smooth (xBR) follows curves and slopes, Sharp keeps every pixel square, exactly as drawn. Edge and Smooth leave dither patterns crisp.";
    case OPT_FILTER: return "Scanlines, a curved CRT tube or an LCD grid laid over the picture, like the screens of the time.";
    case OPT_SHARPEN: return "Crisper edges after scaling, without halos: soft edges are sharpened, hard pixel edges are left alone.";
    case OPT_SMOOTH: return "Draws at your screen's refresh rate (120, 144 Hz or more): the scrolling and every sprite glide between the game's 60 updates. Adds about one frame of delay.";
    case OPT_FLICKER: return "Lifts the hardware's limit of sprites on one line, so crowded scenes no longer flicker.";
    case OPT_OVERCLOCK: return "Gives the console's processor more time each frame, so busy scenes no longer slow down. Music and speed stay the same.";
    case OPT_LIGHTING: return "Bright light glows into its surroundings: Soft and Bright for anything bright, Neon for the signs, lamp bulbs and lit windows of the street only (never the fighters or the HUD), with a wider glow that buzzes and now and then flickers.";
    case OPT_HDR: return "A high-dynamic-range grade for any screen: deeper shadows, brighter highlights and richer colour, so the neon and the street lights stand out against the night. Strong goes further.";
    case OPT_TEXPACK: return "High-resolution replacements for the game's tiles, from a folder in texture_packs next to the game. None is the original art.";
    case OPT_DUMP: return "Writes every tile shown on screen once to texture_packs/dump, named by its key: the starting point for making a pack.";
    case OPT_FOOTSTEPS: return "Ali kicks up what is under his feet as he walks: puffs of dust on sand and stone, bits of grass, splashes and rings in water.";
    case OPT_ELEMENTS: return "The spirits and their magic give off particles by element: embers from fire, drops from water, wisps from shadow, leaves from plants. Ali, the people of Oasis, enemies, items and the HUD give none.";
    case OPT_WATER: return "The sea, rivers and pools roll with slow swells, foam where they meet the shore and anyone standing in them, glint along the crests and reflect the people and things above them.";
    case OPT_CHEAT_HEALTH: return "Both players take no damage from enemies and their health stays full. Falling into a pit still costs a life.";
    case OPT_CHEAT_LIVES: return "Both players always have at least three lives left.";
    case OPT_CHEAT_SPECIALS: return GAME_SOR == 2 ? "Special moves cost no health when they land."
                                                  : "There is always a police car to call: the special attack never runs out.";
    case OPT_CHEAT_TIME: return "The round timer stays at 99, so time never runs out.";
    case OPT_CHEAT_UNLOCK: return GAME_SOR == 3 ? "The game's own Options screen gets its stage select, as holding Up+B while picking Options on the title menu gives it."
                                : GAME_SOR == 2 ? "The game's own Options screen gets Very Easy and Mania, up to 9 lives and a stage select, as holding A+B on controller 2 gives them."
                                : "The game's own Options screen gets its round select and lives setting, as holding A+B+C+Right on controller 2 gives them.";
    case OPT_CHEAT_SPIRIT: return "Spirit power stays full, so the spirits can be called and kept as long as you like.";
    case OPT_CHEAT_WEAPONS: return "Weapons never wear out: a weapon keeps its uses however often it strikes.";
    case OPT_CHEAT_ATTACK: return "Enemies take double or four times the damage from the players' hits, or fall to a single one. Bosses too.";
    case OPT_CHEAT_SPEED: return "Ali walks and runs one and a half or two times as fast. Walls still stop him. If a scripted scene stalls, set it back to Normal.";
    case OPT_REWIND: return "Hold the rewind key to step back in time.";
    case OPT_PLAYER2:
        if (GAME_SOR == 2) return "Who plays player 2 in a 2 PLAYERS game (picked on the title menu). Second controller: a friend. AI partner: the computer picks a fighter, fights beside you and keeps up with you.";
        return "Who plays player 2. Second controller: a friend on another controller joins with Start. AI partner: the computer joins as player 2, fights beside you and keeps up with you.";
    case OPT_RANDOMIZER: return "Chests, pots, enemies and the items lying about give other restoratives (and, with weapons, other weapons) than they used to. Each spot keeps its prize for the seed; turning it on picks a new seed. Keys, upgrades, spirit stones and the Bomb never change, so the game can always be finished.";
    case OPT_FAST: return "How fast the game runs while the fast-forward key is held (Tab, or RB on a controller; change it in Controls).";
    case OPT_COMBAT: return "The damage of every hit rises from the enemy as a number, gold for a one-hit kill; with health bars, a bar over an enemy shows what it has left for a few seconds after each hit (not over bosses: the game shows theirs).";
    case OPT_RUMBLE: return "The controller rumbles when you land a hit, harder for a knockout and when you are hit, and long and low when the police car is called.";
    case OPT_TIMER: return "This session's play time in the top corner of the picture, for speedruns and keeping track. The total is under Stats.";
    case OPT_VOLUME: return "The game's volume.";
    case OPT_EDITION: return settings_edition_count() > 1
        ? "The edition of the game played: each cartridge has its own code. Changing it starts that edition from power on, with its own saves; unsaved progress here is lost."
        : "The edition of the game played: each cartridge has its own code. Add another edition's ROM in the launcher (Game, ROM) to choose it here.";
    case OPT_MUSIC_FILES: return "Your own recordings of the songs, in the folder named music beside the game, each named by the launcher's Track number: 07.flac, or 07 - title.mp3. MP3, FLAC, WAV, M4A or WMA. They loop where the game's songs loop, and its sound effects play on. A change takes effect from the next song.";
    case OPT_BASS: return "The low end of the music and sound, from -12 dB (lighter) to +12 dB (heavier). As made is the console's own balance.";
    case OPT_AUDIO: return "Original is the console's sound as emulated. Clean removes aliasing, the hum and clipping; Rich adds wider stereo and a light room.";
    }
    return NULL;
}

/* ---- Layout, in 1280x720 units ---------------------------------------------------------- */
#define TAB_X0  56
#define TAB_Y0  22
#define TAB_W0  596
#define TAB_H0  676
#define LIST_X  (TAB_X0 + 44)
#define LIST_R  (TAB_X0 + TAB_W0 - 44)
#define TABS_Y  (TAB_Y0 + 92)
#define TABS_H  66
#define TABS_X  (TAB_X0 + 26)
#define TABW    ((TAB_W0 - 52) / TABS)
#define LIST_Y  (TABS_Y + TABS_H + 22)
#define ROW_H   40
#define VOLUME_W 140                    /* the volume slider's track, left of its value */
#define VOLUME_X (LIST_R - 20 - 46 - 26 - VOLUME_W)
#define CARD_X  690
#define CARD_Y  96
#define CARD_W  548
#define CARD_H  520

void (*menu_sound)(int kind);
static void sound(int kind) { if (menu_sound) menu_sound(kind); }
static void outfit_page_sound(int kind) { sound(kind); }

static bool open_;
static int tab, tab_from, selected;     /* selected: a row of the tab, or -1 for the tab bar */
static uint32_t *backdrop;              /* blurred game frame and the tablet, at canvas size */
static uint32_t *frame;                 /* the paused frame, for save thumbnails */
static int frame_w, frame_h;
static SlotInfo slot_info;
static int slot_info_for = -1;
static float mouse_scale = 1;
static char toast[96];
static double toast_until, now, last_now, opened_at, tab_at, changed_at;
static int changed_row = -1;
static float hl_y;
static bool confirm_pending;            /* reset/quit asked once, waiting for a second press */
static int confirm_item;
static float mouse_x, mouse_y, mouse_t = -1;   /* for the light on the steel (layout units) */

bool menu_is_open(void) { return open_; }

static float row_y(int i) { return LIST_Y + i * ROW_H; }
/* An item's name: a fighter's as this edition calls them. */
static const char *item_label(const Item *it) {
    return it->kind == ITEM_ACTION && it->id >= ACT_FIGHTER0 && it->id <= ACT_FIGHTER3 ? fighter_name(it->id - ACT_FIGHTER0) : it->label;
}

static const Item *item_at(int i) { return i >= 0 && i < tab_count[tab] ? &tab_items[tab][i] : NULL; }

/* Box blur in place, three passes per axis (close to a Gaussian). */
static void blur_rgb(uint32_t *px, int w, int h, int r) {
    if (r < 1) return;
    uint32_t *tmp = malloc((size_t)w * h * 4);
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < h; y++) {
            int sr = 0, sg = 0, sb = 0, n = 0;
            for (int x = -r; x <= r; x++) {
                if (x < 0 || x >= w) continue;
                uint32_t p = px[y * w + x]; sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255; n++;
            }
            for (int x = 0; x < w; x++) {
                tmp[y * w + x] = (uint32_t)(sr / n) << 16 | (uint32_t)(sg / n) << 8 | (uint32_t)(sb / n);
                int out = x - r, in = x + r + 1;
                if (out >= 0) { uint32_t p = px[y * w + out]; sr -= (p >> 16) & 255; sg -= (p >> 8) & 255; sb -= p & 255; n--; }
                if (in < w) { uint32_t p = px[y * w + in]; sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255; n++; }
            }
        }
        for (int x = 0; x < w; x++) {
            int sr = 0, sg = 0, sb = 0, n = 0;
            for (int y = -r; y <= r; y++) {
                if (y < 0 || y >= h) continue;
                uint32_t p = tmp[y * w + x]; sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255; n++;
            }
            for (int y = 0; y < h; y++) {
                px[y * w + x] = (uint32_t)(sr / n) << 16 | (uint32_t)(sg / n) << 8 | (uint32_t)(sb / n);
                int out = y - r, in = y + r + 1;
                if (out >= 0) { uint32_t p = tmp[out * w + x]; sr -= (p >> 16) & 255; sg -= (p >> 8) & 255; sb -= p & 255; n--; }
                if (in < h) { uint32_t p = tmp[in * w + x]; sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255; n++; }
            }
        }
    }
    free(tmp);
}

void menu_open(const uint32_t *fb, int fb_w, int fb_h, int fb_pitch, int game_x, int game_y, int game_w, int game_h,
               int out_w, int out_h, float window_to_pixels) {
    bool reopen = open_;                /* the window changed size while open: keep the place */
    open_ = true;
    if (!reopen) {
        tab = tab_from = 0;
        selected = 0;
        confirm_pending = false;
        opened_at = now;
        tab_at = -10;
        hl_y = row_y(0);
        sound(1);
    }
    slot_info_for = -1;
    mouse_scale = window_to_pixels;
    outfit_sound = outfit_page_sound;
    ui_begin(out_w, out_h);

    free(frame);
    frame_w = fb_w; frame_h = fb_h;
    frame = malloc((size_t)fb_w * fb_h * 4);
    for (int y = 0; y < fb_h; y++) memcpy(frame + (size_t)y * fb_w, fb + (size_t)y * fb_pitch, (size_t)fb_w * 4);

    /* Backdrop: the frame where the game shows it, blurred, darkened, with a red haze. */
    free(backdrop);
    backdrop = calloc((size_t)out_w * out_h, 4);
    for (int y = 0; y < game_h; y++)
        for (int x = 0; x < game_w; x++) {
            int X = game_x + x, Y = game_y + y;
            if (X < 0 || Y < 0 || X >= out_w || Y >= out_h) continue;
            backdrop[Y * out_w + X] = frame[(y * fb_h / game_h) * fb_w + x * fb_w / game_w];
        }
    blur_rgb(backdrop, out_w, out_h, (int)(9 * ui_s));
    for (int y = 0; y < out_h; y++)
        for (int x = 0; x < out_w; x++) {
            float u = (x - ui_ox) / ui_s, v = (y - ui_oy) / ui_s;
            float vig = 1 - .35f * smooth(.3f, 1.1f, sqrtf((u - 640) * (u - 640) / (640.f * 640.f) + (v - 360) * (v - 360) / (360.f * 360.f)));
            Rgb c = scale(unpack(backdrop[y * out_w + x]), .36f * vig);
            c = mix(c, scale(rgb(.10f, .02f, .02f), vig), .35f);              /* a red haze */
            c = add(c, rgb(1, 1, 1), (hashf((uint32_t)(y * out_w + x)) - .5f) * 2.5f / 255);   /* dither against banding */
            backdrop[y * out_w + x] = pack(c);
        }
}

void menu_select(int k, int row) {
    tab = tab_from = (k % TABS + TABS) % TABS;
    selected = row < -1 ? -1 : row >= tab_count[tab] ? tab_count[tab] - 1 : row;
    hl_y = row_y(selected < 0 ? 0 : selected);
    opened_at = tab_at = -10;
}

void menu_close(void) {
    if (open_) sound(2);
    open_ = false;
    savestate_info_free(&slot_info);
    slot_info_for = -1;
}

static void notify(const char *msg) {
    snprintf(toast, sizeof toast, "%s", msg);
    toast_until = now + 2.2;
}

enum { HIT_TAB0 = 100 };

static int hit_test(float mx, float my) {
    for (int k = 0; k < TABS; k++)
        if (mx >= TABS_X + k * TABW && mx < TABS_X + (k + 1) * TABW && my >= TABS_Y && my < TABS_Y + TABS_H) return HIT_TAB0 + k;
    for (int i = 0; i < tab_count[tab]; i++) {
        float y = row_y(i);
        if (mx >= LIST_X - 14 && mx <= LIST_R + 8 && my >= y && my < y + ROW_H) return i;
    }
    return -2;
}

static void select_row(int i) {
    if (i == selected) return;
    selected = i;
    confirm_pending = false;
    sound(0);
}

static void move(int dir) {
    int n = tab_count[tab] + 1;         /* the tab bar, then the rows */
    select_row(settings_cycle(selected + 1, n, dir) - 1);
}

static void open_tab(int k) {
    k = (k % TABS + TABS) % TABS;
    if (k == tab) return;
    tab_from = tab;
    tab = k;
    tab_at = now;
    if (selected >= tab_count[tab]) selected = tab_count[tab] - 1;
    confirm_pending = false;
    sound(0);
}

/* Runs the selected item (activate) or steps its value (dir = -1 / +1). */
static MenuAction act(int dir, bool activate) {
    if (selected < 0) {                 /* on the tab bar: left and right switch tabs */
        if (dir) open_tab(tab + dir);
        return MENU_NONE;
    }
    const Item *it = item_at(selected);
    if (!it) return MENU_NONE;
    char msg[96];
    switch (it->kind) {
    case ITEM_OPTION:
        if (it->id == OPT_VOLUME && !dir) return MENU_NONE;
        settings_option_change(it->id, dir ? dir : 1);
        changed_row = selected;
        changed_at = now;
        sound(1);
        return MENU_SETTINGS;
    case ITEM_SLOT:
        if (dir) {
            settings.state_slot = settings_cycle(settings.state_slot - 1, 9, dir) + 1;
            changed_row = selected;
            changed_at = now;
            sound(0);
            return MENU_SETTINGS;
        }
        if (!activate) return MENU_NONE;
        if (it->id == ACT_SAVE) {
            bool ok = savestate_save(settings.state_slot, frame, frame_w, frame_h, frame_w);
            snprintf(msg, sizeof msg, ok ? "Saved to slot %d" : "Could not save slot %d", settings.state_slot);
            notify(msg);
            sound(ok ? 1 : 3);
            slot_info_for = -1;
            return MENU_NONE;
        }
        if (savestate_load(settings.state_slot)) { sound(1); return MENU_LOADED; }
        snprintf(msg, sizeof msg, "Slot %d is empty or from another version", settings.state_slot);
        notify(msg);
        sound(3);
        return MENU_NONE;
    case ITEM_ACTION:
        if (!activate) return MENU_NONE;
        if (it->id == ACT_RESUME) return MENU_CLOSE;
        if (it->id == ACT_CONTROLS) { sound(1); controls_open(); return MENU_NONE; }
        if (it->id == ACT_OUTFIT) { sound(1); outfit_open(); return MENU_NONE; }
        if (it->id >= ACT_FIGHTER0 && it->id <= ACT_FIGHTER3) { sound(1); fighters_open(it->id - ACT_FIGHTER0); return MENU_NONE; }
        if (it->id == ACT_MAP || it->id == ACT_STATS) return MENU_NONE;     /* shown on the card */
        /* Reset and quit lose progress: ask for a second press. */
        if (!confirm_pending || confirm_item != selected) {
            confirm_pending = true;
            confirm_item = selected;
            notify(it->id == ACT_RESET ? "Press again to reset the game" : "Press again to quit");
            sound(3);
            return MENU_NONE;
        }
        confirm_pending = false;
        return it->id == ACT_RESET ? MENU_RESET : MENU_QUIT;
    }
    return MENU_NONE;
}

MenuAction menu_event(const SDL_Event *e) {
    int dir = 0;
    bool activate = false;
    if (controls_is_open() || outfit_is_open() || fighters_is_open()) {   /* a page takes all input while it is up */
        float mx = 0, my = 0;
        if (e->type == SDL_MOUSEMOTION) { mx = e->motion.x; my = e->motion.y; }
        if (e->type == SDL_MOUSEBUTTONDOWN) { mx = e->button.x; my = e->button.y; }
        mx = (mx * mouse_scale - ui_ox) / ui_s;
        my = (my * mouse_scale - ui_oy) / ui_s;
        if (outfit_is_open()) return outfit_event(e, mx, my) ? MENU_SETTINGS : MENU_NONE;
        if (fighters_is_open()) return fighters_event(e, mx, my) ? MENU_SETTINGS : MENU_NONE;
        bool changed = controls_event(e, mx, my);
        if (!controls_is_open()) sound(2);
        return changed ? MENU_SETTINGS : MENU_NONE;
    }
    switch (e->type) {
    case SDL_KEYDOWN:
        switch (e->key.keysym.sym) {
        case SDLK_ESCAPE: case SDLK_BACKSPACE: return MENU_CLOSE;
        case SDLK_UP: move(-1); break;
        case SDLK_DOWN: case SDLK_TAB: move(1); break;
        case SDLK_LEFT: dir = -1; break;
        case SDLK_RIGHT: dir = 1; break;
        case SDLK_q: case SDLK_PAGEUP: open_tab(tab - 1); break;
        case SDLK_e: case SDLK_PAGEDOWN: open_tab(tab + 1); break;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: case SDLK_z: activate = true; break;
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        switch (e->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK: case SDL_CONTROLLER_BUTTON_START: return MENU_CLOSE;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: move(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: move(1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: dir = -1; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: dir = 1; break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: open_tab(tab - 1); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: open_tab(tab + 1); break;
        case SDL_CONTROLLER_BUTTON_A: activate = true; break;
        }
        break;
    case SDL_MOUSEMOTION: {
        float mx = (e->motion.x * mouse_scale - ui_ox) / ui_s, my = (e->motion.y * mouse_scale - ui_oy) / ui_s;
        mouse_x = mx; mouse_y = my; mouse_t = (float)now;
        int h = hit_test(mx, my);
        if (h >= 0 && h < HIT_TAB0) select_row(h);
        return MENU_NONE;
    }
    case SDL_MOUSEBUTTONDOWN: {
        float mx = (e->button.x * mouse_scale - ui_ox) / ui_s, my = (e->button.y * mouse_scale - ui_oy) / ui_s;
        int h = hit_test(mx, my);
        if (h >= HIT_TAB0) { open_tab(h - HIT_TAB0); selected = -1; return MENU_NONE; }
        if (h < 0) return MENU_NONE;
        selected = h;
        const Item *it = item_at(h);
        if (e->button.button == SDL_BUTTON_LEFT && it->kind == ITEM_OPTION && it->id == OPT_VOLUME &&
            mx >= VOLUME_X - 8 && mx <= VOLUME_X + VOLUME_W + 8) {
            settings_set_volume_fraction(ui_slider_value(VOLUME_X, VOLUME_W, mx));   /* click on the track */
            sound(0);
            return MENU_SETTINGS;
        }
        if (e->button.button == SDL_BUTTON_RIGHT) dir = -1;
        else if (it->kind == ITEM_SLOT && mx > LIST_R - 150) dir = 1;   /* click the slot number to step it */
        else if (it->kind == ITEM_OPTION) dir = 1;
        else activate = true;
        break;
    }
    default:
        return MENU_NONE;
    }
    if (!dir && !activate) return MENU_NONE;
    return act(dir, activate);
}

/* ---- Drawing ----------------------------------------------------------------------------- */
static float ease_out(float x) { x = clampf(x, 0, 1); return 1 - (1 - x) * (1 - x) * (1 - x); }

static void draw_slot(float x, float y, float w, float t, float alpha) {
    if (slot_info_for != settings.state_slot) {
        savestate_info_free(&slot_info);
        savestate_info(settings.state_slot, &slot_info);
        slot_info_for = settings.state_slot;
    }
    /* The picture: thumbnail with a thin holographic foil frame. */
    float px = x, py = y, pw = w, ph = pw * 224 / 424.f;
    int x0 = (int)ui_x(px), y0 = (int)ui_y(py), x1 = (int)ui_x(px + pw), y1 = (int)ui_y(py + ph);
    for (int yy = y0 - 3; yy < y1 + 3; yy++)
        for (int xx = x0 - 3; xx < x1 + 3; xx++) {
            float u = (xx - ui_ox) / ui_s, v = (yy - ui_oy) / ui_s;
            float d = ui_rrect_dist(u, v, px, py, pw, ph, 6) * ui_s;
            if (d > 2.5f * ui_s / 1.5f + 1) continue;
            Rgb c;
            if (d < -1) {
                if (slot_info.thumb && slot_info.w) {
                    /* Fit the thumbnail by height; a narrower (4:3) one gets black bars. */
                    float tw = ph * slot_info.w / slot_info.h, tx0 = px + (pw - tw) / 2;
                    float fx = (u - tx0) / tw * slot_info.w, fy = (v - py) / ph * slot_info.h;
                    if (fx < 0 || fx >= slot_info.w) c = rgb(0, 0, 0);
                    else c = unpack(slot_info.thumb[(int)clampf(fy, 0, slot_info.h - 1) * slot_info.w + (int)fx]);
                } else {
                    float n = hashf((uint32_t)(yy * 977 + xx)) * .02f;
                    c = mix(rgb_hex(0x14100C), rgb_hex(0x1E1812), (v - py) / ph);
                    c = add(c, rgb(1, 1, 1), n);
                }
                ui_blend_px(xx, yy, c, alpha);
            } else {
                float a = clampf(1 - fabsf(d - .5f) / (1.6f * ui_s / 1.5f + .6f), 0, 1);
                (void)t;
                ui_blend_px(xx, yy, C_YELLOW, a * .9f * alpha);
            }
        }
    if (!slot_info.exists) {
        const char *empty = "Empty slot";
        float ew = ui_text_width(empty, "Segoe UI", 18, 400, 2);
        ui_draw_text(empty, "Segoe UI", 18, 400, 2, px + (pw - ew) / 2, py + ph / 2 - 12, rgb_hex(0x7A6E5A), rgb_hex(0x7A6E5A), alpha);
    }
    char when[96];
    if (slot_info.exists) {
        struct tm *tm = localtime(&slot_info.saved_at);
        strftime(when, sizeof when, "Saved %d %b %Y, %H:%M", tm);
    } else {
        snprintf(when, sizeof when, "Nothing saved here yet");
    }
    ui_draw_text(when, "Segoe UI", 15, 400, 0, px, py + ph + 18, rgb_hex(0xD8CDB6), rgb_hex(0xD8CDB6), alpha);
    ui_draw_text("\xE2\x86\x90 \xE2\x86\x92  change slot", "Segoe UI", 12.5f, 400, 1, px, py + ph + 44,
                 rgb_hex(0x8F8673), rgb_hex(0x8F8673), alpha);
}

/* Wraps `text` to `width` and draws it from (x, y); returns the y below it. */
static float draw_wrapped(const char *text, float x, float y, float width, float px, Rgb c, float alpha) {
    char line[256], word[96];
    line[0] = 0;
    const char *p = text;
    while (*p) {
        int n = 0;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && n < 95) word[n++] = *p++;
        word[n] = 0;
        if (!n) break;
        char test[352];
        snprintf(test, sizeof test, "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && ui_text_width(test, "Segoe UI", px, 400, 0) > width) {
            ui_draw_text(line, "Segoe UI", px, 400, 0, x, y, c, c, alpha);
            y += px * 1.45f;
            snprintf(line, sizeof line, "%s", word);
        } else {
            snprintf(line, sizeof line, "%s", test);
        }
    }
    if (line[0]) { ui_draw_text(line, "Segoe UI", px, 400, 0, x, y, c, c, alpha); y += px * 1.45f; }
    return y;
}

/* The card on the right, for the selected row. */
static void draw_card(float t, float alpha) {
    hud_box(CARD_X, CARD_Y, CARD_W, CARD_H, alpha);
    const Item *it = item_at(selected);
    float x = CARD_X + 28, w = CARD_W - 56, y = CARD_Y + 26;
    if (!it) {                          /* on the tab bar: the game's name and the tab */
        const char *name = GAME_NAME;
        float nw = ui_text_width(name, HEAD, 48, 400, 2);
        shadow_text(name, HEAD, GAME_SOR >= 2 ? 40 : 48, 400, 2, CARD_X + (CARD_W - nw) / 2, CARD_Y + 170, C_YELLOW, C_DEEP, alpha);
        ui_fill_rect(CARD_X + 120, CARD_Y + 246, CARD_W - 240, 3, C_RED, .9f * alpha);
        float lw = ui_text_width(tab_names[tab], HEAD, 26, 400, 4);
        ui_draw_text(tab_names[tab], HEAD, 26, 400, 4, CARD_X + (CARD_W - lw) / 2, CARD_Y + 268, C_TEXT, C_TEXT, alpha);
        const char *hint = "\xE2\x86\x90 \xE2\x86\x92  or  Q / E  to change tabs";
        float hw = ui_text_width(hint, "Segoe UI", 14, 400, 1);
        ui_draw_text(hint, "Segoe UI", 14, 400, 1, CARD_X + (CARD_W - hw) / 2, CARD_Y + 330, C_DIM, C_DIM, alpha);
        return;
    }
    char head[64];
    if (it->kind == ITEM_SLOT) snprintf(head, sizeof head, "SLOT %d", settings.state_slot);
    else {
        size_t k = 0;
        for (const char *p = item_label(it); *p && k < sizeof head - 1; p++) head[k++] = (char)((*p >= 'a' && *p <= 'z') ? *p - 32 : *p);
        head[k] = 0;
    }
    shadow_text(head, HEAD, 24, 400, 2, x, y - 4, C_YELLOW, rgb(0, 0, 0), alpha);
    ui_fill_rect(x, y + 30, 46, 3, C_RED, .9f * alpha);
    y += 48;
    if (it->kind == ITEM_SLOT) { draw_slot(x, y, w, t, alpha); return; }
    if (it->kind == ITEM_ACTION && it->id >= ACT_FIGHTER0 && it->id <= ACT_FIGHTER3) {
        /* The fighter as they are now, live, under the description. */
        int f = it->id - ACT_FIGHTER0;
        y = draw_wrapped(about(it), x, y, w, 15.5f, C_TEXT, alpha);
        if (fighters_have_preview(f)) fighters_draw(f, CARD_X + CARD_W / 2.f, CARD_Y + CARD_H - 40, 3.2f, alpha, t);
        else draw_wrapped(GAME_SOR >= 2 ? "Play a round with this fighter, and stand still for a moment, to see them here."
                                         : "Start the game once from the launcher to see the fighters here.", x, y + 20, w, 14, C_DIM, alpha);
        ui_draw_text("Enter to change\xE2\x80\xA6", "Segoe UI Semibold", 14, 600, 0, x, CARD_Y + CARD_H - 30, C_YELLOW, C_YELLOW, alpha);
        return;
    }
    if (it->kind == ITEM_ACTION && it->id == ACT_STATS) {
        char lines[12][64];
        int n = combat_stats_lines(lines, 12);
        for (int i = 0; i < n; i++) {
            char *tab_at = strchr(lines[i], '\t');
            if (!tab_at) continue;
            *tab_at = 0;
            float ly = y + i * 38 + (i >= 2 ? 22 : 0);   /* this session, then all sessions */
            if (i == 2) ui_fill_rect(x, ly - 18, w, 1, C_RED, .5f * alpha);
            ui_draw_text(lines[i], "Segoe UI", 16, 400, 0, x, ly, C_TEXT, C_TEXT, alpha);
            float vw = ui_text_width(tab_at + 1, "Segoe UI Semibold", 17, 600, 0);
            ui_draw_text(tab_at + 1, "Segoe UI Semibold", 17, 600, 0, x + w - vw, ly - 1, C_YELLOW, C_YELLOW, alpha);
        }
        return;
    }
    if (it->kind == ITEM_OPTION && preview_supported(it->id) && preview_ready()) {
        float ph = w * 9 / 16.f;
        preview_draw(it->id, x, y, w, ph, t);
        frame_rect(x - 3, y - 3, w + 6, ph + 6, 2, C_AMBER, .8f * alpha);
        y += ph + 18;
    } else {
        y += 8;
    }
    const char *text = about(it);
    if (text) y = draw_wrapped(text, x, y, w, 15.5f, C_TEXT, alpha);
    if (it->kind == ITEM_OPTION) {
        char value[64];
        settings_option_text(it->id, value, sizeof value);
        char line[96];
        snprintf(line, sizeof line, "Now: %s", value);
        ui_draw_text(line, "Segoe UI Semibold", 15, 600, 0, x, y + 8, C_YELLOW, C_YELLOW, alpha);
    }
}

void menu_render(double time_s) {
    float dt = (float)(time_s - last_now);
    if (dt < 0 || dt > .25f) dt = 1 / 60.f;
    last_now = now = time_s;
    float t = (float)time_s;
    float in = ease_out((float)(now - opened_at) / .35f);

    memcpy(ui_px, backdrop, (size_t)ui_w * ui_h * 4);

    /* The panel, "PAUSED" in the game's lettering. */
    hud_box(TAB_X0, TAB_Y0, TAB_W0, TAB_H0, 1);
    shadow_text("PAUSED", HEAD, 46, 400, 6, LIST_X - 2, TAB_Y0 + 24, C_YELLOW, C_DEEP, 1);
    if (sinf(t * 1.7f) > .97f) ui_fill_rect(LIST_X, TAB_Y0 + 80, 120, 3, rgb(1, .6f, .4f), .6f);   /* a neon flicker */
    ui_fill_rect(LIST_X, TAB_Y0 + 80, 120, 3, C_RED, .9f);

    /* The tabs: the open one filled yellow, like the HUD's frame. */
    for (int k = 0; k < TABS; k++) {
        float x = TABS_X + k * TABW + 3, w = TABW - 6, cx = x + w / 2.f;
        bool on = tab == k;
        if (on) {
            ui_fill_rect(x, TABS_Y + 10, w, TABS_H - 20, C_YELLOW, 1);
            ui_fill_rect(x, TABS_Y + TABS_H - 14, w, 4, C_RED, 1);
            if (selected < 0) frame_rect(x - 3, TABS_Y + 7, w + 6, TABS_H - 14, 2, rgb(1, 1, 1), .5f + .3f * sinf(t * 6));
        } else {
            ui_fill_rect(x, TABS_Y + 10, w, TABS_H - 20, rgb_hex(0x24242B), .9f);
            frame_rect(x, TABS_Y + 10, w, TABS_H - 20, 1, rgb_hex(0x4A4A52), 1);
        }
        Rgb tc = on ? C_INK : C_DIM;
        float tw = ui_text_width(tab_names[k], HEAD, 19, 400, 2);
        ui_draw_text(tab_names[k], HEAD, 19, 400, 2, cx - tw / 2, TABS_Y + 20, tc, tc, 1);
    }

    /* The rows slide in from the side the tab came from; the highlight glides. */
    float k = ease_out((float)(now - tab_at) / .3f);
    float dx = (1 - k) * 32 * (tab > tab_from ? 1 : -1) - (1 - in) * 40, alpha = k * in;
    if (selected >= 0) {
        hl_y += (row_y(selected) - hl_y) * (1 - expf(-dt * 14));
        ui_fill_rect(LIST_X - 14, hl_y + 3, LIST_R - LIST_X + 22, ROW_H - 6, C_RED, .22f);
        ui_fill_rect(LIST_X - 14, hl_y + 3, 5, ROW_H - 6, C_YELLOW, 1);
    }
    for (int i = 0; i < tab_count[tab]; i++) {
        const Item *it = &tab_items[tab][i];
        float y = row_y(i);
        bool focus = selected == i;
        bool danger = it->kind == ITEM_ACTION && (it->id == ACT_RESET || it->id == ACT_QUIT);
        Rgb lc = focus ? (danger && confirm_pending ? rgb_hex(0xF2A08F) : rgb(1, 1, 1)) : C_TEXT;
        ui_draw_text(item_label(it), "Segoe UI", 17.5f, 400, 0, LIST_X + dx, y + 9, lc, lc, alpha);
        char value[48] = "";
        if (it->kind == ITEM_OPTION) settings_option_text(it->id, value, sizeof value);
        else if (it->kind == ITEM_SLOT) snprintf(value, sizeof value, "Slot %d", settings.state_slot);
        if (!value[0]) continue;
        float pulse = changed_row == i ? 1 - ease_out((float)(now - changed_at) / .35f) : 0;
        Rgb vc = focus ? C_YELLOW : C_VALUE;
        if (pulse > 0) vc = mix(vc, rgb(1, 1, 1), pulse * .8f);
        Rgb ac = focus ? C_YELLOW : rgb_hex(0x5E5A52);
        float px = 17.5f + pulse * 1.5f;
        float vw = ui_text_width(value, "Segoe UI Semibold", px, 600, 0);
        float vx = LIST_R - 20 - vw;
        if (it->kind == ITEM_OPTION && it->id == OPT_VOLUME) {
            vx = LIST_R - 20 - ui_text_width("100%", "Segoe UI Semibold", px, 600, 0);
            ui_slider(VOLUME_X + dx, y + ROW_H / 2.f, VOLUME_W, settings.volume / 100.f, focus);
        }
        ui_draw_text("\xE2\x80\xB9", "Segoe UI Semibold", 20, 600, 0, vx - 18 - pulse * 3 + dx, y + 6, ac, ac, alpha);
        ui_draw_text("\xE2\x80\xBA", "Segoe UI Semibold", 20, 600, 0, LIST_R - 10 + pulse * 3 + dx, y + 6, ac, ac, alpha);
        ui_draw_text(value, "Segoe UI Semibold", px, 600, 0, vx + dx, y + 9 - pulse * .8f, vc, vc, alpha);
    }
    const char *keys = "\xE2\x86\x91\xE2\x86\x93 select    \xE2\x86\x90\xE2\x86\x92 change    Q E  tabs    Enter  select    Esc  resume";
    ui_draw_text(keys, "Segoe UI", 12, 400, 1, LIST_X, TAB_Y0 + TAB_H0 - 44, rgb_hex(0x77736A), rgb_hex(0x77736A), in);

    draw_card(t, in);

    if (now < toast_until) {
        float a = clampf((float)(toast_until - now) / .4f, 0, 1);
        float w = ui_text_width(toast, "Segoe UI Semibold", 16, 600, 1) + 48;
        float x = CARD_X + (CARD_W - w) / 2, y = CARD_Y + CARD_H + 24;
        ui_fill_rect(x, y, w, 40, rgb_hex(0x3A100A), .94f * a);
        frame_rect(x, y, w, 40, 2, C_YELLOW, a);
        ui_draw_text(toast, "Segoe UI Semibold", 16, 600, 1, x + 24, y + 9, C_YELLOW, C_YELLOW, a);
    }
    controls_render(time_s);
    outfit_render(time_s);
    fighters_render(time_s);
}
