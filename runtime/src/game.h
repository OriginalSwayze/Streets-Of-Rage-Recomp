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

#include <stddef.h>
#include <stdint.h>

/* The cartridges this build has: the game's own (the main edition) and its other editions
 * in game.json (another country's, with its own code), each recompiled from its ROM
 * (tools/recomp.mjs). The one running is `game`, set when its ROM loads. */
enum { REGION_US, REGION_EU, REGION_JP };      /* the console a cartridge expects */

struct RecompEntry;
typedef struct GameEdition {
    const char *id, *name, *sha1;
    const char *edition;                        /* the player's name for it */
    const char *title;                          /* its title as sold, for the launcher's cover; NULL: the game's */
    int region;
    int is_main;                                /* the game's own, which keeps the plain file names */
    const struct RecompEntry *const *tables;    /* its recompiled code, NULL-terminated */
} GameEdition;

extern const GameEdition *const game_editions[GAME_EDITION_COUNT];   /* game.json order; NULL: not built */
extern const GameEdition *game;                                       /* the one loaded */

const GameEdition *game_edition_by_id(const char *id);
/* A file that belongs to one edition (its saves, save states, caches): `name` for the main
 * one, else the name with "-<id>" before its extension ("save.srm" -> "save-jp.srm",
 * "states" -> "states-jp"), so editions never read each other's. */
void game_file_name(char *out, size_t n, const char *name);

#endif
