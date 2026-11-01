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

#define FIGHTERS 3                      /* Axel, Adam, Blaze: the game's own numbers (object +$50) */
#define FIGHTER_PARTS 4                 /* parts per fighter, at most */

extern const char *const fighter_names[FIGHTERS];   /* "Axel", "Adam", "Blaze" */

/* settings.ini: fighter_axel=a,b,c,d (CRAM colours in hex, - = their own) and
 * fighter_axel_anims=0,2,... (OUTFIT_ANIM_*); likewise adam and blaze. */
void fighters_defaults(void);
bool fighters_ini_load(const char *key, const char *value);
void fighters_ini_save(FILE *f);

/* Fills the renderer's colour table for this frame: entries 0-15 for player 1's sprite
 * pixels and 16-31 for player 2's (OUTFIT_KEEP where the game's colour stays), from each
 * player's fighter and the settings. Returns nonzero if anything changes. */
int fighters_colours(uint16_t out[32]);

/* The previews: a still of each fighter from the select screen, taken while the launcher
 * runs the game for its art (fighters_capture_select, then fighters_capture_colours in a
 * round, for the in-game colours), kept in fighters.cache next to the executable. */
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
