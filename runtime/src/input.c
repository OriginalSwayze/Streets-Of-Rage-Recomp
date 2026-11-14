#include "input.h"

#include <string.h>

#include "genesis.h"
#include "settings.h"

_Static_assert(sizeof settings.bind_key / sizeof *settings.bind_key >= IN_COUNT &&
               sizeof settings.bind_pad / sizeof *settings.bind_pad >= IN_COUNT &&
               sizeof settings.bind_key_more / sizeof *settings.bind_key_more >= INPUT_PLAYERS - 1 &&
               sizeof *settings.bind_key_more / sizeof **settings.bind_key_more >= IN_PAD_COUNT &&
               sizeof *settings.bind_pad_more / sizeof **settings.bind_pad_more >= IN_PAD_COUNT, "settings.h binding arrays");

#if INPUT_SIX_BUTTON
#define SIX(...) __VA_ARGS__,
#else
#define SIX(...)
#endif

const char *const input_labels[IN_COUNT] = {
    "Up", "Down", "Left", "Right", "A button", "B button", "C button", "Start",
    SIX("X button", "Y button", "Z button")
    "Rewind (hold)", "Pause menu", "Fast-forward (hold)",
};

/* settings.ini names. */
static const char *const ini_names[IN_COUNT] = {
    "up", "down", "left", "right", "a", "b", "c", "start", SIX("x", "y", "z") "rewind", "menu", "fast",
};

static const uint16_t pad_bit[IN_COUNT] = {
    PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT, PAD_A, PAD_B, PAD_C, PAD_START, SIX(PAD_X, PAD_Y, PAD_Z) 0, 0, 0,
};

/* A player's bindings, and how many actions they have. */
static int *keys_of(int player) { return player ? settings.bind_key_more[player - 1] : settings.bind_key; }
static int *buttons_of(int player) { return player ? settings.bind_pad_more[player - 1] : settings.bind_pad; }
static int actions_of(int player) { return player ? IN_PAD_COUNT : IN_COUNT; }

/* Xbox layout: the Genesis A, B, C row sits on X, A, B; X, Y and Z (Streets of Rage 3)
 * on Y and the two stick buttons. */
static const SDL_GameControllerButton default_buttons[IN_COUNT] = {
    SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_START,
    SIX(SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK)
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
};

static bool more_loaded[INPUT_PLAYERS];  /* a settings.ini had player 2's or 3's buttons */

void input_player_defaults(int player) {
    static const SDL_Scancode keys[IN_COUNT] = {
        SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_RETURN,
        SIX(SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D)
        SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_TAB,
    };
    int *k = keys_of(player), *b = buttons_of(player);
    for (int i = 0; i < actions_of(player); i++) {
        /* Players 2 and 3 have no keys until they pick some; player 1 gets theirs back,
         * taking them from another player that had one. */
        k[i] = player ? 0 : keys[i];
        if (!player)
            for (int p = 1; p < INPUT_PLAYERS; p++)
                for (int j = 0; j < IN_PAD_COUNT; j++)
                    if (keys_of(p)[j] == (int)keys[i]) keys_of(p)[j] = 0;
        b[i] = default_buttons[i];
    }
}

void input_defaults(void) {
    for (int p = 0; p < INPUT_PLAYERS; p++) { input_player_defaults(p); more_loaded[p] = false; }
}

bool input_ini_load(const char *key, const char *value) {
    int player = 0;
    if (key[0] == 'p' && key[1] >= '2' && key[1] < '1' + INPUT_PLAYERS) { player = key[1] - '1'; key += 2; }
    for (int i = 0; i < actions_of(player); i++) {
        if (!strncmp(key, "key_", 4) && !strcmp(key + 4, ini_names[i])) {
            SDL_Scancode s = SDL_GetScancodeFromName(value);
            if (s != SDL_SCANCODE_UNKNOWN || !*value) keys_of(player)[i] = s;   /* empty: no key */
            return true;
        }
        if (!strncmp(key, "pad_", 4) && !strcmp(key + 4, ini_names[i])) {
            SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(value);
            SDL_GameControllerAxis a = SDL_GameControllerGetAxisFromString(value);
            int *pad = buttons_of(player);
            if (b != SDL_CONTROLLER_BUTTON_INVALID) pad[i] = b;
            else if (a == SDL_CONTROLLER_AXIS_TRIGGERLEFT) pad[i] = INPUT_TRIGGER_LEFT;
            else if (a == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) pad[i] = INPUT_TRIGGER_RIGHT;
            more_loaded[player] = true;
            return true;
        }
    }
    return false;
}

/* A settings.ini from before players 2 and 3 had their own buttons: their controllers
 * used player 1's, so they keep doing that until they pick their own. */
void input_ini_done(void) {
    for (int p = 1; p < INPUT_PLAYERS; p++)
        if (!more_loaded[p])
            for (int i = 0; i < IN_PAD_COUNT; i++) buttons_of(p)[i] = settings.bind_pad[i];
}

/* A trigger binding's axis, or SDL_CONTROLLER_AXIS_INVALID for a button. */
static SDL_GameControllerAxis trigger_axis(int bind) {
    return bind == INPUT_TRIGGER_LEFT ? SDL_CONTROLLER_AXIS_TRIGGERLEFT
         : bind == INPUT_TRIGGER_RIGHT ? SDL_CONTROLLER_AXIS_TRIGGERRIGHT : SDL_CONTROLLER_AXIS_INVALID;
}

#define TRIGGER_DOWN 20000              /* of 32767: past 60% of its travel (a pad whose trigger rests at half reads 16383 at rest) */

void input_ini_save(FILE *f) {
    fprintf(f, "# Controls: keyboard key and controller button (or trigger) for each action; p2 and p3 for players 2 and 3\n");
    for (int p = 0; p < INPUT_PLAYERS; p++) {
        char pre[4] = "";
        if (p) { pre[0] = 'p'; pre[1] = (char)('1' + p); }
        for (int i = 0; i < actions_of(p); i++) {
            int bind = buttons_of(p)[i], key = keys_of(p)[i];
            SDL_GameControllerAxis a = trigger_axis(bind);
            const char *b = a != SDL_CONTROLLER_AXIS_INVALID ? SDL_GameControllerGetStringForAxis(a)
                                                              : SDL_GameControllerGetStringForButton((SDL_GameControllerButton)bind);
            fprintf(f, "%skey_%s=%s\n", pre, ini_names[i], key > 0 ? SDL_GetScancodeName((SDL_Scancode)key) : "");
            fprintf(f, "%spad_%s=%s\n", pre, ini_names[i], b ? b : "");
        }
    }
}

static bool held(int player, int action, const Uint8 *keys, SDL_GameController *pad) {
    int k = keys_of(player)[action];
    if (keys && k > 0 && k < SDL_NUM_SCANCODES && keys[k]) return true;
    if (!pad) return false;
    int bind = buttons_of(player)[action];
    SDL_GameControllerAxis a = trigger_axis(bind);
    if (a != SDL_CONTROLLER_AXIS_INVALID) return SDL_GameControllerGetAxis(pad, a) > TRIGGER_DOWN;
    return SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)bind);
}

bool input_held(int action, const Uint8 *keys, SDL_GameController *pad) { return held(0, action, keys, pad); }

int input_trigger_pressed(const SDL_Event *e) {
    if (e->type != SDL_CONTROLLERAXISMOTION) return -1;
    int t = e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? 0 : e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT ? 1 : -1;
    if (t < 0) return -1;
    /* A press is a move past TRIGGER_DOWN: each trigger's last value on each controller.
     * One event may be asked about for several actions, so its answer is kept. */
    static SDL_ControllerAxisEvent asked;
    static int answer = -1;
    if (asked.timestamp == e->caxis.timestamp && asked.which == e->caxis.which && asked.axis == e->caxis.axis &&
        asked.value == e->caxis.value && asked.type) return answer;
    static struct { SDL_JoystickID pad; int trigger; Sint16 value; bool used; } last[8];
    int k = 0;
    while (k < 8 && last[k].used && !(last[k].pad == e->caxis.which && last[k].trigger == t)) k++;
    if (k == 8) k = 0;                  /* (more than four pads: the first slot again) */
    bool was = last[k].used && last[k].pad == e->caxis.which && last[k].trigger == t && last[k].value > TRIGGER_DOWN;
    last[k].used = true; last[k].pad = e->caxis.which; last[k].trigger = t; last[k].value = e->caxis.value;
    asked = e->caxis;
    answer = e->caxis.value > TRIGGER_DOWN && !was ? (t ? INPUT_TRIGGER_RIGHT : INPUT_TRIGGER_LEFT) : -1;
    return answer;
}

bool input_event_is(int action, const SDL_Event *e) {
    if (e->type == SDL_KEYDOWN) return !e->key.repeat && (int)e->key.keysym.scancode == settings.bind_key[action];
    if (e->type == SDL_CONTROLLERBUTTONDOWN) return e->cbutton.button == settings.bind_pad[action];
    if (e->type == SDL_CONTROLLERAXISMOTION) { int t = input_trigger_pressed(e); return t >= 0 && t == settings.bind_pad[action]; }
    return false;
}

uint16_t input_pad_bits(int player, const Uint8 *keys, SDL_GameController *pad) {
    uint16_t p = 0;
    for (int i = 0; i < IN_PAD_COUNT; i++) if (pad_bit[i] && held(player, i, keys, pad)) p |= pad_bit[i];
    if (pad) {                          /* the left stick moves too, whatever the bindings */
        const int dead = 12000;
        int lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        int ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (ly < -dead) p |= PAD_UP;
        if (ly > dead) p |= PAD_DOWN;
        if (lx < -dead) p |= PAD_LEFT;
        if (lx > dead) p |= PAD_RIGHT;
    }
    return p;
}

const char *input_key_name(int player, int action) {
    int k = keys_of(player)[action];
    const char *n = k > 0 ? SDL_GetScancodeName((SDL_Scancode)k) : NULL;
    if (!n || !*n) return "(none)";
    if (!strcmp(n, "Return")) return "Enter";
    return n;
}

/* Xbox-style names for the buttons. */
const char *input_button_name(int player, int action) {
    static const char *const names[SDL_CONTROLLER_BUTTON_MAX] = {
        "A", "B", "X", "Y", "Back", "Guide", "Start", "Left stick", "Right stick", "LB", "RB",
        "D-pad up", "D-pad down", "D-pad left", "D-pad right", "Share", "Paddle 1", "Paddle 2", "Paddle 3",
        "Paddle 4", "Touchpad",
    };
    int b = buttons_of(player)[action];
    if (b == INPUT_TRIGGER_LEFT) return "LT";
    if (b == INPUT_TRIGGER_RIGHT) return "RT";
    return b >= 0 && b < SDL_CONTROLLER_BUTTON_MAX && names[b] ? names[b] : "(none)";
}

void input_bind_key(int player, int action, SDL_Scancode key) {
    int *mine = &keys_of(player)[action], old = *mine;
    if (key > 0)
        for (int p = 0; p < INPUT_PLAYERS; p++)
            for (int i = 0; i < actions_of(p); i++)
                if (&keys_of(p)[i] != mine && keys_of(p)[i] == (int)key) keys_of(p)[i] = old;
    *mine = key;
}

void input_bind_button(int player, int action, int button) {
    int *b = buttons_of(player);
    for (int i = 0; i < actions_of(player); i++)
        if (i != action && b[i] == button) b[i] = b[action];
    b[action] = button;
}
