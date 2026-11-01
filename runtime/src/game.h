/* What this build knows about its game. The engine (recompiler, hardware, renderer, menus,
 * save states, rewind) works for any Genesis cartridge; these features read the game's own
 * RAM or call its own code, so each is on only once that game's addresses are known. */
#ifndef GAME_H
#define GAME_H

#define GAME_TITLE      "Streets of Rage"
#define GAME_ROM_NAME   "Streets of Rage (World)"
#define GAME_ROM_SHA1   "3d74dbc81f3472a5bde45bf265e636a72a314667"
#define GAME_ROM_SIZE   0x80000

/* The game's objects, health and items: built-in cheats, combat info, rumble, stats, the
 * map, the randomizer, spirit effects, the outfit, smooth motion and the camera. */
#define GAME_KNOWS_RAM  0
/* Widescreen: replaces the game's own scrolling and camera routines (widescreen.c). */
#define GAME_WIDESCREEN 0
/* A wider picture at all (Streets of Rage: its own patches, sor_wide.c). */
#define GAME_WIDE_VIEW  1
/* The launcher: its soundtrack player, menu sounds and art run the game's sound driver
 * and read its graphics (launcher.c, ost.c). */
#define GAME_LAUNCHER   1
/* The sound driver is Sega's SMPS 68k (ost.c), not Beyond Oasis's. */
#define GAME_SMPS       1

#endif
