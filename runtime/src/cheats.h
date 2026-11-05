/* Built-in cheats (cheats.c): RAM values held every frame. */
#ifndef CHEATS_H
#define CHEATS_H

#include <stdbool.h>
#include <stdint.h>

/* Raw codes: "FFxxxx:VV" (byte) or "FFxxxx:VVVV" (word), comma-separated. Returns
 * the number of codes parsed. They are held every frame until cheats_clear_codes(). */
int  cheats_add_codes(const char *spec);
/* Writes RAM codes in the same form once, now (tests: --poke-at). */
void cheats_poke_once(const char *spec);
void cheats_clear_codes(void);

/* Called once per frame before the game runs: applies the enabled cheats. */
void cheats_frame(void);

/* After every machine reset (machine_override): the Options screens' hidden entries, for
 * settings.cheat_unlock. */
void cheats_hooks(void);

#endif
