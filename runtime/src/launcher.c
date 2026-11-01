/* Launcher: a settings screen shown before the game. The first round's street pans slowly
 * behind it at night; the title screen sits on the left like a cartridge label, and the
 * settings on the right in the style of the game's own HUD (yellow frame, red bars). The
 * game's soundtrack plays through a built-in player, with its own sounds for every choice.
 *
 * The artwork comes from the user's own ROM: the launcher runs the game silently to the
 * title and into round 1 and captures them, then caches them in launcher_art.cache next
 * to the executable. Everything is drawn in software into one ui_px (also used for
 * headless screenshots), at the window's pixel scale. */
#include "launcher.h"
#include "ost.h"
#include "launcher_scene.h"
#include "pads.h"
#include "foil.h"
#include "recomp_rt.h"
#include "genesis.h"
#include "settings.h"
#include "ui.h"
#include "png.h"
#include "controls.h"
#include "outfit.h"
#include "fighters.h"
#include "preview.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

#define BASE_W UI_BASE_W
#define BASE_H UI_BASE_H

/* ---- Artwork captured from the ROM -------------------------------------------- *
 * The intro's city skyline before its story text rolls in (the cover, on the left) and a
 * panorama of the first round's street, drawn without its people or the HUD (the
 * backdrop). Both come from running the game: through the intro, then Start into round 1
 * and walking right while the camera follows. */
static Art cover, pano;
bool launcher_in_collection;

#define ART_MAGIC    0x32524F53u       /* cache file tag (bump when the capture changes) */
#define PANO_TOP     32                 /* the street starts under the HUD */
#define PANO_MAX_W   1600

static char cache_path[1100];

static void art_free(Art *a) { free(a->rgb); free(a->mask); memset(a, 0, sizeof *a); }

static void art_alloc(Art *a, int w, int h) {
    art_free(a);
    a->w = w; a->h = h;
    a->rgb = calloc((size_t)w * h, 4);
    a->mask = calloc((size_t)w * h, 1);
}

/* Copies rows y0..y0+h of a 320-wide frame into a at column x (opaque). */
static void art_paste(Art *a, const uint32_t *fb, int pitch, int x, int y0, int h) {
    for (int y = 0; y < h && y < a->h; y++)
        for (int i = 0; i < 320; i++) {
            int ax = x + i;
            if (ax < 0 || ax >= a->w) continue;
            a->rgb[y * a->w + ax] = fb[(y0 + y) * pitch + i];
            a->mask[y * a->w + ax] = 255;
        }
}

#if GAME_SOR == 1
/* The select screen is up: a sprite of Blaze's still (tiles $680-$6BF) in palette 0. */
static bool select_screen(void) {
    uint32_t sat = (vdp.reg[5] & 0x7E) << 9;
    for (int i = 0; i < 80; i++) {
        uint32_t e = (sat + i * 8) & 0xFFFF;
        int attr = vdp.vram[e + 4] << 8 | vdp.vram[e + 5];
        if (!(attr & 0x6000) && (attr & 0x7FF) >= 0x680 && (attr & 0x7FF) < 0x6C0) return true;
    }
    return false;
}
#endif

static void run_frames(uint32_t *fb, int n, uint16_t pad) {
    for (int f = 0; f < n; f++) { pad_state[0] = pad; pad_state[1] = 0; machine_run_frame(f == n - 1 ? fb : NULL, 320); }
}

static bool art_save_cache(void) {
    FILE *f = fopen(cache_path, "wb");
    if (!f) return false;
    uint32_t magic = ART_MAGIC;
    fwrite(&magic, 4, 1, f);
    Art *arts[2] = { &cover, &pano };
    for (int i = 0; i < 2; i++) {
        fwrite(&arts[i]->w, 4, 1, f); fwrite(&arts[i]->h, 4, 1, f);
        fwrite(arts[i]->rgb, 4, (size_t)arts[i]->w * arts[i]->h, f);
        fwrite(arts[i]->mask, 1, (size_t)arts[i]->w * arts[i]->h, f);
    }
    fclose(f);
    return true;
}

static bool art_load_cache(void) {
    FILE *f = fopen(cache_path, "rb");
    if (!f) return false;
    uint32_t magic = 0;
    bool ok = fread(&magic, 4, 1, f) == 1 && magic == ART_MAGIC;
    Art *arts[2] = { &cover, &pano };
    for (int i = 0; ok && i < 2; i++) {
        Art *a = arts[i];
        int w = 0, h = 0;
        ok = fread(&w, 4, 1, f) == 1 && fread(&h, 4, 1, f) == 1 && w > 0 && h > 0 && w <= PANO_MAX_W && h <= 224;
        if (!ok) break;
        art_alloc(a, w, h);
        ok = fread(a->rgb, 4, (size_t)w * h, f) == (size_t)w * h && fread(a->mask, 1, (size_t)w * h, f) == (size_t)w * h;
    }
    fclose(f);
    if (!ok) { art_free(&cover); art_free(&pano); }
    return ok;
}

/* Runs the game from a reset (blank save RAM, original speed) to the title, then into
 * round 1, walking right: a few seconds, once; the result is cached. */
#if GAME_SOR >= 2
/* Streets of Rage 2 and 3: the title screen is the cover; then Start through the menus into
 * round 1 and walk right for the street (docs/ram-sor2.md, docs/ram-sor3.md). */
#if GAME_SOR == 3
#define COVER_FRAME 880                 /* the logo and "press start", Start at 300 and 500 */
#define PANO_TOP_GAME 40
static bool in_round(void) {
    uint16_t s = (uint16_t)(bus.ram[0xDF00] << 8 | bus.ram[0xDF01]);
    return s && s != 0x5A && s != 0x5C && s != 0x5E && !bus.ram[0xFB13] && bus.ram[0xDF6C];
}
static int camera_x(void) { return (int16_t)(bus.ram[0xF710] << 8 | bus.ram[0xF711]); }
#else
#define COVER_FRAME 700
#define PANO_TOP_GAME 40
static bool in_round(void) {
    uint16_t s = (uint16_t)(bus.ram[0xEF00] << 8 | bus.ram[0xEF01]);
    return s && s < 0x30 && !bus.ram[0xFD10];
}
static int camera_x(void) { return -(int16_t)(bus.ram[0xFC22] << 8 | bus.ram[0xFC23]); }
#endif

static void art_capture(void) {
    static uint32_t fb[320 * 240];
    int extra = vdp_opts.extra_width, hide = vdp_opts.hide_layers, overclock = machine_opts.overclock;
    bool mask = vdp_opts.mask_sides;
    vdp_opts.extra_width = 0;
    widescreen_patches(0);
    machine_opts.overclock = 100;
    uint8_t *sram = malloc(SRAM_SIZE);
    memcpy(sram, bus.sram, SRAM_SIZE);
    memset(bus.sram, 0xFF, SRAM_SIZE);
    machine_reset();
    for (int f = 0; f < COVER_FRAME; f++) run_frames(fb, 1, f == 300 || f == 500 || (f > 300 && f < 306) || (f > 500 && f < 506) ? PAD_START : 0);
    art_alloc(&cover, 320, 224);
    art_paste(&cover, fb, 320, 0, 0, 224);
    /* Start every 200 frames (held for 6) until round 1 is under way. */
    for (int f = COVER_FRAME; f < 6000 && !in_round(); f++) run_frames(fb, 1, f % 200 < 6 ? PAD_START : 0);
    run_frames(fb, 90, 0);
    vdp_opts.hide_layers = 4;
    art_alloc(&pano, PANO_MAX_W, 224 - PANO_TOP_GAME);
    int cam0 = -1, right = 0;
    for (int f = 0; f < 3000 && right < PANO_MAX_W; f += 8) {
        run_frames(fb, 8, PAD_RIGHT | (f % 64 < 8 ? PAD_B : 0));
        int cam = camera_x();
        if (cam0 < 0) cam0 = cam;
        if (cam < cam0) continue;
        art_paste(&pano, fb, 320, cam - cam0, PANO_TOP_GAME, 224 - PANO_TOP_GAME);
        if (cam - cam0 + 320 > right) right = cam - cam0 + 320;
    }
    if (right < PANO_MAX_W) {
        Art cut = { 0 };
        art_alloc(&cut, right, pano.h);
        for (int y = 0; y < pano.h; y++) {
            memcpy(cut.rgb + y * right, pano.rgb + y * pano.w, (size_t)right * 4);
            memcpy(cut.mask + y * right, pano.mask + y * pano.w, (size_t)right);
        }
        art_free(&pano);
        pano = cut;
    }
    vdp_opts.hide_layers = hide;
    machine_opts.overclock = overclock;
    memcpy(bus.sram, sram, SRAM_SIZE);
    free(sram);
    vdp_opts.extra_width = extra;
    vdp_opts.mask_sides = mask;
    widescreen_patches(extra);
    pad_state[0] = pad_state[1] = 0;
    if (cover.w && pano.w) art_save_cache();
    char *base = SDL_GetBasePath();
    char card[1100];
    snprintf(card, sizeof card, "%scard.png", base ? base : "");
    if (base) SDL_free(base);
    if (cover.w) png_write(card, cover.rgb, cover.w, cover.h, cover.w);
}
#else
static void art_capture(void) {
    static uint32_t fb[320 * 240];
    int extra = vdp_opts.extra_width, hide = vdp_opts.hide_layers, overclock = machine_opts.overclock;
    bool mask = vdp_opts.mask_sides;
    vdp_opts.extra_width = 0;
    widescreen_patches(0);
    machine_opts.overclock = 100;
    uint8_t *sram = malloc(SRAM_SIZE);
    memcpy(sram, bus.sram, SRAM_SIZE);
    memset(bus.sram, 0xFF, SRAM_SIZE);
    machine_reset();
    /* The skyline, once it has faded in (its top two thirds lit and still), before the
     * story's white text rolls up over it. */
    int last = -1;
    for (int f = 0; f < 1200; f += 4) {
        run_frames(fb, 4, 0);
        int lit = 0, white = 0;
        for (int i = 0; i < 320 * 224; i++) {
            uint32_t p = fb[i];
            if (i < 320 * 150) lit += ((p >> 16) & 255) > 16 || ((p >> 8) & 255) > 16 || (p & 255) > 16;
            white += ((p >> 16) & 255) > 200 && ((p >> 8) & 255) > 200 && (p & 255) > 200;
        }
        if (f >= 200 && lit > 320 * 150 * 6 / 10 && abs(lit - last) < 50 && white < 200) break;
        last = lit;
    }
    art_alloc(&cover, 320, 224);
    art_paste(&cover, fb, 320, 0, 0, 224);
    /* Start through the menus until round 1 is under way: game mode $16 ($FFFF00; the select
     * screen, $12, already has player 1 in the game), not the demo. */
    bool picked = false;
    for (int f = 0; f < 4000 && !(bus.ram[0xFF01] == 0x16 && (bus.ram[0xFF18] & 1) && !bus.ram[0xFF2A] && bus.ram[0xB800]); f++) {
        /* The select screen (Blaze's still, tiles $680, in palette 0): the fighters' previews. */
        if (!picked && select_screen()) { run_frames(fb, 40, 0); fighters_capture_select(); picked = true; }
        run_frames(fb, 1, f % 150 < 6 ? PAD_START : 0);   /* slow enough for the select screen to show its fighters */
    }
    run_frames(fb, 120, 0);
    fighters_capture_colours();         /* palette 0 as the round has it */
    char *dir = SDL_GetBasePath();
    fighters_preview_save(dir ? dir : "");
    if (dir) SDL_free(dir);
    /* Walk right; the street, without sprites, at each step of the camera. */
    vdp_opts.hide_layers = 4;
    art_alloc(&pano, PANO_MAX_W, 224 - PANO_TOP);
    int cam0 = -1, right = 0;
    for (int f = 0; f < 2400 && right < PANO_MAX_W; f += 8) {
        run_frames(fb, 8, PAD_RIGHT | (f % 64 < 8 ? PAD_B : 0));
        int cam = (int16_t)(bus.ram[0xE002] << 8 | bus.ram[0xE003]);
        if (cam <= 0) continue;         /* (0 while the round is still being set up) */
        if (cam0 < 0) cam0 = cam;
        art_paste(&pano, fb, 320, cam - cam0, PANO_TOP, 224 - PANO_TOP);
        if (cam - cam0 + 320 > right) right = cam - cam0 + 320;
    }
    if (right < PANO_MAX_W) {           /* keep only what was filled */
        Art cut = { 0 };
        art_alloc(&cut, right, pano.h);
        for (int y = 0; y < pano.h; y++) {
            memcpy(cut.rgb + y * right, pano.rgb + y * pano.w, (size_t)right * 4);
            memcpy(cut.mask + y * right, pano.mask + y * pano.w, (size_t)right);
        }
        art_free(&pano);
        pano = cut;
    }
    vdp_opts.hide_layers = hide;
    machine_opts.overclock = overclock;
    memcpy(bus.sram, sram, SRAM_SIZE);
    free(sram);
    vdp_opts.extra_width = extra;
    vdp_opts.mask_sides = mask;
    widescreen_patches(extra);
    pad_state[0] = pad_state[1] = 0;
    if (cover.w && pano.w) art_save_cache();
    /* The collection's card for this game: its cover, next to the program. */
    char *base = SDL_GetBasePath();
    char card[1100];
    snprintf(card, sizeof card, "%scard.png", base ? base : "");
    if (base) SDL_free(base);
    if (cover.w) png_write(card, cover.rgb, cover.w, cover.h, cover.w);
}
#endif

static bool rom_ok;

static void art_prepare(void) {
    art_free(&cover); art_free(&pano);
    rom_ok = settings.rom_path[0] && !bus_load_rom(settings.rom_path);
    if (!rom_ok) return;
    if (!art_load_cache()) art_capture();
}

static void init_common(void);

/* ---- Soundtrack player ------------------------------------------------------------ *
 * The launcher plays the game's own soundtrack. A background thread runs the ROM from a
 * reset (with blank save RAM, like the art capture) to the title screen in a fraction of
 * a second without sound, notes which song the title asked for, then parks the game and
 * drives its sound driver directly (ost.c): any of the 34 songs, and the game's own menu
 * sounds for the launcher's choices. Songs that loop play for PLAY_FOR seconds and fade
 * out; jingles end when they go quiet. The thread owns the machine until music_stop();
 * the game resets it afterwards. */
#define PLAY_FOR   (3 * 60)             /* seconds a looping song plays before the next */
#define FADE_OUT   (4 * 60)             /* frames */
#define FADE_STOP  15                   /* frames: quitting or switching off */
#define METER_BARS 24

static SDL_Thread *music_thread;
static SDL_AudioDeviceID music_dev;
static SDL_mutex *music_lock;
static uint8_t *music_sram;             /* the real save RAM, put back by music_stop() */
static bool music_sram_dirty;
static uint32_t music_fb[320 * 240];
static int title_song = -1;             /* the song the title screen starts */

/* Requests from the launcher and what the player is doing, under music_lock. */
static struct {
    bool quit;
    int  want_song;                     /* -1: none */
    int  step;                          /* +1 / -1: next / previous song */
    bool want_on, want_off;
    uint16_t sfx[8];
    int  sfx_count;
    /* published by the player */
    bool ready, on;
    int  song, frames;                  /* the song playing, and for how long */
    float meter[METER_BARS];            /* level of the latest frame, in slices, 0-1 */
} player;

/* Runs the game from a reset to its title screen, where the driver is set up. */
static bool music_to_title(void) {
    vdp_opts.extra_width = 0;
    widescreen_patches(0);
    memset(bus.sram, 0xFF, SRAM_SIZE);
    machine_reset();
    run_frames(music_fb, 1000, 0);
    title_song = ost_title_song();
    return true;
}

/* One frame of sound, scaled by volume (0-1). Returns the stereo frames in sound_out and
 * the frame's level without its DC offset (the FM DAC can hold a constant level). */
static int music_frame(float volume, double *level) {
    ost_frame();
    sound_quality = settings.audio;     /* the Audio setting applies to the player too */
    machine_run_frame(NULL, 320);
    double sum = 0, sum2 = 0;
    int n = sound_out_count * 2;
    for (int i = 0; i < n; i++) { sum += sound_out[i]; sum2 += (double)sound_out[i] * sound_out[i]; }
    *level = n ? sqrt(fmax(0, sum2 / n - (sum / n) * (sum / n))) : 0;
    int v = (int)(volume * settings.volume / 100.f * 256);
    for (int i = 0; i < n; i++) sound_out[i] = (int16_t)(sound_out[i] * v / 256);
    return sound_out_count;
}

static int next_song(int song, int dir) {
    if (settings.ost_mode == OST_MODE_SHUFFLE && dir > 0) {
        int s = (int)(hash32((uint32_t)SDL_GetPerformanceCounter()) % (OST_SONGS - 1));
        return s >= song ? s + 1 : s;   /* any other song */
    }
    return settings_cycle(song, OST_SONGS, dir);
}

/* The player's state, kept by the thread (and by launcher_music_check()). */
typedef struct {
    int song, frames;
    bool on;
    float volume, peak;
    double top, quiet;                  /* loudest second so far; frames it has been quiet */
    int fading;                         /* frames of fade left (0: not fading) */
    int after_fade;                     /* song to start when the fade ends, -1 to stop */
} Player;

static void start_song(Player *p, int song) {
    p->song = song;
    p->frames = 0;
    p->top = p->quiet = 0;
    p->fading = 0;
    p->volume = p->peak;
    ost_command(OST_STOP);
    ost_command(OST_PLAY(song));
}

static void fade_to(Player *p, int song, int frames) {
    if (p->fading) { p->after_fade = song; return; }
    p->fading = frames;
    p->after_fade = song;
}

/* Runs one frame of the player; returns the stereo frames in sound_out. */
static int player_frame(Player *p) {
    double level;
    if (p->fading) {
        p->volume -= p->volume / p->fading;
        if (--p->fading == 0) {
            p->volume = 0;
            if (p->after_fade >= 0) start_song(p, p->after_fade);
            else { ost_command(OST_STOP); p->on = false; p->volume = p->peak; }
        }
    }
    int n = music_frame(p->on || p->fading ? p->volume : p->peak, &level);
    if (!p->on || p->fading) return n;
    p->frames++;
    /* A jingle is over once it stays quiet (well under its loudest part) for 1.5 s. */
    if (level > p->top) p->top = level;
    p->quiet = level < fmax(120, p->top * .03) ? p->quiet + 1 : 0;
    bool ended = p->frames > 3 * 60 && p->quiet > 90;
    bool long_enough = settings.ost_mode != OST_MODE_REPEAT && p->frames >= PLAY_FOR * 60;
    if (ended) start_song(p, settings.ost_mode == OST_MODE_REPEAT ? p->song : next_song(p->song, 1));
    else if (long_enough) fade_to(p, next_song(p->song, 1), FADE_OUT);
    return n;
}

static void publish(const Player *p) {
    float m[METER_BARS];
    int n = sound_out_count;
    for (int b = 0; b < METER_BARS; b++) {
        double s = 0;
        int i0 = n * b / METER_BARS, i1 = n * (b + 1) / METER_BARS, c = 0;
        for (int i = i0; i < i1; i++) { double v = (sound_out[i * 2] + sound_out[i * 2 + 1]) * .5; s += v * v; c++; }
        m[b] = c ? clampf((float)sqrt(s / c) / 9000.f, 0, 1) : 0;
    }
    SDL_LockMutex(music_lock);
    player.ready = true;
    player.on = p->on && !(p->fading && p->after_fade < 0);
    player.song = p->song;
    player.frames = p->frames;
    for (int b = 0; b < METER_BARS; b++) player.meter[b] = p->on ? fmaxf(m[b], player.meter[b] * .82f) : player.meter[b] * .8f;
    SDL_UnlockMutex(music_lock);
}

static int music_run(void *arg) {
    (void)arg;
    if (!music_to_title()) return 0;
    ost_start();
    Player p = { .song = title_song >= 0 ? title_song : 0, .peak = .8f, .after_fade = -1 };
    if (settings.ost_track >= 0 && settings.ost_track < OST_SONGS) p.song = settings.ost_track;
    p.volume = p.peak;
    p.on = settings.launcher_music;
    if (p.on) start_song(&p, p.song);
    else ost_command(OST_STOP);
    const Uint32 target = (Uint32)sound_out_rate * 4 / 10;      /* keep about 100 ms queued */
    bool quitting = false;
    for (;;) {
        SDL_LockMutex(music_lock);
        bool quit = player.quit;
        int want = player.want_song, step = player.step;
        bool want_on = player.want_on, want_off = player.want_off;
        for (int k = 0; k < player.sfx_count; k++) ost_command(player.sfx[k]);
        player.sfx_count = 0;
        player.want_song = -1; player.step = 0; player.want_on = player.want_off = false;
        SDL_UnlockMutex(music_lock);

        if (quit && !quitting) { quitting = true; if (p.on) fade_to(&p, -1, FADE_STOP); }
        if (!quitting) {
            if (step) want = next_song(p.song, step);
            if (want >= 0) { p.on = true; start_song(&p, want); }
            if (want_on && !p.on) { p.on = true; start_song(&p, p.song); }
            if (want_off && p.on) fade_to(&p, -1, FADE_STOP);
        }
        if (quitting && !p.fading) break;
        if (SDL_GetQueuedAudioSize(music_dev) > target) { SDL_Delay(2); continue; }
        int n = player_frame(&p);
        SDL_QueueAudio(music_dev, sound_out, (Uint32)n * 4);
        publish(&p);
    }
    ost_end();
    return 0;
}

static void music_start(void) {
    if (music_thread || !rom_ok) return;
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioSpec want = { .freq = 48000, .format = AUDIO_S16SYS, .channels = 2, .samples = 1024 }, have;
    music_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!music_dev) return;
    sound_out_rate = have.freq;
    if (!music_lock) music_lock = SDL_CreateMutex();
    memset(&player, 0, sizeof player);
    player.want_song = -1;
    music_sram = malloc(SRAM_SIZE);
    memcpy(music_sram, bus.sram, SRAM_SIZE);
    music_sram_dirty = bus.sram_dirty;
    SDL_PauseAudioDevice(music_dev, 0);
    music_thread = SDL_CreateThread(music_run, "music", NULL);
}

static void music_stop(void) {
    if (!music_thread) return;
    SDL_LockMutex(music_lock);
    player.quit = true;
    SDL_UnlockMutex(music_lock);
    SDL_WaitThread(music_thread, NULL);
    music_thread = NULL;
    SDL_Delay(60);                      /* let the faded tail play out */
    SDL_CloseAudioDevice(music_dev);
    music_dev = 0;
    memcpy(bus.sram, music_sram, SRAM_SIZE);
    bus.sram_dirty = music_sram_dirty;
    free(music_sram);
    music_sram = NULL;
    vdp_opts.extra_width = settings_extra_width();
    widescreen_patches(vdp_opts.extra_width);
    pad_state[0] = 0;
}

/* Requests to the player; they do nothing before it is running. */
static void music_sfx(uint16_t sfx) {
    if (!music_thread) return;
    SDL_LockMutex(music_lock);
    if (player.sfx_count < 8) player.sfx[player.sfx_count++] = sfx;
    SDL_UnlockMutex(music_lock);
}

static void music_request(int song, int step, bool on, bool off) {
    if (!music_thread) return;
    SDL_LockMutex(music_lock);
    if (song >= 0) player.want_song = song;
    player.step += step;
    player.want_on |= on;
    player.want_off |= off;
    SDL_UnlockMutex(music_lock);
}

/* --launcher-music-check: the player without an audio device, at full speed. Prints each
 * second's level and every song change, starting with the title theme. */
int launcher_music_check(int seconds) {
    init_common();
    if (!rom_ok) {
        const char *why = settings.rom_path[0] ? bus_load_rom(settings.rom_path) : "no ROM path in settings";
        fprintf(stderr, "could not load the ROM: %s\n", why ? why : "?");
        return 1;
    }
    uint8_t *sram = malloc(SRAM_SIZE);
    memcpy(sram, bus.sram, SRAM_SIZE);
    bool dirty = bus.sram_dirty;
    Uint64 t0 = SDL_GetPerformanceCounter();
    bool ok = music_to_title();
    printf("title reached in %.0f ms: %s; the title theme is song %02X\n",
           (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency(), ok ? "yes" : "NO", title_song);
    if (ok) {
        ost_start();
        Player p = { .peak = 1, .volume = 1, .on = true, .after_fade = -1 };
        start_song(&p, title_song >= 0 ? title_song : 0);
        int shown = -1;
        for (int s = 0; s < seconds; s++) {
            double sum = 0;
            long n = 0;
            for (int f = 0; f < 60; f++) {
                if (p.song != shown) { printf("  second %d: song %02X\n", s, p.song); shown = p.song; }
                int c = player_frame(&p);
                for (int i = 0; i < c * 2; i++) { sum += (double)sound_out[i] * sound_out[i]; n++; }
            }
            if (s % 30 == 0) printf("second %4d: level %5.0f rms\n", s, n ? sqrt(sum / n) : 0);
        }
        ost_end();
    }
    memcpy(bus.sram, sram, SRAM_SIZE);
    bus.sram_dirty = dirty;
    free(sram);
    return ok ? 0 : 1;
}

/* Renders the four menu sounds the launcher uses into `path` for the pause menu, which
 * cannot call the driver while the game is paused: 48 kHz 16-bit stereo, each sound's
 * frame count first. Runs the ROM to the title like the player (about half a second). */
bool launcher_make_menu_sounds(const char *path) {
    if (!rom_ok && !(settings.rom_path[0] && !bus_load_rom(settings.rom_path))) return false;
    rom_ok = true;
    int rate = sound_out_rate, quality = sound_quality;
    sound_out_rate = 48000;
    sound_quality = 0;
    bool ok = music_to_title();
    FILE *f = ok ? fopen(path, "wb") : NULL;
    if (f) {
        ost_start();
        static const uint16_t sfx[4] = { SFX_CURSOR, SFX_CONFIRM, SFX_CANCEL, SFX_ERROR };
        static int16_t pcm[48000 * 2];
        uint32_t magic = MENU_SOUNDS_MAGIC;
        fwrite(&magic, 4, 1, f);
        fwrite(sfx, 2, 4, f);           /* which effects: a cache made for others is made again */
        for (int k = 0; k < 4; k++) {
            ost_command(OST_STOP);
            for (int i = 0; i < 20; i++) { ost_frame(); machine_run_frame(NULL, 320); }
            ost_command(sfx[k]);
            int n = 0;
            for (int i = 0; i < 36; i++) {      /* 0.6 s */
                ost_frame();
                machine_run_frame(NULL, 320);
                if (n + sound_out_count <= 48000) { memcpy(pcm + n * 2, sound_out, (size_t)sound_out_count * 4); n += sound_out_count; }
            }
            /* Without the DC offset, faded out over the last 20 ms. */
            double dc[2] = { 0, 0 };
            for (int i = 0; i < n; i++) { dc[0] += pcm[i * 2]; dc[1] += pcm[i * 2 + 1]; }
            for (int i = 0; i < n; i++)
                for (int c = 0; c < 2; c++) {
                    double v = pcm[i * 2 + c] - dc[c] / n;
                    if (i > n - 960) v *= (n - i) / 960.0;
                    pcm[i * 2 + c] = (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
                }
            fwrite(&n, 4, 1, f);
            fwrite(pcm, 4, (size_t)n, f);
        }
        ost_end();
        fclose(f);
    }
    sound_out_rate = rate;
    sound_quality = quality;
    return f != NULL;
}

/* Plays every song for up to `seconds` and prints its level per second (in tenths of the
 * loudest second), to tell songs from jingles and empty entries. */
int launcher_sound_probe(int from, int to, int seconds) {
    if (!settings.rom_path[0] || bus_load_rom(settings.rom_path)) { fprintf(stderr, "could not load the ROM\n"); return 1; }
    music_to_title();
    ost_start();
    for (int id = from; id <= to; id++) {
        ost_request(0);
        for (int f = 0; f < 20; f++) { ost_frame(); machine_run_frame(NULL, 320); }
        ost_request((uint16_t)id);
        if (!seconds) {                 /* a sound effect: its level and length in frames */
            double lv[90], top = 0, all = 0;
            for (int f = 0; f < 90; f++) {
                ost_frame();
                machine_run_frame(NULL, 320);
                double sum = 0, sum2 = 0;
                int k = sound_out_count * 2;
                for (int i = 0; i < k; i++) { sum += sound_out[i]; sum2 += (double)sound_out[i] * sound_out[i]; }
                lv[f] = k ? sqrt(fmax(0, sum2 / k - (sum / k) * (sum / k))) : 0;
                if (lv[f] > top) top = lv[f];
                all += lv[f];
            }
            int last = 0;
            for (int f = 0; f < 90; f++) if (lv[f] > top / 10) last = f + 1;
            printf("%04X level %6.0f peak %6.0f %2d frames\n", id, all / 90, top, last);
            continue;
        }
        double top = 0;
        char line[700];
        int n = 0;
        for (int s = 0; s < seconds && s < 600; s++) {
            double sum = 0, sum2 = 0;
            long k = 0;
            for (int f = 0; f < 60; f++) {
                ost_frame();
                machine_run_frame(NULL, 320);
                for (int i = 0; i < sound_out_count * 2; i++) { sum += sound_out[i]; sum2 += (double)sound_out[i] * sound_out[i]; k++; }
            }
            double level = k ? sqrt(fmax(0, sum2 / k - (sum / k) * (sum / k))) : 0;
            if (level > top) top = level;
            line[n++] = level < 30 ? '.' : level < 300 ? '-' : level < 1500 ? '+' : '#';
        }
        line[n] = 0;
        printf("%04X peak %6.0f %s\n", id, top, line);
        fflush(stdout);
    }
    return 0;
}

int launcher_ost_check(int seconds) {
    init_common();
    if (!rom_ok) { fprintf(stderr, "could not load the ROM\n"); return 1; }
    uint8_t *sram = malloc(SRAM_SIZE);
    memcpy(sram, bus.sram, SRAM_SIZE);
    bool dirty = bus.sram_dirty;
    bool ok = music_to_title();
    ost_start();
    for (int song = 0; ok && song < OST_SONGS; song++) {
        ost_command(OST_STOP);
        ost_command(OST_PLAY(song));
        double level[600] = { 0 }, top = 0;
        for (int s = 0; s < seconds && s < 600; s++) {
            double sum = 0, sum2 = 0;
            long n = 0;
            for (int f = 0; f < 60; f++) {
                ost_frame();
                machine_run_frame(NULL, 320);
                for (int i = 0; i < sound_out_count * 2; i++) { sum += sound_out[i]; sum2 += (double)sound_out[i] * sound_out[i]; n++; }
            }
            /* Without the DC offset (the FM DAC holds a constant level after a jingle). */
            level[s] = n ? sqrt(fmax(0, sum2 / n - (sum / n) * (sum / n))) : 0;
            if (level[s] > top) top = level[s];
        }
        printf("song %02X peak %5.0f: ", song, top);
        for (int s = 0; s < seconds && s < 600; s++) putchar(top > 0 ? '0' + (int)(level[s] * 9.99 / top) : '.');
        putchar('\n');
    }
    /* The sound effects over silence: their level, and how long they last (frames until
     * the last one above a tenth of the loudest). The menu's four, or with SMPS every one. */
    uint16_t sfx[64] = { SFX_CURSOR, SFX_CONFIRM, SFX_CANCEL, SFX_ERROR };
    int nsfx = 4;
    if (GAME_SMPS) for (nsfx = 0; nsfx < 64; nsfx++) sfx[nsfx] = (uint16_t)(0x02A0 + nsfx);
    for (int k = 0; ok && k < nsfx; k++) {
        ost_command(OST_STOP);
        for (int f = 0; f < 30; f++) { ost_frame(); machine_run_frame(NULL, 320); }
        ost_command(sfx[k]);
        double level[60], top = 0, all = 0;
        for (int f = 0; f < 60; f++) {
            ost_frame();
            machine_run_frame(NULL, 320);
            double sum = 0, sum2 = 0;
            int n = sound_out_count * 2;
            for (int i = 0; i < n; i++) { sum += sound_out[i]; sum2 += (double)sound_out[i] * sound_out[i]; }
            level[f] = n ? sqrt(fmax(0, sum2 / n - (sum / n) * (sum / n))) : 0;
            if (level[f] > top) top = level[f];
            all += level[f];
        }
        int last = 0;
        for (int f = 0; f < 60; f++) if (level[f] > top * .1) last = f + 1;
        printf("sound effect %04X: level %5.0f, peak %5.0f, %2d frames\n", sfx[k], all / 60, top, last);
    }
    ost_end();
    memcpy(bus.sram, sram, SRAM_SIZE);
    bus.sram_dirty = dirty;
    free(sram);
    return ok ? 0 : 1;
}

/* ---- The settings: five tabs ----------------------------------------------------- */
typedef enum { ROW_OPTION, ROW_ROM, ROW_CONTROLS, ROW_TRACK, ROW_OUTFIT, ROW_FIGHTER } RowKind;
typedef struct { RowKind kind; const char *label; int id; const char *about; } Row;

#define TABS 6
static const char *const tab_names[TABS] = { "DISPLAY", "EFFECTS", "GAME", "MUSIC", "COLOURS", "CHEATS" };
static const Row tab_rows[TABS][9] = {
    { { ROW_OPTION, "Widescreen", OPT_WIDESCREEN, "More of the street at the sides; enemies show beyond the old edges." },
      { ROW_OPTION, "Aspect ratio", OPT_ASPECT, "The shape of the widescreen picture." },
      { ROW_OPTION, "Integer scaling", OPT_INTEGER, "Whole numbers only, every pixel the same size: borders on most screens (4× on 1080p). Off fills the screen." },
      { ROW_OPTION, "Window", OPT_WINDOW_ALL, "Fullscreen, or a window of this many times the game's size." },
      { ROW_OPTION, "Filter", OPT_FILTER, "Scanlines, a curved CRT tube or an LCD grid over the picture." },
      { ROW_OPTION, "Sharpening", OPT_SHARPEN, "Crisper edges after scaling, without halos (contrast-adaptive)." },
      { ROW_OPTION, "Internal resolution", OPT_INTERNAL, "Redraws each layer at a higher resolution; 6x (4K) needs a fast CPU." },
      { ROW_OPTION, "Scaler", OPT_SCALER, "Edge rounds corners; Smooth follows curves (xBR); Sharp keeps pixels square." },
      { ROW_OPTION, "Texture pack", OPT_TEXPACK, "High-resolution tile replacements from a folder in texture_packs." } },
    { { ROW_OPTION, "Lighting", OPT_LIGHTING, "Glow from bright light; Neon: the signs and lamps only, buzzing." },
      { ROW_OPTION, "HDR look", OPT_HDR, "Deeper shadows, brighter highlights, richer colour." },
      { ROW_OPTION, "Combat info", OPT_COMBAT, "Damage numbers rise from enemies; health bars show what they have left." },
      { ROW_OPTION, "Rumble", OPT_RUMBLE, GAME_SOR == 1 ? "The controller rumbles on your hits, knockouts, when you are hit and on the police car."
                                                         : "The controller rumbles on your hits, knockouts and when you are hit." } },
    { { ROW_ROM, "ROM", 0, GAME_SOR == 3 ? "Your Streets of Rage 3 (USA) cartridge file." : GAME_SOR == 2 ? "Your Streets of Rage 2 (USA) cartridge file."
                                                                                        : "Your Streets of Rage cartridge file." },
      { ROW_OPTION, "Player 2", OPT_PLAYER2, GAME_SOR == 2 ? "In a 2 PLAYERS game: a friend on a second controller, or the AI partner beside you."
                                                            : "A friend on a second controller, or the AI partner fighting beside you." },
      { ROW_OPTION, "High frame rate", OPT_SMOOTH, "Draws at your screen's refresh rate (120, 144 Hz...): movement glides between the game's 60 updates." },
      { ROW_OPTION, "Sprite flicker fix", OPT_FLICKER, "Lifts the hardware's sprites-per-line limit, so nothing flickers." },
      { ROW_OPTION, "Slowdown fix", OPT_OVERCLOCK, "A faster 68000, so crowded fights no longer slow down." },
      { ROW_OPTION, "Rewind", OPT_REWIND_ALL, "Hold the rewind key to step back in time." },
      { ROW_CONTROLS, "Controls", 0, "Keyboard keys and controller buttons for every action." },
      { ROW_OPTION, "Show launcher at startup", OPT_LAUNCHER, "Off starts the game directly (show_launcher=1 in settings.ini brings it back)." } },
    { { ROW_OPTION, "Launcher music", OPT_MUSIC, NULL }, { ROW_TRACK, "Track", 0, NULL },
      { ROW_OPTION, "Player", OPT_OST_MODE, NULL }, { ROW_OPTION, "Volume", OPT_VOLUME, NULL },
      { ROW_OPTION, "Audio", OPT_AUDIO, NULL } },
#if GAME_SOR == 3
    { { ROW_FIGHTER, "Axel", 0, "His top, hair, trousers, gloves and skin in any colour, still or animated." },
      { ROW_FIGHTER, "Blaze", 1, "Her outfit, hair and skin in any colour, still or animated." },
      { ROW_FIGHTER, "Skate", 2, "His top, cap, shorts, skates and skin in any colour, still or animated." },
      { ROW_FIGHTER, "Zan", 3, "His jacket, trousers, shoes and metal head in any colour, still or animated." } },
    { { ROW_OPTION, "Infinite health", OPT_CHEAT_HEALTH, "Both players take no damage from enemies." },
      { ROW_OPTION, "Infinite lives", OPT_CHEAT_LIVES, "Both players always have at least two lives left." },
      { ROW_OPTION, "Attack power", OPT_CHEAT_ATTACK, "Double or quadruple damage, or every enemy falls to one hit." } },
};
static const int tab_count[TABS] = { 9, 4, 8, 5, 4, 3 };
#elif GAME_SOR == 2
    { { ROW_FIGHTER, "Max", 0, "His trousers, boots, belt and skin in any colour, still or animated." },
      { ROW_FIGHTER, "Axel", 1, "His hair, shirt, jeans, gloves and skin in any colour, still or animated." },
      { ROW_FIGHTER, "Blaze", 2, "Her outfit, trim, hair and skin in any colour, still or animated." },
      { ROW_FIGHTER, "Skate", 3, "His top, cap, shorts and skin in any colour, still or animated." } },
    { { ROW_OPTION, "Infinite health", OPT_CHEAT_HEALTH, "Both players take no damage from enemies." },
      { ROW_OPTION, "Infinite lives", OPT_CHEAT_LIVES, "Both players always have at least three lives." },
      { ROW_OPTION, "Free specials", OPT_CHEAT_SPECIALS, "Special moves cost no health when they land." },
      { ROW_OPTION, "Infinite time", OPT_CHEAT_TIME, "The round timer never runs down." },
      { ROW_OPTION, "Attack power", OPT_CHEAT_ATTACK, "Double or quadruple damage, or every enemy falls to one hit." } },
};
static const int tab_count[TABS] = { 9, 4, 8, 5, 4, 5 };
#else
    { { ROW_FIGHTER, "Axel", 0, "His hair, top, jeans and skin in any colour, still or animated." },
      { ROW_FIGHTER, "Adam", 1, "His vest, trousers, skin and shoes in any colour, still or animated." },
      { ROW_FIGHTER, "Blaze", 2, "Her outfit, hair and skin in any colour, still or animated." } },
    { { ROW_OPTION, "Infinite health", OPT_CHEAT_HEALTH, "Both players take no damage from enemies." },
      { ROW_OPTION, "Infinite lives", OPT_CHEAT_LIVES, "Both players always have at least three lives." },
      { ROW_OPTION, "Infinite specials", OPT_CHEAT_SPECIALS, "The police car can always be called." },
      { ROW_OPTION, "Infinite time", OPT_CHEAT_TIME, "The round timer never runs down." },
      { ROW_OPTION, "Attack power", OPT_CHEAT_ATTACK, "Double or quadruple damage, or every enemy falls to one hit." } },
};
static const int tab_count[TABS] = { 9, 4, 8, 5, 3, 5 };
#endif

/* ---- Look: the game's HUD, a yellow frame on charcoal, red bars ---------------------- */
#include "theme.h"
#define HEAD     "Impact"                /* the game's lettering is a heavy condensed face */

/* ---- Layout (1280x720 units) ------------------------------------------------------- */
#define CASE_X 60                       /* the title screen, framed */
#define CASE_Y 52
#define CASE_W 448
#define CASE_H (CASE_W * 224 / 320)
#define BOX_X  560                      /* the settings panel */
#define BOX_Y  28
#define BOX_W  680
#define BOX_H  664
#define PANEL_X (BOX_X + 40)
#define PANEL_R (BOX_X + BOX_W - 44)
#define TAB_Y   (BOX_Y + 26)
#define TAB_H   46
#define TAB_X0  (BOX_X + 26)
#define TAB_W   ((BOX_W - 52) / TABS)
#define ROWS_Y  (TAB_Y + TAB_H + 24)
#define ROW_H   40
#define CARD_Y  (ROWS_Y + 5 * ROW_H + 14)
#define CARD_H  160
#define BTN_Y   (BOX_Y + BOX_H - 118)
#define BTN_H   54
#define PLAY_X  PANEL_X
#define PLAY_W  330
#define QUIT_X  (PANEL_R - 150)
#define QUIT_W  150
#define VOLUME_W 150
#define VOLUME_X (PANEL_R - 22 - 52 - 26 - VOLUME_W)
#define PLAYER_BTN_Y (CARD_Y + 112)

/* Selection: the tab bar, a row of the open tab, or a button. */
enum { ITEM_TABS = -1, ITEM_PLAY = 100, ITEM_QUIT = 101 };
enum { HIT_PREV = 200, HIT_TOGGLE, HIT_NEXT, HIT_TAB0 = 300 };

static struct {
    int tab, selected;
    float tab_t0;                       /* when the tab last changed, and from where */
    int tab_from;
    float hl_y;                         /* the highlight, gliding to the selected row */
    float changed_t0;                   /* when a value last changed (it pulses) */
    float selected_t0;                  /* when the selection last moved (the preview fades in) */
    int changed_item;
    float flash;                        /* the frame's flare on a choice, fading */
    float t;
} ui;

static bool play_enabled(void) { return rom_ok; }
static char rom_status[96];

static float row_y(int r) { return ROWS_Y + r * ROW_H; }

static int hit_test(float mx, float my) {
    for (int k = 0; k < TABS; k++)
        if (mx >= TAB_X0 + k * TAB_W && mx < TAB_X0 + (k + 1) * TAB_W && my >= TAB_Y && my < TAB_Y + TAB_H) return HIT_TAB0 + k;
    for (int r = 0; r < tab_count[ui.tab]; r++) {
        float y = row_y(r);
        if (mx >= PANEL_X - 14 && mx <= PANEL_R + 8 && my >= y && my < y + ROW_H) return r;
    }
    if (ui.tab == 3 && my >= PLAYER_BTN_Y - 20 && my <= PLAYER_BTN_Y + 20) {
        float cx = (PANEL_X + PANEL_R) / 2.f;
        if (fabsf(mx - (cx - 70)) < 24) return HIT_PREV;
        if (fabsf(mx - cx) < 28) return HIT_TOGGLE;
        if (fabsf(mx - (cx + 70)) < 24) return HIT_NEXT;
    }
    if (my >= BTN_Y && my <= BTN_Y + BTN_H) {
        if (mx >= PLAY_X && mx <= PLAY_X + PLAY_W) return ITEM_PLAY;
        if (mx >= QUIT_X && mx <= QUIT_X + QUIT_W) return ITEM_QUIT;
    }
    return -2;
}

static void select_item(int item) {
    if (item == ui.selected) return;
    ui.selected = item;
    ui.selected_t0 = ui.t;
    music_sfx(SFX_CURSOR);
}

static void move_selection(int dir) {
    int order[16], n = 0, at = 0;
    order[n++] = ITEM_TABS;
    for (int r = 0; r < tab_count[ui.tab]; r++) order[n++] = r;
    order[n++] = ITEM_PLAY; order[n++] = ITEM_QUIT;
    for (int i = 0; i < n; i++) if (order[i] == ui.selected) at = i;
    select_item(order[settings_cycle(at, n, dir)]);
}

static void open_tab(int tab) {
    tab = (tab % TABS + TABS) % TABS;
    if (tab == ui.tab) return;
    ui.tab_from = ui.tab;
    ui.tab = tab;
    ui.tab_t0 = ui.t;
    ui.selected_t0 = ui.t;
    if (ui.selected >= 0 && ui.selected < 100 && ui.selected >= tab_count[tab]) ui.selected = tab_count[tab] - 1;
    ui.flash = 1;
    music_sfx(SFX_CURSOR);
}

/* What the player is doing (a copy, taken under its lock). */
typedef struct { bool ready, on; int song, frames; float meter[METER_BARS]; } PlayerView;

static PlayerView player_view(void) {
    PlayerView v = { 0 };
    if (!music_lock || !music_thread) return v;
    SDL_LockMutex(music_lock);
    v.ready = player.ready; v.on = player.on; v.song = player.song; v.frames = player.frames;
    memcpy(v.meter, player.meter, sizeof v.meter);
    SDL_UnlockMutex(music_lock);
    return v;
}

static void song_name(int song, char *out, size_t n) {
    const char *name = ost_song_name(song);
    if (name) snprintf(out, n, "%s", name);
    else snprintf(out, n, "Track %02d", song + 1);
}

/* ---- Drawing ----------------------------------------------------------------------- */
static float ease_out(float x) { x = clampf(x, 0, 1); return 1 - (1 - x) * (1 - x) * (1 - x); }

/* A filled triangle (layout units), antialiased by 2x2 supersampling. */
static void fill_tri(float ax, float ay, float bx, float by, float cx, float cy, Rgb c, float a) {
    float x0 = fminf(ax, fminf(bx, cx)), x1 = fmaxf(ax, fmaxf(bx, cx)), y0 = fminf(ay, fminf(by, cy)), y1 = fmaxf(ay, fmaxf(by, cy));
    int px0 = (int)ui_x(x0) - 1, px1 = (int)ui_x(x1) + 1, py0 = (int)ui_y(y0) - 1, py1 = (int)ui_y(y1) + 1;
    for (int y = py0; y <= py1; y++)
        for (int x = px0; x <= px1; x++) {
            int in = 0;
            for (int s = 0; s < 4; s++) {
                float ux = (x + .25f + (s & 1) * .5f - ui_ox) / ui_s, uy = (y + .25f + (s >> 1) * .5f - ui_oy) / ui_s;
                float e0 = (bx - ax) * (uy - ay) - (by - ay) * (ux - ax);
                float e1 = (cx - bx) * (uy - by) - (cy - by) * (ux - bx);
                float e2 = (ax - cx) * (uy - cy) - (ay - cy) * (ux - cx);
                in += (e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0);
            }
            if (in) ui_blend_px(x, y, c, a * in / 4.f);
        }
}

/* A rectangular frame of `thick` units inside (x, y, w, h). */
static void frame_rect(float x, float y, float w, float h, float thick, Rgb c, float a) {
    ui_fill_rect(x, y, w, thick, c, a);
    ui_fill_rect(x, y + h - thick, w, thick, c, a);
    ui_fill_rect(x, y + thick, thick, h - 2 * thick, c, a);
    ui_fill_rect(x + w - thick, y + thick, thick, h - 2 * thick, c, a);
}

/* The HUD box: charcoal with a yellow frame and a dark line inside it. */
static void hud_box(float x, float y, float w, float h, float alpha, float glow) {
    ui_fill_rect(x, y, w, h, C_INK, .9f * alpha);
    frame_rect(x, y, w, h, 4, mix(C_YELLOW, rgb(1, 1, 1), glow * .5f), alpha);
    frame_rect(x + 4, y + 4, w - 8, h - 8, 2, rgb(0, 0, 0), .8f * alpha);
}

/* Text with the game's hard drop shadow. */
static void shadow_text(const char *s, const char *face, float px, int weight, float tracking, float x, float y,
                        Rgb c, Rgb shade, float alpha) {
    float d = px / 16;
    ui_draw_text(s, face, px, weight, tracking, x + d, y + d, shade, shade, alpha * .9f);
    ui_draw_text(s, face, px, weight, tracking, x, y, c, mix(c, C_AMBER, .35f), alpha);
}

/* An Art scaled to (x, y, w, h) in layout units, nearest pixel, from its columns
 * src_x .. src_x + src_w; `bright` darkens it. */
static void draw_art(const Art *a, float x, float y, float w, float h, float src_x, float src_w, float bright) {
    if (!a->w) return;
    int px0 = (int)ui_x(x), px1 = (int)ui_x(x + w), py0 = (int)ui_y(y), py1 = (int)ui_y(y + h);
    if (px0 < 0) px0 = 0;
    if (py0 < 0) py0 = 0;
    if (px1 > ui_w) px1 = ui_w;
    if (py1 > ui_h) py1 = ui_h;
    float sx = src_w / (ui_x(x + w) - ui_x(x)), sy = a->h / (ui_y(y + h) - ui_y(y));
    int k = (int)(bright * 256);
    for (int py = py0; py < py1; py++) {
        int ay = (int)((py - ui_y(y)) * sy);
        if (ay < 0 || ay >= a->h) continue;
        uint32_t *out = ui_px + (size_t)py * ui_w;
        const uint32_t *row = a->rgb + (size_t)ay * a->w;
        for (int px = px0; px < px1; px++) {
            int ax = (int)(src_x + (px - ui_x(x)) * sx);
            if (ax < 0 || ax >= a->w) continue;
            uint32_t c = row[ax];
            out[px] = ((((c >> 16) & 255) * k >> 8) << 16) | ((((c >> 8) & 255) * k >> 8) << 8) | ((c & 255) * k >> 8);
        }
    }
}

/* Behind everything: the street at night, panning slowly to and fro, with a red haze
 * rising from the bottom and faint scanlines. */
static void draw_backdrop(float t) {
    for (size_t i = 0; i < (size_t)ui_w * ui_h; i++) ui_px[i] = 0x07060A;
    if (pano.w) {
        float view = pano.h * (float)UI_BASE_W / UI_BASE_H;          /* street pixels across */
        float span = pano.w - view;
        float at = span > 0 ? (.5f - .5f * cosf(t * 6.2831853f / 120)) * span : 0;
        draw_art(&pano, 0, 0, UI_BASE_W, UI_BASE_H, at, span > 0 ? view : (float)pano.w, .42f);
    }
    for (int y = 0; y < ui_h; y++) {
        float v = (float)y / ui_h;
        float haze = smooth(.45f, 1, v) * .28f, dark = (1 - smooth(0, .35f, v)) * .35f;
        bool line = (y / (ui_s > 1.5f ? 3 : 2)) % 2;
        uint32_t *out = ui_px + (size_t)y * ui_w;
        for (int x = 0; x < ui_w; x++) {
            Rgb c = unpack(out[x]);
            float edge = fabsf((float)x / ui_w - .5f) * 2;
            c = scale(c, (1 - dark) * (1 - edge * edge * .45f) * (line ? .9f : 1));
            c = add(c, rgb(.85f, .12f, .06f), haze);
            out[x] = pack(c);
        }
    }
}

static void draw_tabs(void) {
    for (int k = 0; k < TABS; k++) {
        float x = TAB_X0 + k * TAB_W + 4, w = TAB_W - 8, cx = x + w / 2.f;
        bool open = ui.tab == k, focus = ui.selected == ITEM_TABS && open;
        if (open) {
            ui_fill_rect(x, TAB_Y, w, TAB_H, C_YELLOW, 1);
            ui_fill_rect(x, TAB_Y + TAB_H - 4, w, 4, C_RED, 1);
            if (focus) frame_rect(x - 3, TAB_Y - 3, w + 6, TAB_H + 6, 2, rgb(1, 1, 1), .5f + .3f * sinf(ui.t * 6));
        } else {
            ui_fill_rect(x, TAB_Y, w, TAB_H, rgb_hex(0x24242B), .9f);
            frame_rect(x, TAB_Y, w, TAB_H, 1, rgb_hex(0x4A4A52), 1);
        }
        Rgb tc = open ? C_INK : C_DIM;
        float tw = ui_text_width(tab_names[k], HEAD, 21, 400, 2);
        ui_draw_text(tab_names[k], HEAD, 21, 400, 2, cx - tw / 2, TAB_Y + 10, tc, tc, 1);
    }
}

static void draw_row(int r, const Row *row, float dx, float alpha, const PlayerView *pv) {
    float y = row_y(r);
    bool focus = ui.selected == r;
    Rgb lc = focus ? rgb(1, 1, 1) : C_TEXT;
    ui_draw_text(row->label, "Segoe UI", 17.5f, 400, 0, PANEL_X + 4 + dx, y + 10, lc, lc, alpha);
    char value[96];
    Rgb vc = focus ? C_YELLOW : C_VALUE;
    const char *hint = NULL;
    switch (row->kind) {
    case ROW_ROM:
        snprintf(value, sizeof value, "%s", rom_status);
        vc = rom_ok ? rgb_hex(0x9FDCA6) : rgb_hex(0xF09A86);
        hint = "Enter to choose\xE2\x80\xA6";
        break;
    case ROW_CONTROLS:
        snprintf(value, sizeof value, "Keyboard & controller");
        hint = "Enter to change\xE2\x80\xA6";
        break;
    case ROW_OUTFIT:
        snprintf(value, sizeof value, "-");
        break;
    case ROW_FIGHTER: {
        int changed = 0;
        for (int k = 0; k < FIGHTER_PARTS; k++) changed += settings.fighter_color[row->id][k] >= 0;
        if (!changed) snprintf(value, sizeof value, "Their own colours");
        else snprintf(value, sizeof value, "%d part%s recoloured", changed, changed == 1 ? "" : "s");
        hint = "Enter to change\xE2\x80\xA6";
        break;
    }
    case ROW_TRACK:
        song_name(pv->ready ? pv->song : (settings.ost_track >= 0 ? settings.ost_track : title_song >= 0 ? title_song : 0), value, sizeof value);
        hint = pv->on ? "Enter to stop" : "Enter to play";
        break;
    default:
        settings_option_text(row->id, value, sizeof value);
    }
    float pulse = ui.changed_item == r ? 1 - ease_out((ui.t - ui.changed_t0) / .35f) : 0;
    if (pulse > 0) vc = mix(vc, rgb(1, 1, 1), pulse * .8f);
    float px = 17.5f + pulse * 1.5f;
    float vw = ui_text_width(value, "Segoe UI Semibold", px, 600, 0);
    float vx = PANEL_R - vw;
    bool arrows = row->kind == ROW_OPTION || row->kind == ROW_TRACK;
    if (row->kind == ROW_OPTION && row->id == OPT_VOLUME) {
        vw = ui_text_width("100%", "Segoe UI Semibold", px, 600, 0);
        vx = PANEL_R - 22 - vw;
        ui_slider(VOLUME_X + dx, y + ROW_H / 2.f, VOLUME_W, settings.volume / 100.f, focus);
    } else if (arrows) {
        vx -= 22;
    }
    if (focus && hint) {
        float hw = ui_text_width(hint, "Segoe UI", 13, 400, 0);
        ui_draw_text(hint, "Segoe UI", 13, 400, 0, vx - hw - (arrows ? 34 : 16) + dx, y + 13, C_DIM, C_DIM, alpha);
    }
    if (arrows) {
        Rgb ac = focus ? C_YELLOW : rgb_hex(0x5E5A52);
        float nudge = pulse * 3;
        fill_tri(vx - 18 - nudge + dx, y + ROW_H / 2.f, vx - 10 - nudge + dx, y + ROW_H / 2.f - 6, vx - 10 - nudge + dx, y + ROW_H / 2.f + 6, ac, alpha);
        fill_tri(PANEL_R + 4 + nudge + dx, y + ROW_H / 2.f, PANEL_R - 4 + nudge + dx, y + ROW_H / 2.f - 6, PANEL_R - 4 + nudge + dx, y + ROW_H / 2.f + 6, ac, alpha);
        vx -= 0;
    }
    ui_draw_text(value, "Segoe UI Semibold", px, 600, 0, vx - (arrows ? 6 : 0) + dx, y + 10 - pulse * .8f, vc, vc, alpha);
}

/* The soundtrack player: the song, its time, a level meter and the transport buttons. */
static void draw_player(const PlayerView *pv, float dx, float alpha) {
    float x = PANEL_X - 6 + dx, w = PANEL_R - PANEL_X + 12, y = CARD_Y;
    ui_fill_rect(x, y, w, CARD_H, rgb_hex(0x0C0C10), .85f * alpha);
    frame_rect(x, y, w, CARD_H, 2, C_RED, .8f * alpha);
    char name[64], time[48];
    int song = pv->ready ? pv->song : (settings.ost_track >= 0 ? settings.ost_track : title_song >= 0 ? title_song : 0);
    song_name(song, name, sizeof name);
    ui_draw_text(pv->on ? "NOW PLAYING" : "STOPPED", "Segoe UI Semibold", 11, 600, 3, x + 20, y + 16, C_RED, C_RED, alpha);
    shadow_text(name, HEAD, 30, 400, 1, x + 20, y + 30, C_YELLOW, C_DEEP, alpha);
    char count[32];
    snprintf(count, sizeof count, "%d / %d", (song < 0 ? 0 : song) + 1, OST_SONGS);
    float cw = ui_text_width(count, "Segoe UI", 14, 400, 0);
    ui_draw_text(count, "Segoe UI", 14, 400, 0, x + w - 20 - cw, y + 20, C_DIM, C_DIM, alpha);
    /* Level meter, right of the name: red at rest, yellow at the peaks. */
    float mx0 = x + w - 20 - METER_BARS * 7, my = y + 72;
    for (int b = 0; b < METER_BARS; b++) {
        float h = 3 + pv->meter[b] * 26;
        Rgb c = mix(C_RED, C_YELLOW, pv->meter[b]);
        ui_fill_rect(mx0 + b * 7, my - h, 4, h, c, alpha * (.4f + .6f * pv->meter[b]));
    }
    /* Progress through the song's turn, and the time. */
    int secs = pv->frames / 60;
    snprintf(time, sizeof time, "%d:%02d", secs / 60, secs % 60);
    float bar_x = x + 20, bar_w = w - 40, bar_y = y + 86;
    float prog = clampf(pv->frames / (PLAY_FOR * 60.f), 0, 1);
    ui_fill_rect(bar_x, bar_y, bar_w, 4, rgb_hex(0x34343C), alpha);
    ui_fill_rect(bar_x, bar_y, bar_w * prog, 4, C_YELLOW, alpha);
    ui_draw_text(time, "Segoe UI", 12, 400, 0, bar_x, bar_y + 9, C_DIM, C_DIM, alpha);
    /* Previous, play/stop, next. */
    float cx = (PANEL_X + PANEL_R) / 2.f + dx, by = PLAYER_BTN_Y;
    Rgb bc = C_TEXT;
    fill_tri(cx - 62, by, cx - 50, by - 9, cx - 50, by + 9, bc, alpha * .85f);
    fill_tri(cx - 74, by, cx - 62, by - 9, cx - 62, by + 9, bc, alpha * .85f);
    fill_tri(cx + 62, by, cx + 50, by - 9, cx + 50, by + 9, bc, alpha * .85f);
    fill_tri(cx + 74, by, cx + 62, by - 9, cx + 62, by + 9, bc, alpha * .85f);
    if (pv->on) {
        ui_fill_rect(cx - 9, by - 10, 6, 20, C_YELLOW, alpha);
        ui_fill_rect(cx + 3, by - 10, 6, 20, C_YELLOW, alpha);
    } else {
        fill_tri(cx - 8, by - 12, cx + 13, by, cx - 8, by + 12, C_YELLOW, alpha);
    }
}

/* PLAY in red with a yellow frame (like the HUD's health bar), QUIT in charcoal. */
static void hud_button(float x, float y, float w, float h, const char *label, bool primary, bool focus, bool enabled, float t) {
    Rgb fill = primary ? (enabled ? C_RED : rgb_hex(0x4A2622)) : rgb_hex(0x23232A);
    ui_fill_rect(x, y, w, h, fill, .96f);
    if (primary && enabled) ui_fill_rect(x, y, w, h * .45f, rgb(1, 1, 1), .10f);
    Rgb border = focus ? C_YELLOW : primary ? C_AMBER : rgb_hex(0x56565E);
    frame_rect(x, y, w, h, focus ? 4 : 2, border, focus ? .75f + .25f * sinf(t * 6) : .9f);
    float tw = ui_text_width(label, HEAD, 28, 400, 3);
    Rgb tc = enabled ? (focus ? C_YELLOW : rgb(1, 1, 1)) : C_DIM;
    shadow_text(label, HEAD, 28, 400, 3, x + w / 2 - tw / 2, y + h / 2 - 17, tc, rgb(0, 0, 0), 1);
}

static void draw_ui(float t) {
    PlayerView pv = player_view();
    draw_tabs();
    /* The open tab slides in from the side it came from. */
    float k = ease_out((t - ui.tab_t0) / .32f);
    int dir = ui.tab > ui.tab_from ? 1 : -1;
    float dx = (1 - k) * 36 * dir, alpha = k;
    /* The highlight glides to the selected row: a red bar, like the health bar. */
    bool on_row = ui.selected >= 0 && ui.selected < tab_count[ui.tab];
    if (on_row) {
        float y = ui.hl_y;
        ui_fill_rect(PANEL_X - 14, y + 3, PANEL_R - PANEL_X + 26, ROW_H - 6, C_RED, .22f);
        ui_fill_rect(PANEL_X - 14, y + 3, 5, ROW_H - 6, C_YELLOW, 1);
    }
    for (int r = 0; r < tab_count[ui.tab]; r++) draw_row(r, &tab_rows[ui.tab][r], dx, alpha, &pv);
    if (ui.tab == 3) draw_player(&pv, dx, alpha);
    else if (on_row && tab_rows[ui.tab][ui.selected].about) {
        /* What the selected option does, under a short red rule. */
        float y = row_y(tab_count[ui.tab]) + 22;
        ui_fill_rect(PANEL_X, y, 46, 3, C_RED, .9f * alpha);
        ui_draw_text(tab_rows[ui.tab][ui.selected].about, "Segoe UI", 14.5f, 400, 0, PANEL_X + dx, y + 14,
                     rgb_hex(0xBDB7AA), rgb_hex(0xBDB7AA), alpha);
    }

    hud_button(PLAY_X, BTN_Y, PLAY_W, BTN_H, "PLAY", true, ui.selected == ITEM_PLAY, play_enabled(), t);
    hud_button(QUIT_X, BTN_Y, QUIT_W, BTN_H, launcher_in_collection ? "BACK" : "QUIT", false, ui.selected == ITEM_QUIT, true, t);
    const char *keys = "\xE2\x86\x91\xE2\x86\x93 select    \xE2\x86\x90\xE2\x86\x92 change    Q E  tabs    [ ]  track    M  music    Esc  quit";
    ui_draw_text(keys, "Segoe UI", 12, 400, 1, PANEL_X, BOX_Y + BOX_H - 44, rgb_hex(0x77736A), rgb_hex(0x77736A), 1);

    /* A picture option: its before/after on the last frame the game saved, over the cover. */
    if (on_row && tab_rows[ui.tab][ui.selected].kind == ROW_OPTION && preview_supported(tab_rows[ui.tab][ui.selected].id) &&
        preview_ready()) {
        float a = ease_out((t - ui.selected_t0) / .3f);
        float px = 40, py = 226, pw = 480, ph = pw * 9 / 16.f;
        ui_fill_rect(0, 0, BOX_X - 16, UI_BASE_H, rgb(0, 0, 0), .55f * a);
        hud_box(px - 16, py - 48, pw + 32, ph + 70, a, 0);
        ui_draw_text("PREVIEW  \xC2\xB7  YOUR LAST GAME FRAME", "Segoe UI Semibold", 12, 600, 3, px, py - 30, C_YELLOW, C_YELLOW, a);
        if (a > .05f) preview_draw(tab_rows[ui.tab][ui.selected].id, px, py, pw, ph, t);
    }

    /* A fighter's row: the fighter in their colours, over the cover. */
    if (on_row && tab_rows[ui.tab][ui.selected].kind == ROW_FIGHTER && fighters_have_preview(tab_rows[ui.tab][ui.selected].id)) {
        float a = ease_out((t - ui.selected_t0) / .3f);
        ui_fill_rect(CASE_X - 8, CASE_Y - 8, CASE_W + 16, CASE_H + 16, rgb_hex(0x0C0C10), .92f * a);
        fighters_draw(tab_rows[ui.tab][ui.selected].id, CASE_X + CASE_W / 2.f, CASE_Y + CASE_H - 14, 2.8f, a, t);
    }

    /* Under the cover, on every tab: what is playing. */
    if (pv.ready && pv.on) {
        char name[64], line[96];
        song_name(pv.song, name, sizeof name);
        snprintf(line, sizeof line, "\xE2\x99\xAA  %s", name);
        float w = ui_text_width(line, "Segoe UI Semibold", 15, 600, 1);
        ui_draw_text(line, "Segoe UI Semibold", 15, 600, 1, CASE_X + CASE_W / 2.f - w / 2, 640, C_YELLOW, C_AMBER, .9f);
    }
}

/* ---- Frame -------------------------------------------------------------------- */
static void layout(int pixel_w) {
    ui_begin(pixel_w, (int)(BASE_H * (pixel_w / (float)BASE_W) + .5f));
}

/* Advances the animations by dt seconds. */
static void animate(float dt) {
    float k = 1 - expf(-dt * 10);
    float hy = ui.selected >= 0 && ui.selected < tab_count[ui.tab] ? row_y(ui.selected) : ui.hl_y;
    ui.hl_y += (hy - ui.hl_y) * k;
    ui.flash *= expf(-dt * 3);
}

/* The left side: the title screen in a HUD frame, the name in the game's lettering. */
static void draw_cover(float t) {
    float x = CASE_X, y = CASE_Y, w = CASE_W, h = CASE_H;
    ui_fill_rect(x + 10, y + 12, w + 8, h + 8, rgb(0, 0, 0), .5f);       /* shadow */
    hud_box(x - 8, y - 8, w + 16, h + 16, 1, ui.flash * .6f);
    if (cover.w) draw_art(&cover, x, y, w, h, 0, (float)cover.w, 1);
    else ui_fill_rect(x, y, w, h, rgb_hex(0x101018), 1);
    float ty = y + h + 34;
    const char *name = GAME_NAME;
    float size = GAME_SOR >= 2 ? 46 : 54;
    float nw = ui_text_width(name, HEAD, size, 400, 2);
    shadow_text(name, HEAD, size, 400, 2, x + w / 2 - nw / 2, ty + (54 - size) / 2, C_YELLOW, C_DEEP, 1);
    ui_fill_rect(x + 60, ty + 78, w - 120, 3, C_RED, .9f);
    const char *sub = "NATIVE PC EDITION";
    float sw = ui_text_width(sub, "Segoe UI Semibold", 14, 600, 6);
    ui_draw_text(sub, "Segoe UI Semibold", 14, 600, 6, x + w / 2 - sw / 2, ty + 92, C_TEXT, C_DIM, .9f);
    /* A neon flicker along the red rule, now and then. */
    float flick = sinf(t * 1.7f) > .97f ? .5f : 0;
    if (flick > 0) ui_fill_rect(x + 60, ty + 78, w - 120, 3, rgb(1, .6f, .4f), flick);
}

static void render(float t) {
    ui.t = t;
    draw_backdrop(t);
    draw_cover(t);
    hud_box(BOX_X, BOX_Y, BOX_W, BOX_H, 1, ui.flash * .4f);
    draw_ui(t);
}

static void update_rom_status(void) {
    if (rom_ok) snprintf(rom_status, sizeof rom_status, "Verified \xE2\x9C\x93");
    else if (!settings.rom_path[0]) snprintf(rom_status, sizeof rom_status, "Not selected");
    else snprintf(rom_status, sizeof rom_status, "Not Streets of Rage (World)");
}

static void init_common(void) {
    char *bp = SDL_GetBasePath();
    snprintf(cache_path, sizeof cache_path, "%slauncher_art.cache", bp ? bp : "");
    settings_set_base(bp ? bp : "");
    art_prepare();                      /* (may run the intro, which draws over hd_frame) */
    fighters_preview_load(bp ? bp : "");
    preview_load(bp ? bp : "");
    if (bp) SDL_free(bp);
    update_rom_status();
}

static void art_write_png(const Art *a, const char *base, const char *suffix) {
    if (!a->w) return;
    char p[1200];
    snprintf(p, sizeof p, "%s%s", base, suffix);
    png_write(p, a->rgb, a->w, a->h, a->w);
}

int launcher_screenshot(const char *png, int width, int tab, float t) {
    init_common();
    art_write_png(&cover, png, ".cover.png");
    art_write_png(&pano, png, ".street.png");
    layout(width);
    ui.tab = ui.tab_from = (tab % TABS + TABS) % TABS;
    ui.selected = tab_count[ui.tab] > 1 ? 1 : 0;
    ui.hl_y = row_y(ui.selected);
    ui.tab_t0 = -10;
    render(t);                          /* builds the surfaces and the backdrop: not timed */
    Uint64 t0 = SDL_GetPerformanceCounter();
    for (int i = 0; i < 10; i++) render(t);
    printf("launcher frame: %.1f ms at %dx%d\n", (SDL_GetPerformanceCounter() - t0) * 100.0 / SDL_GetPerformanceFrequency(), ui_w, ui_h);
    return png_write(png, ui_px, ui_w, ui_h, ui_w);
}

/* ---- ROM picker ------------------------------------------------------------------ */
static void choose_rom(SDL_Window *win) {
#ifdef _WIN32
    wchar_t file[1024] = L"";
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof ofn;
    (void)win;
    ofn.lpstrFilter = L"Genesis / Mega Drive ROM (*.md;*.bin;*.gen;*.smd)\0*.md;*.bin;*.gen;*.smd\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.lpstrTitle = L"Choose your Streets of Rage ROM";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return;
    char path[1024];
    WideCharToMultiByte(CP_UTF8, 0, file, -1, path, sizeof path, NULL, NULL);
    for (char *p = path; *p; p++) if (*p == '\\') *p = '/';
    snprintf(settings.rom_path, sizeof settings.rom_path, "%s", path);
    remove(cache_path);
    music_stop();
    art_prepare();
    update_rom_status();
    music_start();
#else
    (void)win;
#endif
}

/* ---- Interactive loop --------------------------------------------------------------- */
/* Changes the selected row's value (dir -1/+1), or activates it (dir 0). */
static void row_action(SDL_Window *win, int dir) {
    const Row *row = &tab_rows[ui.tab][ui.selected];
    switch (row->kind) {
    case ROW_ROM: if (!dir) { music_sfx(SFX_CONFIRM); choose_rom(win); } return;
    case ROW_CONTROLS: if (!dir) { music_sfx(SFX_CONFIRM); controls_open(); } return;
    case ROW_FIGHTER: if (!dir) { music_sfx(SFX_CONFIRM); fighters_open(row->id); } return;
    case ROW_OUTFIT:
        if (!dir) {
            char *bp = SDL_GetBasePath();
            outfit_preview_load(bp ? bp : "");
            if (bp) SDL_free(bp);
            music_sfx(SFX_CONFIRM);
            outfit_open();
        }
        return;
    case ROW_TRACK: {
        PlayerView pv = player_view();
        if (dir) music_request(-1, dir, false, false);
        else music_request(-1, 0, !pv.on, pv.on);
        if (dir || !pv.on) settings.launcher_music = true;
        break;
    }
    default:
        if (row->id == OPT_VOLUME && !dir) return;
        settings_option_change(row->id, dir ? dir : 1);
        if (row->id == OPT_MUSIC) music_request(-1, 0, settings.launcher_music, !settings.launcher_music);
    }
    ui.changed_item = ui.selected;
    ui.changed_t0 = ui.t;
    ui.flash = 1;
    music_sfx(SFX_CONFIRM);
}

static void launcher_outfit_sound(int kind) { music_sfx(kind == 0 ? SFX_CURSOR : kind == 1 ? SFX_CONFIRM : SFX_CANCEL); }

int launcher_run(const char *ini_path) {
    init_common();
    SDL_Window *win = SDL_CreateWindow(GAME_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, BASE_W, BASE_H,
                                       SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) return 1;
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    layout(ow > 1920 ? 1920 : ow);      /* drawn in software: past 1080p the GPU scales it up */
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, ui_w, ui_h);
    memset(&ui, 0, sizeof ui);
    outfit_sound = launcher_outfit_sound;
    ui.tab_t0 = -10;
    ui.changed_item = -1;
    ui.selected = ITEM_PLAY;
    if (!rom_ok) { ui.tab = ui.tab_from = 2; ui.selected = 0; }   /* the ROM row */
    ui.hl_y = row_y(ui.selected >= 0 && ui.selected < 100 ? ui.selected : 0);
    music_start();

    Uint64 freq = SDL_GetPerformanceFrequency(), start = SDL_GetPerformanceCounter(), last = start;
    int result = -1;
    while (result < 0) {
        Uint64 now = SDL_GetPerformanceCounter();
        float t = (float)(now - start) / freq, dt = (float)(now - last) / freq;
        last = now;
        ui.t = t;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            pads_event(&e);
            int activate = 0, change = 0;
            if ((controls_is_open() || outfit_is_open() || fighters_is_open()) && e.type != SDL_QUIT) {
                /* A page takes all input while it is up (mouse points match the layout). */
                float mx = e.type == SDL_MOUSEMOTION ? e.motion.x : e.type == SDL_MOUSEBUTTONDOWN ? e.button.x : 0;
                float my = e.type == SDL_MOUSEMOTION ? e.motion.y : e.type == SDL_MOUSEBUTTONDOWN ? e.button.y : 0;
                if (outfit_is_open()) { outfit_event(&e, mx, my); continue; }
                if (fighters_is_open()) { fighters_event(&e, mx, my); continue; }
                controls_event(&e, mx, my);
                if (!controls_is_open()) music_sfx(SFX_CANCEL);
                continue;
            }
            switch (e.type) {
            case SDL_QUIT: result = 0; break;
            case SDL_MOUSEMOTION: {
                /* Mouse positions are in window points, which match the 1280x720 layout. */
                int h = hit_test(e.motion.x, e.motion.y);
                if (h >= HIT_TAB0) { if (ui.tab == h - HIT_TAB0) select_item(ITEM_TABS); }
                else if (h >= -1 && h < 200) select_item(h);
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                int h = hit_test(e.button.x, e.button.y);
                if (e.button.button == SDL_BUTTON_LEFT) {
                    if (h >= HIT_TAB0) { open_tab(h - HIT_TAB0); ui.selected = ITEM_TABS; }
                    else if (h == HIT_PREV || h == HIT_NEXT) { music_request(-1, h == HIT_NEXT ? 1 : -1, false, false); settings.launcher_music = true; music_sfx(SFX_CONFIRM); }
                    else if (h == HIT_TOGGLE) { PlayerView pv = player_view(); music_request(-1, 0, !pv.on, pv.on); settings.launcher_music = !pv.on; music_sfx(SFX_CONFIRM); }
                    else if (h >= 0 && h < 100 && tab_rows[ui.tab][h].kind == ROW_OPTION && tab_rows[ui.tab][h].id == OPT_VOLUME &&
                             e.button.x >= VOLUME_X - 8 && e.button.x <= VOLUME_X + VOLUME_W + 8) {
                        ui.selected = h;        /* click on the track: set the volume there */
                        settings_set_volume_fraction(ui_slider_value(VOLUME_X, VOLUME_W, (float)e.button.x));
                        music_sfx(SFX_CURSOR);
                    } else if (h >= 0 || h == ITEM_PLAY || h == ITEM_QUIT) { ui.selected = h; activate = 1; }
                } else if (e.button.button == SDL_BUTTON_RIGHT && h >= 0 && h < 100) {
                    ui.selected = h; change = -1;
                }
                break;
            }
            case SDL_KEYDOWN:
                switch (e.key.keysym.sym) {
                case SDLK_UP: case SDLK_w: move_selection(-1); break;
                case SDLK_DOWN: case SDLK_s: case SDLK_TAB: move_selection(1); break;
                case SDLK_LEFT: case SDLK_a: change = -1; break;
                case SDLK_RIGHT: case SDLK_d: change = 1; break;
                case SDLK_q: case SDLK_PAGEUP: open_tab(ui.tab - 1); break;
                case SDLK_e: case SDLK_PAGEDOWN: open_tab(ui.tab + 1); break;
                case SDLK_LEFTBRACKET: music_request(-1, -1, false, false); settings.launcher_music = true; break;
                case SDLK_RIGHTBRACKET: music_request(-1, 1, false, false); settings.launcher_music = true; break;
                case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: activate = 1; break;
                case SDLK_ESCAPE: result = 0; break;
                case SDLK_m:            /* music on/off */
                    settings.launcher_music = !settings.launcher_music;
                    music_request(-1, 0, settings.launcher_music, !settings.launcher_music);
                    break;
                }
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                switch (e.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP: move_selection(-1); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: move_selection(1); break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: change = -1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: change = 1; break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: open_tab(ui.tab - 1); break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: open_tab(ui.tab + 1); break;
                case SDL_CONTROLLER_BUTTON_A: activate = 1; break;
                case SDL_CONTROLLER_BUTTON_START: if (play_enabled()) result = 1; else music_sfx(SFX_ERROR); break;
                case SDL_CONTROLLER_BUTTON_B: result = 0; break;
                }
                break;
            }
            if (ui.selected == ITEM_TABS && change) open_tab(ui.tab + change);
            else if (ui.selected >= 0 && ui.selected < tab_count[ui.tab] && (change || activate)) row_action(win, change);
            else if (activate && ui.selected == ITEM_PLAY) {
                if (play_enabled()) { music_sfx(SFX_CONFIRM); result = 1; }
                else music_sfx(SFX_ERROR);
            }
            else if (activate && ui.selected == ITEM_QUIT) result = 0;
        }

        animate(dt);
        render(t);
        controls_render(t);
        outfit_render(t);
        fighters_render(t);
        SDL_UpdateTexture(tex, NULL, ui_px, ui_w * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    if (music_thread) {
        PlayerView pv = player_view();
        if (pv.ready) settings.ost_track = pv.song;
    }
    music_stop();
    settings_save(ini_path);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    return result;
}
