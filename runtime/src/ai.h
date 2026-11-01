/* The AI partner (ai.c): plays player 2 by pressing its pad, from what the game keeps in
 * RAM (docs/ram.md). It joins in, fights the nearest enemy on its row, follows player 1
 * when the street is clear and saves its special for when it is in trouble. */
#ifndef AI_H
#define AI_H

#include <stdint.h>

/* Player 2's buttons for the coming frame (PAD_* bits). Only looks at RAM and its own
 * counters, so it plays the same way every time from the same state. */
uint16_t ai_pad(void);
/* Forgets the counters (after loading a state or rewinding). */
void ai_reset(void);

#endif
