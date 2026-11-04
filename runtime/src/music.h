/* Replacement music (music.c): the game's songs from the player's own files.
 *
 * Files go in the game's folder, in music\, named by the song's number in the launcher's
 * player (its Track row), e.g. "07.flac" or "07 - Go Straight.mp3": MP3, FLAC, WAV, M4A/AAC
 * or WMA (Windows decodes them). When the game asks its sound driver for a song that has
 * a file, the driver is asked for silence instead (its sound effects play on) and the file
 * plays, looping where the game's song loops. */
#ifndef MUSIC_H
#define MUSIC_H

#include <stdbool.h>
#include <stdint.h>

#include "genesis.h"

/* The folder holding music\ (the game's own folder, ending in a separator). */
void music_set_folder(const char *dir);
/* Looks for the files again (at start, and when the setting changes). Returns how many
 * songs have one. */
int  music_scan(void);
int  music_found(void);                 /* songs with a file, from the last scan */
extern bool music_files_on;             /* play them (settings: Music files) */

/* After every machine reset: hooks the driver's song requests (machine_override). */
void music_hooks(void);
/* Mixes the playing file into a frame's output (sound_end_frame), at `gain` (the mix's
 * headroom). */
void music_mix(int16_t *out, int frames, int rate, float gain);
/* Stops the file (a reset, the launcher's player stopping). */
void music_stop(void);

/* Save states: which song plays and where, after the rest of the state (older states
 * have none, and load with the file stopped). */
void music_state(StateIO *s);

#endif
