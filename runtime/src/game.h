/* What this build knows about its game, from games/<name>/game.json (tools/game-config.mjs
 * writes game_config.h into the build). The engine (recompiler, hardware, renderer, menus,
 * save states, rewind) works for any Genesis cartridge; these features read the game's own
 * RAM or call its own code, so each is on only once that game's addresses are known:
 *   GAME_KNOWS_RAM   its objects, health and items: built-in cheats, combat info, rumble,
 *                    stats, the map, the randomizer, spirit effects, the outfit, smooth
 *                    motion and the camera (Beyond Oasis's addresses)
 *   GAME_WIDESCREEN  replaces its own scrolling and camera routines (widescreen.c)
 *   GAME_LAUNCHER    the launcher: its soundtrack player, menu sounds and art run the
 *                    game's sound driver and read its graphics (launcher.c, ost.c)
 *   GAME_SOR         which Streets of Rage this is (1-3; 0 for another game): the code that
 *                    reads one game's RAM runs for that game only (docs/ram.md)
 *   GAME_WIDE_VIEW   a wider picture from the game's own patches (Streets of Rage: sorWide)
 *   GAME_SMPS        the sound driver is Sega's SMPS 68k (ost.c)
 *   GAME_FIGHTERS    the fighters' colours (fighters.c) */
#ifndef GAME_H
#define GAME_H

#include "game_config.h"

#endif
