/* Helpers used by the generated code in generated/recomp_*.c.
 * Generated code operates directly on Musashi's CPU state, so control can pass
 * between recompiled blocks and the interpreter at any instruction boundary.
 * Flags use Musashi's encoding: N/V in bit 7, C/X in bit 8, Z set when FLAG_Z == 0. */
#ifndef RECOMP_RT_H
#define RECOMP_RT_H

#include "m68kcpu.h"
#include "genesis.h"
#include "game.h"

extern unsigned char m68ki_cycles[][0x10000];
/* An instruction's cycles are charged before its body runs, where the interpreter charges
 * them after; recomp_charged keeps them, so a VDP status or HV read inside the instruction
 * sees the same point in the line as the interpreter (machine_cycles_in_line). */
extern int recomp_charged;
#define CYC(op) (recomp_charged = m68ki_cycles[0][op], USE_CYCLES(recomp_charged))

#define D(n) REG_D[n]
#define BAIL 0x80000000u                /* returned with a PC: interpret this instruction */
#define A(n) REG_A[n]

/* Runs native code starting at pc (a block start or any instruction inside it). */
typedef uint32_t (*RecompBlock)(uint32_t pc);
typedef struct RecompEntry { uint32_t pc; RecompBlock fn; } RecompEntry;

/* Patched instructions (config/patches.json); 0 keeps the original behavior. */
extern uint32_t patch_cull_left, patch_cull_widen;
extern int patch_cam_margin;
extern uint32_t patch_sor_x_lo, patch_sor_x_hi;   /* Streets of Rage sprite culling (sorCull) */
uint32_t sor_table_x(uint32_t type, uint32_t x);  /* Streets of Rage: where a level-table object goes (widescreen.c) */
extern int patch_sprite_cap;                       /* Streets of Rage 3: sprite list entries (sor3SpriteList) */
extern int patch_sor_wide;                         /* Streets of Rage: widescreen pixels per side (sorWide) */
extern int patch_sor_reach;                        /* how far past the original edges the players may walk */
extern int patch_sor_reach_right;                  /* and past the right one (less where the map ends: SoR2, SoR3) */
extern int patch_sor_reach_left;                   /* SoR3: past the left one (0 where the view keeps to 4:3) */
extern int patch_sor_cull_left, patch_sor_cull_right; /* sprites drawn this far past the original edges (sorCullList) */
extern int patch_sor_view_left, patch_sor_view_right; /* SoR3: how far past the original edges the view shows */
extern int patch_sor_x_cap;                        /* and never at hardware X this or more ($200, or more: sprite_x_wide) */
extern int patch_no_damage;             /* infinite health cheat (cheats.c) */
extern int patch_attack;                /* attack power cheat: 0 normal, 1 double, 2 quadruple, 3 one-hit kills */
extern int patch_free_specials;         /* Streets of Rage 2: special moves cost no health */
extern int patch_speed;                 /* move speed cheat: the player's movement per tick in percent (100 = normal) */
extern int patch_random;                /* randomizer: 0 off, 1 restoratives, 2 restoratives and weapons */

/* The randomizer (cheats.c): an item about to come into the world (item << 8 | count), swapped
 * for another of its kind by the seed, the area and `place`; unchanged when off. */
uint32_t random_drop_swap(uint32_t item, uint32_t place);
static inline uint32_t random_drop(uint32_t item, uint32_t place) {
    return patch_random ? random_drop_swap(item, place) : item;
}

/* Replaces the code at pc with hand-written C (machine.c); NULL restores the generated code. */
void machine_override(uint32_t pc, RecompBlock fn);
/* The recompiled code at pc, whether or not an override replaces it (NULL if none). */
RecompBlock machine_generated(uint32_t pc);

/* ---- Memory (work RAM fast path) ------------------------------------------ */
static inline uint32_t rd8(uint32_t a) {
    a &= 0xFFFFFF;
    return a >= 0xE00000 ? bus.ram[a & 0xFFFF] : bus_read8(a);
}
static inline uint32_t rd16(uint32_t a) {
    a &= 0xFFFFFF;
    if (a >= 0xE00000) { a &= 0xFFFF; return (uint32_t)bus.ram[a] << 8 | bus.ram[a + 1]; }
    return bus_read16(a);
}
static inline uint32_t rd32(uint32_t a) { return rd16(a) << 16 | rd16(a + 2); }
static inline void wr8(uint32_t a, uint32_t v) {
    a &= 0xFFFFFF;
    if (a >= 0xE00000) bus.ram[a & 0xFFFF] = (uint8_t)v;
    else bus_write8(a, (uint8_t)v);
}
static inline void wr16(uint32_t a, uint32_t v) {
    a &= 0xFFFFFF;
    if (a >= 0xE00000) { a &= 0xFFFF; bus.ram[a] = (uint8_t)(v >> 8); bus.ram[a + 1] = (uint8_t)v; }
    else bus_write16(a, (uint16_t)v);
}
static inline void wr32(uint32_t a, uint32_t v) { wr16(a, v >> 16); wr16(a + 2, v); }

static inline uint32_t sx8(uint32_t v)  { return (uint32_t)(int32_t)(int8_t)v; }
static inline uint32_t sx16(uint32_t v) { return (uint32_t)(int32_t)(int16_t)v; }

/* The damage a hit leaves in its target (hitDamage, config/patches.json): the game's, or
 * more with the attack power cheat for anything but the player (the object at $FF19E8).
 * The flags in the top 4 bits are kept; the damage (low 12) is capped at 12 bits, and a
 * one-hit kill is the target's HP plus its defence. */
static inline uint32_t hit_damage(uint32_t target, uint32_t v) {
    uint32_t damage = v & 0xFFF;
    if (!patch_attack || !damage || (target & 0xFFFFFF) == 0xFF19E8) return v;
    switch (patch_attack) {
    case 1: damage *= 2; break;
    case 2: damage *= 4; break;
    default: damage = rd16(target + 0x8A) + rd16(target + 0x92) + 1; break;
    }
    return (v & 0xF000) | (damage > 0xFFF ? 0xFFF : damage);
}

/* Each Streets of Rage's two player objects and the health in an object (a word, or in
 * Streets of Rage 3 a byte) (docs/ram.md, docs/ram-sor2.md, docs/ram-sor3.md). */
#if GAME_SOR == 3
#define SOR_P1 0xDF00u
#define SOR_P2 0xE000u
#define SOR_HP 0x6Cu
#define SOR_HP_BYTE 1
#elif GAME_SOR == 2
#define SOR_P1 0xEF00u
#define SOR_P2 0xF000u
#define SOR_HP 0x80u
#else
#define SOR_P1 0xB800u
#define SOR_P2 0xB880u
#define SOR_HP 0x32u
#endif
static inline bool sor_player(uint32_t object) {
    uint32_t o = object & 0xFFFF;
    return (object & 0xFF0000) == 0xFF0000 && (o == SOR_P1 || o == SOR_P2);
}

/* Streets of Rage (sorHit, games/<name>/patches.json): the health a hit takes off its
 * target. With infinite health the players lose none; their hits on anything else are
 * scaled by the attack power cheat, a one-hit kill taking all the target's health and one
 * more (below 0, or in Streets of Rage 2 0 or below, is dead). */
static inline uint32_t sor_hit(uint32_t target, uint32_t attacker, uint32_t damage) {
    bool t_player = sor_player(target), a_player = sor_player(attacker);
    if (t_player && patch_no_damage) return 0;
    if (!a_player || t_player || !patch_attack || !damage) return damage;
    switch (patch_attack) {
    case 1: damage *= 2; break;
    case 2: damage *= 4; break;
    default: {
#ifdef SOR_HP_BYTE
        damage = rd8(target + SOR_HP);  /* 0 is dead */
        break;
#else
        int16_t hp = (int16_t)rd16(target + SOR_HP); damage = hp < 0 ? 1u : (uint32_t)hp + 1; break;
#endif
    }
    }
#ifdef SOR_HP_BYTE
    return damage > 0xFF ? 0xFF : damage;
#else
    return damage > 0x7FFF ? 0x7FFF : damage;
#endif
}

/* The health change at $4E6E (sorHit.changePc): d7 is added to the target's health; a
 * hit's is negative, from the object in the target's +$7E. */
static inline uint32_t sor_change(uint32_t target, uint32_t d7) {
    if (!(d7 & 0x8000)) return d7;                         /* health gained */
    uint32_t d = sor_hit(target, 0xFF0000 | rd16(target + 0x7E), (0x10000 - d7) & 0xFFFF);
    return (0x10000 - d) & 0xFFFF;
}

/* The movement of an object for this tick (16.16), scaled by the move speed cheat when
 * it is the player's (the object at $FF19E8). */
static inline uint32_t player_move(uint32_t object, uint32_t move) {
    if (patch_speed == 100 || (object & 0xFFFFFF) != 0xFF19E8) return move;
    return (uint32_t)(int32_t)((int64_t)(int32_t)move * patch_speed / 100);
}

/* Pixels the camera keeps from each map edge: the widescreen extra width, or less
 * when the map is too narrow to keep that much on both sides. */
static inline uint32_t cam_margin(void) {
    int max = (int16_t)rd16(0xFF177A), m = patch_cam_margin;
    if (m > max / 2) m = max > 0 ? max / 2 : 0;
    return (uint32_t)m;
}

/* ---- Flag helpers, generated per operand size ---------------------------- */
#define SIZED(N, MASK, SIGN)                                                              \
static inline void logic##N(uint32_t r) {                                                 \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r & MASK; FLAG_V = 0; FLAG_C = 0;            \
}                                                                                         \
static inline uint32_t add##N(uint32_t s, uint32_t d) {                                   \
    s &= MASK; d &= MASK;                                                                 \
    uint64_t w = (uint64_t)s + d; uint32_t r = (uint32_t)w & MASK;                        \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r;                                           \
    FLAG_V = ((s ^ r) & (d ^ r) & SIGN) ? 0x80 : 0;                                       \
    FLAG_X = FLAG_C = (w > MASK) ? 0x100 : 0;                                             \
    return r;                                                                             \
}                                                                                         \
static inline uint32_t sub##N(uint32_t s, uint32_t d) {  /* d - s */                      \
    s &= MASK; d &= MASK; uint32_t r = (d - s) & MASK;                                    \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r;                                           \
    FLAG_V = ((s ^ d) & (r ^ d) & SIGN) ? 0x80 : 0;                                       \
    FLAG_X = FLAG_C = (s > d) ? 0x100 : 0;                                                \
    return r;                                                                             \
}                                                                                         \
static inline void cmp##N(uint32_t s, uint32_t d) {                                       \
    uint32_t x = FLAG_X; sub##N(s, d); FLAG_X = x;                                        \
}                                                                                         \
static inline uint32_t addx##N(uint32_t s, uint32_t d) {                                  \
    s &= MASK; d &= MASK; uint32_t x = (FLAG_X >> 8) & 1;                                 \
    uint64_t w = (uint64_t)s + d + x; uint32_t r = (uint32_t)w & MASK;                    \
    FLAG_N = (r & SIGN) ? 0x80 : 0; if (r) FLAG_Z = r;  /* Z only ever cleared */         \
    FLAG_V = ((s ^ r) & (d ^ r) & SIGN) ? 0x80 : 0;                                       \
    FLAG_X = FLAG_C = (w > MASK) ? 0x100 : 0;                                             \
    return r;                                                                             \
}                                                                                         \
static inline uint32_t subx##N(uint32_t s, uint32_t d) {                                  \
    s &= MASK; d &= MASK; uint32_t x = (FLAG_X >> 8) & 1;                                 \
    uint32_t r = (d - s - x) & MASK;                                                      \
    FLAG_N = (r & SIGN) ? 0x80 : 0; if (r) FLAG_Z = r;                                    \
    FLAG_V = ((s ^ d) & (r ^ d) & SIGN) ? 0x80 : 0;                                       \
    FLAG_X = FLAG_C = ((uint64_t)s + x > d) ? 0x100 : 0;                                  \
    return r;                                                                             \
}                                                                                         \
/* Shifts and rotates: count already reduced (register count is mod 64). */               \
static inline uint32_t asl##N(uint32_t d, uint32_t n) {                                   \
    d &= MASK; uint32_t r = d, v = 0, c = 0;                                              \
    for (uint32_t i = 0; i < n; i++) {                                                    \
        c = r & SIGN; r = (r << 1) & MASK;                                                \
        if ((r & SIGN) != (c ? SIGN : 0)) v = 1;                                          \
    }                                                                                     \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r; FLAG_V = v ? 0x80 : 0;                    \
    if (n) FLAG_X = FLAG_C = c ? 0x100 : 0; else FLAG_C = 0;                              \
    return r;                                                                             \
}                                                                                         \
static inline uint32_t asr##N(uint32_t d, uint32_t n) {                                   \
    d &= MASK; uint32_t r = d, c = 0;                                                     \
    for (uint32_t i = 0; i < n; i++) { c = r & 1; r = (r >> 1) | (r & SIGN); }            \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r; FLAG_V = 0;                               \
    if (n) FLAG_X = FLAG_C = c ? 0x100 : 0; else FLAG_C = 0;                              \
    return r;                                                                             \
}                                                                                         \
static inline uint32_t lsl##N(uint32_t d, uint32_t n) {                                   \
    d &= MASK; uint32_t r = d, c = 0;                                                     \
    for (uint32_t i = 0; i < n; i++) { c = r & SIGN; r = (r << 1) & MASK; }               \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r; FLAG_V = 0;                               \
    if (n) FLAG_X = FLAG_C = c ? 0x100 : 0; else FLAG_C = 0;                              \
    return r;                                                                             \
}                                                                                         \
static inline uint32_t lsr##N(uint32_t d, uint32_t n) {                                   \
    d &= MASK; uint32_t r = d, c = 0;                                                     \
    for (uint32_t i = 0; i < n; i++) { c = r & 1; r >>= 1; }                              \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r; FLAG_V = 0;                               \
    if (n) FLAG_X = FLAG_C = c ? 0x100 : 0; else FLAG_C = 0;                              \
    return r;                                                                             \
}                                                                                         \
static inline uint32_t rol##N(uint32_t d, uint32_t n) {                                   \
    d &= MASK; uint32_t r = d, c = 0;                                                     \
    for (uint32_t i = 0; i < n; i++) { c = r & SIGN; r = ((r << 1) | (c ? 1 : 0)) & MASK; } \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r; FLAG_V = 0; FLAG_C = c ? 0x100 : 0;       \
    return r;                                                                             \
}                                                                                         \
static inline uint32_t ror##N(uint32_t d, uint32_t n) {                                   \
    d &= MASK; uint32_t r = d, c = 0;                                                     \
    for (uint32_t i = 0; i < n; i++) { c = r & 1; r = (r >> 1) | (c ? SIGN : 0); }        \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r; FLAG_V = 0; FLAG_C = c ? 0x100 : 0;       \
    return r;                                                                             \
}                                                                                         \
static inline uint32_t roxl##N(uint32_t d, uint32_t n) {                                  \
    d &= MASK; uint32_t r = d, x = (FLAG_X >> 8) & 1;                                     \
    for (uint32_t i = 0; i < n; i++) { uint32_t c = (r & SIGN) ? 1 : 0; r = ((r << 1) | x) & MASK; x = c; } \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r; FLAG_V = 0;                               \
    FLAG_X = FLAG_C = x ? 0x100 : 0;                                                      \
    return r;                                                                             \
}                                                                                         \
static inline uint32_t roxr##N(uint32_t d, uint32_t n) {                                  \
    d &= MASK; uint32_t r = d, x = (FLAG_X >> 8) & 1;                                     \
    for (uint32_t i = 0; i < n; i++) { uint32_t c = r & 1; r = (r >> 1) | (x ? SIGN : 0); x = c; } \
    FLAG_N = (r & SIGN) ? 0x80 : 0; FLAG_Z = r; FLAG_V = 0;                               \
    FLAG_X = FLAG_C = x ? 0x100 : 0;                                                      \
    return r;                                                                             \
}

SIZED(b, 0xFFu, 0x80u)
SIZED(w, 0xFFFFu, 0x8000u)
SIZED(l, 0xFFFFFFFFu, 0x80000000u)

/* Writes the low part of a data register, keeping the rest. */
static inline void setb(uint32_t *r, uint32_t v) { *r = (*r & 0xFFFFFF00u) | (v & 0xFF); }
static inline void setw(uint32_t *r, uint32_t v) { *r = (*r & 0xFFFF0000u) | (v & 0xFFFF); }

/* Data-dependent multiply timing, as Musashi computes it for the 68000. */
static inline int mulu_cycles(uint32_t x) { int c = 0; for (x &= 0xFFFF; x; x >>= 1) if (x & 1) c += 2; return c; }
static inline int muls_cycles(uint32_t x) {
    int c = 0; uint32_t f = 0;
    for (uint32_t y = sx16(x); y; y >>= 1) if ((y & 1) != f) { c += 2; f = 1 - f; }
    return c;
}

static inline void push32(uint32_t v) { REG_SP -= 4; wr32(REG_SP, v); }
static inline uint32_t pop32(void) { uint32_t v = rd32(REG_SP); REG_SP += 4; return v; }

#endif
