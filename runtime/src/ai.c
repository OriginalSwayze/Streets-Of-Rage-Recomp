/* The AI partner: see ai.h. Addresses are in docs/ram.md. */
#include "ai.h"
#include "genesis.h"

#define P1          0xB800              /* player objects */
#define P2          0xB880
#define FIRST_OTHER 0xB900              /* enemies, items and effects */
#define END_OTHER   0xC800
#define SLOT        0x80

#define IN_GAME     0xFF18              /* bit 0: player 1 is in the game, bit 1: player 2 */
#define DEMO        0xFF2A              /* the attract demo is playing */
#define SPECIALS2   0xFF24              /* player 2's special attacks */

/* How it fights: where it stands next to its target, and how close counts as in reach. */
#define REACH_X     28                  /* stand this far from the target's centre */
#define CLOSE_X     6                   /* near enough to that spot */
#define ROW_Z       3                   /* on the same row */
#define FOLLOW_X    56                  /* with nothing to fight: this far behind player 1 */
#define FOLLOW_Z    24                  /* and this many rows off his */

static int frame, stuck, last_x, last_z, special_wait, join_wait;

static uint8_t peek8(uint32_t a) { return bus.ram[a & 0xFFFF]; }
static int16_t peek16(uint32_t a) { a &= 0xFFFF; return (int16_t)(bus.ram[a] << 8 | bus.ram[(a + 1) & 0xFFFF]); }

static int x_of(uint32_t o) { return peek16(o + 0x10); }
static int z_of(uint32_t o) { return peek16(o + 0x14); }
static int screen_x(uint32_t o) { return peek16(o + 0x28) - 0x80; }
static bool alive(uint32_t o) { return peek16(o) != 0 && peek16(o + 0x32) >= 0 && peek16(o + 0x30) != 0x300; }

/* Something to fight: in use, alive, hits back (attack power), and on screen. */
static bool enemy(uint32_t o) {
    if (!alive(o) || peek8(o + 0x34) == 0) return false;
    int sx = screen_x(o);
    return sx > -8 && sx < 328;
}

void ai_reset(void) { frame = stuck = last_x = last_z = special_wait = join_wait = 0; }

static uint16_t toward(int from, int to, int slack, uint16_t less, uint16_t more) {
    if (to < from - slack) return less;
    if (to > from + slack) return more;
    return 0;
}

uint16_t ai_pad(void) {
    frame++;
    if (special_wait) special_wait--;

    /* Join in once player 1 is playing (not in the demo). Start is held for a moment, as
     * the game only sees a press that lasts; once in, Start would pause, so never then. */
    uint8_t in = peek8(IN_GAME);
    if (peek8(DEMO) || !(in & 1)) { join_wait = 0; return 0; }
    if (!(in & 2)) {
        stuck = 0;
        return ++join_wait % 90 < 10 ? PAD_START : 0;
    }
    join_wait = 0;
    if (!alive(P2)) return 0;

    int mx = x_of(P2), mz = z_of(P2), msx = screen_x(P2);

    /* The nearest enemy, rows counting double (walking up and down is slow). */
    uint32_t target = 0;
    int best = 1 << 30, crowd = 0;
    for (uint32_t o = FIRST_OTHER; o < END_OTHER; o += SLOT) {
        if (!enemy(o)) continue;
        int dx = x_of(o) - mx, dz = z_of(o) - mz;
        int d = (dx < 0 ? -dx : dx) + 2 * (dz < 0 ? -dz : dz);
        if (d < 64) crowd++;
        if (d < best) { best = d; target = o; }
    }

    uint16_t pad = 0;
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
            /* In trouble: the special clears the screen. */
            if (peek16(P2 + 0x32) < 0x14 && crowd >= 2 && peek8(SPECIALS2) && !special_wait) {
                pad = PAD_A;
                special_wait = 600;
            }
            stuck = 0;
            last_x = mx; last_z = mz;
            return pad;
        }
    } else {
        /* Nothing to fight: keep up with player 1, behind and on another row, out of the
         * way of his punches (in two-player games they hit each other). The street's rows
         * run from about 16 to 110. */
        int px = x_of(P1), pz = z_of(P1);
        want_x = mx <= px ? px - FOLLOW_X : px + FOLLOW_X;
        want_z = pz >= 60 ? pz - FOLLOW_Z : pz + FOLLOW_Z;
        int dx = want_x - mx, dz = want_z - mz;
        if ((dx < 0 ? -dx : dx) < 24 && (dz < 0 ? -dz : dz) < 6) { stuck = 0; return 0; }
    }

    /* Stay on screen: the camera follows player 1 only. */
    int cam = mx - msx;
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
