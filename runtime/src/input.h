/* Input bindings (input.c): each action has one keyboard key and one controller button,
 * kept in settings.ini as key_<action>=<key name> and pad_<action>=<button name>.
 *
 * Players 2 and 3 have their own key and button for each of the pad's actions (Up to
 * Start, and X, Y and Z), as p2key_<action> / p2pad_<action> and p3...: their buttons on
 * their own controller, and keys on the shared keyboard (none by default). Rewind, the
 * menu and fast-forward are player 1's. */
#ifndef INPUT_H
#define INPUT_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "game.h"

/* Streets of Rage 3 reads a six-button pad (io.c): X, Y and Z have bindings too. */
#define INPUT_SIX_BUTTON (GAME_SOR == 3)

enum { IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_A, IN_B, IN_C, IN_START,
#if INPUT_SIX_BUTTON
       IN_X, IN_Y, IN_Z,
#endif
       IN_REWIND, IN_MENU, IN_FAST, IN_COUNT };

#define INPUT_PLAYERS 3
#define IN_PAD_COUNT  IN_REWIND         /* the pad's own actions, the ones every player has */

extern const char *const input_labels[IN_COUNT];   /* "Up", "A button", "Rewind", ... */

void     input_defaults(void);                      /* default bindings into settings */
bool     input_ini_load(const char *key, const char *value);   /* true if it was a binding */
void     input_ini_done(void);                      /* after a load: see input.c */
void     input_ini_save(FILE *f);

/* A player's Genesis pad bits (0 = player 1) from the keyboard and a controller (also its
 * left stick). */
uint16_t input_pad_bits(int player, const Uint8 *keys, SDL_GameController *pad);
/* Whether one of player 1's actions is held, or was just pressed in this event. */
bool     input_held(int action, const Uint8 *keys, SDL_GameController *pad);
bool     input_event_is(int action, const SDL_Event *e);

/* Display names of a player's bindings ("Z", "Enter", "A", "LB", ... or "(none)"). */
const char *input_key_name(int player, int action);
const char *input_button_name(int player, int action);

/* Binds a key or a button to a player's action. One that had it already gets the action's
 * old key or button instead, so no two share one: a key among all the players (they share
 * the keyboard), a button among that player's actions. */
void     input_bind_key(int player, int action, SDL_Scancode key);
void     input_bind_button(int player, int action, int button);
void     input_player_defaults(int player);         /* one player's defaults (Reset to defaults) */

/* The triggers bind like buttons (settings.bind_pad), past SDL's buttons: some pads give
 * their C or Z (a Genesis-style pad's) as a trigger, which is an axis, not a button (#31). */
#define INPUT_TRIGGER_LEFT  100
#define INPUT_TRIGGER_RIGHT 101
/* The trigger an event pressed past 60% of its travel: INPUT_TRIGGER_*, or -1. */
int      input_trigger_pressed(const SDL_Event *e);

#endif
