/* Launcher shown before the game (launcher.c). */
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <stdbool.h>

/* Shows the launcher window; saves settings to ini_path. Returns 1 to play, 0 to quit. */
int launcher_run(const char *ini_path);
/* Started from the Streets of Rage Collection (--collection): Quit is "Back", to it. */
extern bool launcher_in_collection;
/* Renders one launcher frame, `width` pixels wide, with tab `tab` (0-3) open at time
 * t, to a PNG (for checking the look without a window). */
int launcher_screenshot(const char *png, int width, int tab, float t);
/* Runs the soundtrack player for `seconds` without an audio device and prints its level
 * and song changes (for checking it headless); with sfx_every, the menu's sounds play
 * every sfx_every frames, as while browsing. */
int launcher_music_check(int seconds, int sfx_every);
int launcher_ost_check(int seconds);   /* every song's level per second */
int launcher_sound_probe(int from, int to, int seconds);   /* driver requests from..to, the same */
/* Renders the game's four menu sounds (cursor, confirm, cancel, error) to a file for the
 * pause menu; the machine needs a reset afterwards. */
bool launcher_make_menu_sounds(const char *path);

#endif
