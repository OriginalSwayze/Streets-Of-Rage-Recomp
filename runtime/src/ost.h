/* The game's sound driver, played on its own (ost.c): songs for the launcher's soundtrack
 * player and the game's sound effects for its menus. */
#ifndef OST_H
#define OST_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"

/* Commands, the same for every driver: OST_PLAY(n) plays song n of the soundtrack
 * (0 .. OST_SONGS-1), $02xx-$04xx play a sound effect, 0 stops. */
#define OST_PLAY(song) ((uint16_t)(0x0100 | (song)))
#define OST_STOP 0

#if GAME_SOR == 3
/* Streets of Rage 3: its own driver at $1A8000 (requests $01xx songs). */
#define OST_SONGS 36
#define SFX_CURSOR  0x0342              /* a 4-frame tick, the game's most used */
#define SFX_CONFIRM 0x0341
#define SFX_CANCEL  0x0344
#define SFX_ERROR   0x0340
#elif GAME_SOR == 2
/* Streets of Rage 2: its own driver at $040000 (requests $01xx songs, $02xx-$07xx effects). */
#define OST_SONGS 26
/* Effects only, $02xx-$04xx ($01xx is the music's class, and OST_PLAY's): */
#define SFX_CURSOR  0x0424              /* short and soft: 7 frames (--sound-probe ... 0) */
#define SFX_CONFIRM 0x0429              /* the money pickup's chime */
#define SFX_CANCEL  0x0420
#define SFX_ERROR   0x0422
#elif GAME_SMPS
/* Streets of Rage: Sega's SMPS 68k driver (songs $81-$91, effects $A0-$DF). */
#define OST_SONGS 16
/* The option screens' own two ($A1 a move, $A5 a choice). Not $BD, the pause sound: it
 * silences the music, which the launcher's player then takes for the song's end. */
#define SFX_CURSOR  0x02A1
#define SFX_CONFIRM 0x02A5
#define SFX_CANCEL  0x02A6
#define SFX_ERROR   0x02A8
#else
/* Beyond Oasis: song numbers $00-$21. */
#define OST_SONGS 0x22
#define SFX_CURSOR  0x0256
#define SFX_CONFIRM 0x0347
#define SFX_CANCEL  0x03AA
#define SFX_ERROR   0x0253
#endif

/* menu_sounds.cache: this tag and the four effects' numbers, then each one's sound. */
#define MENU_SOUNDS_MAGIC 0x324E534Du   /* "MSN2" (2: the quieter FM mix) */

/* The song's name in the player, or NULL to number it. */
const char *ost_song_name(int song);
/* The player's song number for a driver request (a song's), or -1 for anything else. */
int ost_song_of_request(uint16_t request);
/* The song the title screen plays, or -1 if the driver does not say. */
int ost_title_song(void);

/* Parks the game (its code stops; the driver, the Z80 and the sound chips keep their
 * state). Call once the game has set the driver up, e.g. at the title screen. */
void ost_start(void);
void ost_end(void);                     /* removes the hooks; the machine needs a reset after */

/* Queues a driver command; false if the queue is full. */
bool ost_command(uint16_t cmd);
/* The driver's own request number, as the game passes it (tests: --sound-probe). */
bool ost_request(uint16_t id);

/* Before each frame: queues the driver's once-a-frame update (as the game's V blank
 * does) and starts the queued calls; they run at the start of the frame. */
void ost_frame(void);

#endif
