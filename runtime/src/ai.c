/* The AI partner: see ai.h. Addresses are in docs/ram.md (Streets of Rage) and
 * docs/ram-sor2.md and docs/ram-sor3.md. */
#include "ai.h"
#include "game.h"
#include "genesis.h"

/* Where each game keeps its fighters. */
typedef struct {
    uint16_t p1, p2;                    /* the player objects */
    uint16_t first_other, end_other;    /* enemies, items and effects */
    uint16_t slot;                      /* object size */
    uint8_t x, z, hp;                   /* world X, depth and health words in an object */
} Layout;

#if GAME_SOR == 3
static const Layout L = { 0xDF00, 0xE000, 0xE100, 0xEF00, 0x100, 0x02, 0x06, 0x6C };
#elif GAME_SOR == 2
static const Layout L = { 0xEF00, 0xF000, 0xF100, 0xFA00, 0x100, 0x02, 0x06, 0x80 };
#else
static const Layout L = { 0xB800, 0xB880, 0xB900, 0xC800, 0x80, 0x10, 0x14, 0x32 };
#endif
#define P1 L.p1
#define P2 me                         /* the one it plays: player 2, or player 1 for tests */
static uint16_t me;

/* How it fights: where it stands next to its target, and how close counts as in reach. */
#define REACH_X     28                  /* stand this far from the target's centre */
#define CLOSE_X     6                   /* near enough to that spot */
#define ROW_Z       3                   /* on the same row */
#define FOLLOW_X    56                  /* with nothing to fight: this far behind player 1 */
#define FOLLOW_Z    24                  /* and this many rows off his */

static int frame, stuck, last_x, last_z, special_wait, join_wait;

static uint8_t peek8(uint32_t a) { return bus.ram[a & 0xFFFF]; }
static int16_t peek16(uint32_t a) { a &= 0xFFFF; return (int16_t)(bus.ram[a] << 8 | bus.ram[(a + 1) & 0xFFFF]); }

static int x_of(uint32_t o) { return peek16(o + L.x); }
static int z_of(uint32_t o) { return peek16(o + L.z); }
/* Health: a byte in Streets of Rage 3, a word in the others. */
static int hp_of(uint32_t o) { return GAME_SOR == 3 ? peek8(o + L.hp) : peek16(o + L.hp); }

/* Where an object is on screen (0-319 in view). */
static int screen_x(uint32_t o) {
#if GAME_SOR == 3
    return x_of(o) - peek16(0xF710);    /* the camera */
#elif GAME_SOR == 2
    return x_of(o) + peek16(0xFC22);    /* the plane's horizontal scroll: minus the camera */
#else
    return peek16(o + 0x28) - 0x80;     /* the sprite's X */
#endif
}

/* The camera's left edge in world X. */
static int camera(void) { return x_of(P2) - screen_x(P2); }

static bool alive(uint32_t o) {
#if GAME_SOR == 3
    int state = peek16(o);
    return state != 0 && state != 0x5C && state != 0x5E && hp_of(o) > 0;   /* $5C/$5E: not joined */
#elif GAME_SOR == 2
    int state = peek16(o);
    return state > 0 && state < 0x30 && hp_of(o) > 0;
#else
    return peek16(o) != 0 && peek16(o + 0x32) >= 0 && peek16(o + 0x30) != 0x300;
#endif
}

/* Something to fight: in use, alive, an enemy, and on screen (or anywhere). */
static bool enemy(uint32_t o, bool anywhere) {
    if (!alive(o)) return false;
#if GAME_SOR == 2
    /* Enemies carry the name the HUD shows ("GALSIA", "Y.SIGNAL"); items and props do not. */
    uint8_t c0 = peek8(o + 0x78), c1 = peek8(o + 0x79);
    if (c0 < 'A' || c0 > 'Z' || ((c1 < 'A' || c1 > 'Z') && c1 != '.')) return false;
#elif GAME_SOR == 3
    if (peek8(o + 0x6D) == 0) return false;    /* has a full health (items and props none) */
#else
    if (peek8(o + 0x34) == 0) return false;    /* hits back (attack power) */
#endif
    int sx = screen_x(o);
    return anywhere || (sx > -8 && sx < 328);
}

void ai_reset(void) { frame = stuck = last_x = last_z = special_wait = join_wait = 0; }

static uint16_t toward(int from, int to, int slack, uint16_t less, uint16_t more) {
    if (to < from - slack) return less;
    if (to > from + slack) return more;
    return 0;
}

/* Start, held for a moment every 1.5 s: the game only sees a press that lasts. */
static uint16_t press_start(void) { stuck = 0; return ++join_wait % 90 < 10 ? PAD_START : 0; }

/* Whether player 2 is in, pressing Start to get in when it can. Returns false (and the
 * pad to send in *pad) while it is not. */
static bool joined(uint16_t *pad) {
    *pad = 0;
#if GAME_SOR == 3
    /* Like Streets of Rage: player 2 joins a game in progress ($5C/$5E: not in), then picks
     * a fighter in its corner of the HUD ($5A), with Start too. Once in, Start would pause. */
    int s2 = peek16(P2);
    if (bus.ram[0xFB13] || !alive(P1)) { join_wait = 0; return false; }
    if (s2 == 0x5C || s2 == 0x5E || s2 == 0x5A) { *pad = press_start(); return false; }
    if (!alive(P2)) return false;
#elif GAME_SOR == 2
    /* Player 2 plays only in a two-player game (picked on the title menu). On the select
     * screen (no player 1 yet) Start picks its fighter; in a round, it comes back in when
     * it has not joined ($34/$36). Never otherwise: Start would pause. */
    if (bus.ram[0xFD10] || peek16(0xFC18) != 1) { join_wait = 0; return false; }
    int s1 = peek16(P1), s2 = peek16(P2);
    if (s1 == 0 || s2 == 0x34 || s2 == 0x36) { *pad = press_start(); return false; }
    if (!alive(P2)) return false;
#else
    /* Join in once player 1 is playing (not in the demo). Once in, Start would pause. */
    uint8_t in = peek8(0xFF18);         /* bit 0: player 1 is in the game, bit 1: player 2 */
    if (peek8(0xFF2A) || !(in & 1)) { join_wait = 0; return false; }
    if (!(in & 2)) { *pad = press_start(); return false; }
    if (!alive(P2)) return false;
#endif
    join_wait = 0;
    return true;
}

/* In trouble: the special. Streets of Rage's police car clears the screen while there are
 * cars left; the others' cost health, so only with plenty to spare. */
static bool special_now(int crowd) {
    if (special_wait || crowd < 2) return false;
#if GAME_SOR == 3
    return hp_of(P2) > 0x1A;           /* half of $34 */
#elif GAME_SOR == 2
    return hp_of(P2) > 0x30;
#else
    return peek16(P2 + 0x32) < 0x14 && peek8(0xFF24);
#endif
}

static uint16_t fight(uint16_t pad, bool pilot);
static uint32_t chased, given_up;       /* piloting: the target, and one it gave up on */
static int chased_hp, chased_since, given_up_until;

uint16_t ai_pad(void) {
    if (GAME_SOR < 1 || GAME_SOR > 3) return 0;
    me = L.p2;
    frame++;
    if (special_wait) special_wait--;

    uint16_t pad;
    if (!joined(&pad)) return pad;
    return fight(pad, false);
}

uint16_t ai_pilot_pad(void) {
    if (GAME_SOR < 1 || GAME_SOR > 3) return 0;
    me = L.p1;
    frame++;
    if (special_wait) special_wait--;
    if (!alive(P1)) return frame % 90 < 10 ? PAD_START : 0;  /* the title, the menus, a continue */
    return fight(0, true);
}

/* Fights the nearest enemy; with none, follows player 1 (or, piloting, walks on). */
static uint16_t fight(uint16_t pad, bool pilot) {
    int mx = x_of(P2), mz = z_of(P2);

    /* The nearest enemy, rows counting double (walking up and down is slow). */
    uint32_t target = 0;
    int best = 1 << 30, crowd = 0;
    for (uint32_t o = L.first_other; o < L.end_other; o += L.slot) {
        if (!enemy(o, pilot) || (pilot && o == given_up && frame < given_up_until)) continue;
        int dx = x_of(o) - mx, dz = z_of(o) - mz;
        int d = (dx < 0 ? -dx : dx) + 2 * (dz < 0 ? -dz : dz);
        if (d < 64) crowd++;
        if (d < best) { best = d; target = o; }
    }

    /* Piloting, a target it has not hurt in 5 s is out of reach for now (in a doorway,
     * behind something): it looks past it for 10 s. */
    if (pilot) {
        int hp = target ? hp_of(target) : 0;
        if (target != chased || hp != chased_hp) { chased = target; chased_hp = hp; chased_since = frame; }
        else if (target && frame - chased_since > 300) { given_up = target; given_up_until = frame + 600; chased = 0; }
    }

    int want_x, want_z;
    if (target) {
        int tx = x_of(target), tz = z_of(target);
        /* Stand beside it on the side we are already on. */
        want_x = mx <= tx ? tx - REACH_X : tx + REACH_X;
        want_z = tz;
        int dx = tx - mx, dz = tz - mz;
        int adx = dx < 0 ? -dx : dx, adz = dz < 0 ? -dz : dz;
        if (adz <= ROW_Z && adx <= REACH_X + 12 && adx >= REACH_X - 16) {
            /* In reach: face it, then a combo of quick punches. */
            int phase = frame % 8;
            if (phase == 0) pad |= dx < 0 ? PAD_LEFT : PAD_RIGHT;
            else if (phase < 4) pad |= PAD_B;
            if (special_now(crowd)) {
                pad = PAD_A;
                special_wait = 600;
            }
            stuck = 0;
            last_x = mx; last_z = mz;
            return pad;
        }
    } else if (pilot) {
        want_x = mx + 80;               /* on along the street */
        want_z = z_of(P1);
    } else {
        /* Nothing to fight: keep up with player 1, behind and on another row, out of the
         * way of his punches (in two-player games they hit each other). */
        int px = x_of(P1), pz = z_of(P1);
        want_x = mx <= px ? px - FOLLOW_X : px + FOLLOW_X;
#if GAME_SOR >= 2
        want_z = pz >= 0xC0 ? pz - FOLLOW_Z : pz + FOLLOW_Z;   /* rows about $A0-$E8 */
#else
        want_z = pz >= 60 ? pz - FOLLOW_Z : pz + FOLLOW_Z;     /* rows about 16-110 */
#endif
        int dx = want_x - mx, dz = want_z - mz;
        if ((dx < 0 ? -dx : dx) < 24 && (dz < 0 ? -dz : dz) < 6) { stuck = 0; return 0; }
    }

    /* Stay on screen: the camera follows player 1. */
    int cam = camera();
    if (want_x < cam + 16) want_x = cam + 16;
    if (want_x > cam + 304) want_x = cam + 304;

    pad |= toward(mx, want_x, CLOSE_X, PAD_LEFT, PAD_RIGHT);
    pad |= toward(mz, want_z, ROW_Z - 1, PAD_UP, PAD_DOWN);

    /* Walking into something that will not move (a wall, a car): jump. */
    if (pad && mx == last_x && mz == last_z) {
        if (++stuck > 40) { pad |= PAD_C; if (stuck > 48) stuck = 0; }
    } else stuck = 0;
    last_x = mx; last_z = mz;
    return pad;
}
