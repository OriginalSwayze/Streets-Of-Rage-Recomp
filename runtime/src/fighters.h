/* The fighters' colours (fighters.c): Axel's, Adam's and Blaze's clothes, hair and skin
 * in any of the Genesis' 512 colours, still or animated, for whichever player plays them,
 * and the page that edits them with a live preview (drawn over the ui canvas by the pause
 * menu and the launcher, like the controls page). */
#ifndef FIGHTERS_H
#define FIGHTERS_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "game.h"

/* Each game's playable fighters, and the tiles each player's sprite is drawn from (vdp.c
 * recolours a player's pixels by them: player 1's from FIGHTER_TILES_P1, player 2's from
 * FIGHTER_TILES_P2, up to FIGHTER_TILES_END). */
#if GAME_SOR == 3
#define FIGHTERS 4                      /* Axel, Blaze, Skate, Zan (object +$1F, from 1) */
#define FIGHTER_TILES_P1  0x4CE                 /* each player's +$0A */
#define FIGHTER_TILES_P2  0x519
#define FIGHTER_TILES_END 0x564
#elif GAME_SOR == 2
#define FIGHTERS 4                      /* Max, Axel, Blaze, Skate (the select screen's order) */
#define FIGHTER_TILES_P1  0x180
#define FIGHTER_TILES_P2  0x1D0
#define FIGHTER_TILES_END 0x220
#else
#define FIGHTERS 3                      /* Axel, Adam, Blaze: the game's own numbers (object +$50) */
#define FIGHTER_TILES_P1  0x580
#define FIGHTER_TILES_P2  0x5C0
#define FIGHTER_TILES_END 0x600
#endif
#define FIGHTERS_MAX 4                  /* settings.h's tables */
#define FIGHTER_PARTS 5                 /* parts per fighter, at most */

extern const char *const fighter_names[FIGHTERS];   /* "Axel", "Adam", "Blaze", ... */
/* The name shown for fighter f in this edition: Bare Knuckle III's Skate is Sammy. */
const char *fighter_name(int f);

/* settings.ini: fighter_axel=a,b,c,d (CRAM colours in hex, - = their own) and
 * fighter_axel_anims=0,2,... (OUTFIT_ANIM_*); likewise adam and blaze. */
void fighters_defaults(void);
bool fighters_ini_load(const char *key, const char *value);
void fighters_ini_save(FILE *f);

/* Fills the renderer's colour table for this frame: entries 0-15 for player 1's sprite
 * pixels and 16-31 for player 2's (OUTFIT_KEEP where the game's colour stays), from each
 * player's fighter and the settings. Returns nonzero if anything changes. */
int fighters_colours(uint16_t out[32]);

/* The previews: a still of each fighter, kept in fighters.cache next to the executable.
 * Streets of Rage takes them from the select screen while the launcher runs the game for its
 * art (fighters_capture_select, then fighters_capture_colours in a round, for the in-game
 * colours); the later games take each one the first time that fighter stands still in a
 * round (fighters_colours does it). */
void fighters_capture_select(void);
void fighters_capture_colours(void);
bool fighters_preview_save(const char *dir);
bool fighters_preview_load(const char *dir);
bool fighters_have_preview(int fighter);
/* Fighter f in the settings' colours, feet centred at (cx, bottom), `scale` layout units
 * per pixel. */
void fighters_draw(int f, float cx, float bottom, float scale, float alpha, double time_s);

/* The page, for fighter f. Returns true from fighters_event when Apply changed the
 * settings. (mx, my): the mouse in layout units. */
void fighters_open(int f);
bool fighters_is_open(void);
bool fighters_event(const SDL_Event *e, float mx, float my);
void fighters_render(double time_s);

#endif
