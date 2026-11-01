/* Controllers (pads.c): every connected controller is open, from the start and as they come
 * and go, so one that sleeps or reconnects is picked up again. The game reads the one used
 * last. Shared by the launcher and the game, which call pads_event() on every event. */
#ifndef PADS_H
#define PADS_H

#include <SDL.h>
#include <stdbool.h>

/* Opens the controllers already connected (after SDL_INIT_GAMECONTROLLER). */
void pads_init(void);
/* Follows connects, disconnects and which controller was used last. */
void pads_event(const SDL_Event *e);
/* The controller used last, or the first connected; NULL with none. */
SDL_GameController *pads_active(void);
/* The controller of player 0 or 1, in the order they were connected; NULL if there are
 * not that many. */
SDL_GameController *pads_player(int player);
int pads_count(void);
/* Rumbles the active controller: low and high frequency motors (0-1) for ms. */
void pads_rumble(float low, float high, int ms);

#endif
