/* Motion from the game's own data. Beyond Oasis builds its sprite list in RAM once per
 * game tick (30 Hz): $FF13CC holds the entries (8 bytes each, as in the VDP's sprite
 * table), $FF188A their count. Objects add their sprites through three routines that
 * first work out the object's screen position (the anchor every piece is placed from):
 *   $AB4A, $AECC, $B0C4   a6 = object, d1.w = x + $80, a4.w = y - height + $80
 * and small effects (8-byte objects) through one that places a single piece:
 *   $A98E                 a6 = object, d2.w = x + 8, d3.w = y + 8
 * The object loop calls each object with a return address pushed, so the returns below
 * mark the end of one object's drawing. The V blank handler sends the finished list to
 * the VDP by DMA ($1FBE) and empties it; that is one tick.
 *
 * Every sprite added after an anchor and before its object's drawing ends belongs to
 * that object. Sprites added any other way (the HUD, menus, text) have no object and
 * never move between ticks. */
#include "motion.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "recomp_rt.h"

#define LIST     0x13CC                 /* sprite list in RAM */
#define COUNT    0x188A                 /* entries in it */
#define MAX_JUMP 48                     /* a bigger move in one tick is a jump: no gliding */

typedef struct {
    uint32_t object;                    /* object address */
    uint16_t kind;                      /* the object's first word (its type) when it drew */
    int      nth;                       /* this object's 1st, 2nd... anchor in the tick */
    int16_t  x, y;                      /* screen position the game placed it at */
} Anchor;

typedef struct {
    unsigned serial;                    /* 0 = empty */
    int      count;
    uint8_t  raw[SPRITES_MAX][8];                /* the entries as sent to the VDP */
    int8_t   anchor[SPRITES_MAX];                /* anchor of each entry, or -1 */
    Anchor   anchors[SPRITES_MAX];
    int      anchor_count;
} Tick;

static Tick ring[4];
static unsigned serial;

/* The list being built. */
static int8_t  build_anchor[SPRITES_MAX];
static Anchor  build_anchors[SPRITES_MAX];
static int     build_anchor_count;
static int     seen_count;
static bool    built;                   /* entries added since the last copy */
static int     pending = -1;            /* anchor for the sprites about to be added */

static int ram16(uint32_t a) { return bus.ram[a] << 8 | bus.ram[a + 1]; }

static void anchor_at(int16_t x, int16_t y) {
    if (build_anchor_count >= SPRITES_MAX) { pending = -1; return; }
    uint32_t object = A(6) & 0xFFFFFF;
    int nth = 0;
    for (int k = 0; k < build_anchor_count; k++) nth += build_anchors[k].object == object;
    Anchor *a = &build_anchors[build_anchor_count];
    *a = (Anchor){ object, (uint16_t)ram16(object & 0xFFFF), nth, x, y };
    pending = build_anchor_count++;
}

static void take_tick(void) {
    Tick *t = &ring[serial % 4];
    int n = seen_count > SPRITES_MAX ? SPRITES_MAX : seen_count;
    t->serial = ++serial;
    t->count = n;
    memcpy(t->raw, bus.ram + LIST, (size_t)n * 8);
    memcpy(t->anchor, build_anchor, sizeof t->anchor);
    memcpy(t->anchors, build_anchors, sizeof t->anchors);
    t->anchor_count = build_anchor_count;
}

/* Called after each native block has run: the anchor registers are still as the block
 * found them (none of these blocks change them), and $A98E adds its piece itself. */
void motion_block(uint32_t pc) {
    if (!GAME_KNOWS_RAM) return;      /* Beyond Oasis's sprite routines: nothing to follow yet */
    switch (pc) {
    case 0xAB4A: case 0xAECC: case 0xB0C4: anchor_at((int16_t)D(1), (int16_t)A(4)); break;
    case 0xA98E: anchor_at((int16_t)D(2), (int16_t)D(3)); break;
    }
    int count = ram16(COUNT);
    if (count != seen_count) {
        if (count < seen_count) {       /* a new list */
            build_anchor_count = 0;
            for (int k = 0; k < SPRITES_MAX; k++) build_anchor[k] = -1;
            seen_count = 0;
        }
        for (int k = seen_count; k < count && k < SPRITES_MAX; k++) build_anchor[k] = (int8_t)pending;
        if (count > seen_count) built = true;
        seen_count = count;
    }
    switch (pc) {
    /* Back in the object loops: that object's drawing is over. */
    case 0xA1BA: case 0xA1D0: case 0xA1F8: case 0xA258: case 0xA270: case 0xA2A6: pending = -1; break;
    case 0x1FBE:                        /* V blank: about to send the list to the VDP */
        if (built) take_tick();
        built = false;
        build_anchor_count = 0;
        pending = -1;
        break;
    }
}

void motion_reset(void) {
    memset(ring, 0, sizeof ring);
    for (int k = 0; k < SPRITES_MAX; k++) build_anchor[k] = -1;
    build_anchor_count = 0;
    seen_count = ram16(COUNT);
    built = false;
    pending = -1;
}

/* The entry as the VDP shows it: y, x and pattern match (the link may differ). */
static bool same_entry(const uint8_t *e, const HdSprite *s) {
    int y = ((e[0] << 8 | e[1]) & 0x3FF) - 128, x = ((e[6] << 8 | e[7]) & 0x1FF) - 128;
    return y == s->y && x == s->x && (e[4] << 8 | e[5]) == s->attr;
}

static const Tick *shown_tick(const HdFrame *f) {
    const Tick *best = NULL;
    for (int r = 0; r < 4; r++) {
        const Tick *t = &ring[r];
        if (!t->serial || (best && t->serial < best->serial)) continue;
        bool all = true;
        for (int i = 0; i < SPRITES_MAX && all; i++)
            if (f->sprites[i].drawn) all = i < t->count && same_entry(t->raw[i], &f->sprites[i]);
        if (all) best = t;
    }
    return best;
}

static const Tick *tick_numbered(unsigned s) {
    const Tick *t = &ring[(s - 1) % 4];
    return s && t->serial == s ? t : NULL;
}

#define PLAYER 0xFF19E8                 /* the player character's object */

void motion_player_entries(const uint8_t *sat, int count, bool out[SPRITES_MAX]) {
    memset(out, 0, SPRITES_MAX * sizeof *out);
    const Tick *t = NULL;
    for (int r = 0; r < 4; r++) if (ring[r].serial && (!t || ring[r].serial > t->serial)) t = &ring[r];
    if (!t) return;
    for (int i = 0; i < count && i < t->count; i++) {
        const uint8_t *e = sat + i * 8, *m = t->raw[i];
        /* The same entry as the game built it (the link may differ). */
        if (((e[0] ^ m[0]) & 3) || e[1] != m[1] || e[4] != m[4] || e[5] != m[5] || ((e[6] ^ m[6]) & 1) || e[7] != m[7]) continue;
        out[i] = t->anchor[i] >= 0 && t->anchors[t->anchor[i]].object == PLAYER;
    }
}

bool motion_entry_owner(int i, uint32_t *object, uint16_t *kind) {
    const Tick *t = NULL;
    for (int r = 0; r < 4; r++) if (ring[r].serial && (!t || ring[r].serial > t->serial)) t = &ring[r];
    if (!t || i < 0 || i >= t->count || t->anchor[i] < 0) return false;
    *object = t->anchors[t->anchor[i]].object;
    *kind = t->anchors[t->anchor[i]].kind;
    return true;
}

/* Games whose sprite routines are not followed (the Streets of Rage games, updating every
 * frame): each sprite is matched to itself in the previous frame, a sprite of the same size
 * showing the same picture, the nearest if there are several (enemies of one kind look
 * alike), and moves as far as it went. By picture, not tile number: the fighters' tiles are
 * loaded again every frame, so one tile number holds a different piece from frame to frame. A piece that has no match (its animation frame changed
 * shape) moves with the nearest piece that has one, so a fighter's pieces stay together.
 * When the whole fighter changed (a walk changes every piece every few frames) none of its
 * own has: then a piece of the frame before, nearer than any match, keeps it going as that
 * piece went, rather than it standing still for a frame and jumping, or taking the move of
 * something else nearby (the jitter a walk showed). */
static uint32_t picture_of(const HdSprite *c) {
    uint32_t h = 2166136261u;
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++) h = (h ^ (c->px[y * 32 + x] & 0xFFFFFF)) * 16777619u;
    return h;
}

static unsigned frame_moves(const HdFrame *f, int dx[SPRITES_MAX], int dy[SPRITES_MAX], int *owned) {
    static struct { bool drawn, matched; int x, y, w, h, dx, dy; uint32_t picture; } prev[SPRITES_MAX];
    uint32_t pic[SPRITES_MAX];
    for (int i = 0; i < SPRITES_MAX; i++) pic[i] = f->sprites[i].drawn ? picture_of(&f->sprites[i]) : 0;
    static unsigned serial;
    bool matched[SPRITES_MAX] = { false }, steady[SPRITES_MAX] = { false };
    for (int i = 0; i < SPRITES_MAX; i++) {
        const HdSprite *c = &f->sprites[i];
        if (!c->drawn) continue;
        int best = MAX_JUMP * 2 + 1;
        for (int j = 0; j < SPRITES_MAX; j++) {
            if (!prev[j].drawn || prev[j].picture != pic[i] || prev[j].w != c->w || prev[j].h != c->h) continue;
            int mx = c->x - prev[j].x, my = c->y - prev[j].y, d = abs(mx) + abs(my);
            if (abs(mx) <= MAX_JUMP && abs(my) <= MAX_JUMP && d < best) {
                best = d; dx[i] = mx; dy[i] = my; matched[i] = true;
                steady[i] = prev[j].matched && abs(prev[j].dx - mx) <= 1 && abs(prev[j].dy - my) <= 1;
            }
        }
        if (matched[i] && owned) ++*owned;
    }
    for (int i = 0; i < SPRITES_MAX; i++) {
        const HdSprite *c = &f->sprites[i];
        if (!c->drawn || matched[i]) continue;
        int best = 48 * 48, cx = c->x + c->w / 2, cy = c->y + c->h / 2;
        for (int k = 0; k < SPRITES_MAX; k++) {
            if (!matched[k]) continue;
            const HdSprite *m = &f->sprites[k];
            int ex = m->x + m->w / 2 - cx, ey = m->y + m->h / 2 - cy, d = ex * ex + ey * ey;
            if (d < best) { best = d; dx[i] = dx[k]; dy[i] = dy[k]; }
        }
        /* None close by (one close is of its own fighter): a piece of the frame before. */
        if (best <= 24 * 24) continue;
        best = 24 * 24;
        for (int j = 0; j < SPRITES_MAX; j++) {
            if (!prev[j].drawn) continue;
            int ex = prev[j].x + prev[j].w / 2 - cx, ey = prev[j].y + prev[j].h / 2 - cy, d = ex * ex + ey * ey;
            if (d < best) { best = d; dx[i] = prev[j].dx; dy[i] = prev[j].dy; }
        }
    }
    /* A figure (sprites touching each other) moves as one: by the move most of its matched
     * pixels agree on. A new step of an animation brings pieces found elsewhere in the
     * frame before (a pipe's repeated tiles, a foot) and pieces not found at all, which
     * glided from wrong places on their own: legs twice, a pipe's end apart (issue #5).
     * A piece found as itself and going on as it went the frame before keeps its own move:
     * it may be another object the figure touches, an apple or a crate the fighter walks
     * onto, which glided along with the fighter and jumped back (it jittered). */
    int group[SPRITES_MAX];
    for (int i = 0; i < SPRITES_MAX; i++) group[i] = i;
    for (int i = 0; i < SPRITES_MAX; i++) {
        const HdSprite *a = &f->sprites[i];
        if (!a->drawn) continue;
        for (int j = i + 1; j < SPRITES_MAX; j++) {
            const HdSprite *b = &f->sprites[j];
            if (!b->drawn || a->x > b->x + b->w + 1 || b->x > a->x + a->w + 1 || a->y > b->y + b->h + 1 || b->y > a->y + a->h + 1) continue;
            int ri = i, rj = j;
            while (group[ri] != ri) ri = group[ri];
            while (group[rj] != rj) rj = group[rj];
            if (ri != rj) group[ri > rj ? ri : rj] = ri < rj ? ri : rj;
        }
    }
    for (int i = 0; i < SPRITES_MAX; i++) {
        int r = i;
        while (group[r] != r) r = group[r];
        group[i] = r;
    }
    for (int g = 0; g < SPRITES_MAX; g++) {
        if (group[g] != g || !f->sprites[g].drawn) continue;
        int votes = 0, total = 0, best = 0, bx = 0, by = 0, members = 0;
        for (int i = g; i < SPRITES_MAX; i++) {
            if (group[i] != g || !f->sprites[i].drawn) continue;
            members++;
            if (!matched[i]) continue;
            int w = f->sprites[i].w * f->sprites[i].h, same = 0;
            total += w;
            for (int k = g; k < SPRITES_MAX; k++)
                if (group[k] == g && matched[k] && dx[k] == dx[i] && dy[k] == dy[i]) same += f->sprites[k].w * f->sprites[k].h;
            if (same > best) { best = same; bx = dx[i]; by = dy[i]; }
            votes++;
        }
        if (members < 2 || !votes || best * 2 < total) continue;
        for (int i = g; i < SPRITES_MAX; i++)
            if (group[i] == g && f->sprites[i].drawn && !steady[i]) { dx[i] = bx; dy[i] = by; }
    }
    for (int i = 0; i < SPRITES_MAX; i++) {
        const HdSprite *c = &f->sprites[i];
        prev[i].drawn = c->drawn;
        prev[i].matched = c->drawn && matched[i];
        prev[i].x = c->x; prev[i].y = c->y; prev[i].w = c->w; prev[i].h = c->h;
        prev[i].dx = c->drawn ? dx[i] : 0; prev[i].dy = c->drawn ? dy[i] : 0;
        prev[i].picture = pic[i];
    }
    return ++serial;
}

unsigned motion_sprite_moves(const HdFrame *f, int dx[SPRITES_MAX], int dy[SPRITES_MAX], int *owned) {
    memset(dx, 0, SPRITES_MAX * sizeof *dx);
    memset(dy, 0, SPRITES_MAX * sizeof *dy);
    if (owned) *owned = 0;
    if (!GAME_KNOWS_RAM) return frame_moves(f, dx, dy, owned);
    const Tick *now = shown_tick(f);
    if (!now) return 0;
    const Tick *was = tick_numbered(now->serial - 1);
    for (int i = 0; i < SPRITES_MAX; i++) {
        if (!f->sprites[i].drawn || now->anchor[i] < 0) continue;
        if (owned) ++*owned;
        if (!was) continue;
        const Anchor *a = &now->anchors[now->anchor[i]];
        for (int k = 0; k < was->anchor_count; k++) {
            const Anchor *b = &was->anchors[k];
            if (b->object != a->object || b->nth != a->nth || b->kind != a->kind) continue;
            int mx = a->x - b->x, my = a->y - b->y;
            if (abs(mx) <= MAX_JUMP && abs(my) <= MAX_JUMP) { dx[i] = mx; dy[i] = my; }
            break;
        }
    }
    return now->serial;
}
