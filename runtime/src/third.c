/* A third fighter (Streets of Rage 2; settings.third_player). The game has two, but its
 * fighter code is one routine, sub_00B4CC, called for each with the fighter in a2, the other
 * one in a1 and its pad in d7, so a third can run on the same code:
 *
 *   Slot     $FFF600, the last enemy slot. The fighters' attacks and the sprite builder go
 *            over all eight slots from $FFEF00 and take it in as they are; the enemies' own
 *            loops are held to the five below it (their entries here).
 *   Update   after player 2's, in the three play loops.
 *   Hits     the enemies' attacks test the two players ($004FC2): the third is added at the
 *            loop's end.
 *   Graphics the V blank copies each player's frame to its own 80 tiles, and video memory
 *            has no room for 80 more: the frame goes to a bank of the renderer's
 *            (vdp_third_tiles), which sprites reach through tile numbers from $600. Those
 *            are the planes' tables in video memory, never sprite graphics.
 *   HUD      the game writes a fighter's score and bars at +$74: a row out of view here. */
#include "third.h"

#include <string.h>

#include "genesis.h"
#include "recomp_rt.h"
#include "settings.h"

uint16_t third_pad;

#if GAME_SOR == 2
#define SLOT      0xFFF600u
#define P1        0xFFEF00u
#define SIZE      0x100
#define HUD_OFF   0x52800003u           /* window plane row 5, below the HUD */

static bool ran;                        /* this frame's third update has been started */
static uint8_t held, timer_a, timer_b, pad_held, pad_pressed;
static bool playing;                    /* it has joined: the slot's clearing does not end that */
static uint16_t saved_health;
static uint8_t saved_lives = 2;

static bool on(void) { return settings.third_player; }
static bool joined(void) { return rd16(SLOT) != 0; }

static uint32_t original(uint32_t pc) {
    RecompBlock fn = machine_generated(pc);
    return fn ? fn(pc) : (pc | BAIL);
}

/* sub_000800 for the third pad: a press of A counts for 20 frames, two taps of a direction
 * make a dash, and a held B charges the special. */
static void pad_word(void) {
    uint8_t now = (uint8_t)(third_pad & ~PAD_START), pressed = (uint8_t)(now & ~held);   /* its Start is for joining */
    held = now;
    pad_held = now;
    pad_pressed = pressed;
    if (timer_a) timer_a--;
    if (pressed & 0x40) timer_a = 0x14;
    if (timer_a) { pad_held |= 0x40; pad_pressed |= 0x40; } else pad_pressed &= (uint8_t)~0x40;
    int dash = (int16_t)rd16(SLOT + 0xAE);
    if (dash > 0) wr16(SLOT + 0xAE, (uint32_t)(dash - 1));
    else wr8(SLOT + 0x48, rd8(SLOT + 0x48) & ~0x10u);
    if (pressed & 0x0C) {
        if (rd16(SLOT + 0xAE)) {
            wr8(SLOT + 0x48, rd8(SLOT + 0x48) | 0x10);
            wr8(SLOT + 0x52, pressed);
        }
        wr16(SLOT + 0xAE, 0x10);
    }
    uint8_t charge = (uint8_t)rd8(SLOT + 0x58);
    if (!(pad_held & 0x10)) {
        if (charge >= 0x3C) { wr8(SLOT + 0x59, 0xFF); timer_b = 0x14; wr8(SLOT + 0x52, pad_held); }
        else if (timer_b) timer_b--;
        else wr8(SLOT + 0x59, 0);
        wr8(SLOT + 0x58, 0);
    } else if (pad_pressed & 0x60) wr8(SLOT + 0x58, 0);
    else {
        if (charge < 0x3C) wr8(SLOT + 0x58, charge + 1u);
        wr8(SLOT + 0x59, 0);
    }
}

/* After player 2's update: the fighter routine, then the health bar's, then back here. */
static uint32_t update_hook(uint32_t pc) {
    if (ran) { ran = false; return original(pc); }
    if (!on() || !joined()) return original(pc);
    ran = true;
    pad_word();
    A(1) = P1 | 0xFF000000u;
    A(2) = SLOT | 0xFF000000u;
    D(7) = (uint32_t)(pad_held << 8 | pad_pressed);
    push32(pc);
    push32(0x0077D4);                   /* sub_0077D4 follows each fighter's update */
    return 0x00B4CC;
}

/* The enemy-only loops, held to the slots below the third fighter's. */
static uint32_t enemies_update(uint32_t pc) {
    if (!on()) return original(pc);
    A(2) = 0xFFFFF100; setw(&D(0), 4);
    return 0x00DDA4;
}
static uint32_t enemies_free(uint32_t pc) {
    if (!on()) return original(pc);
    A(0) = 0xFFFFF500; setw(&D(2), 4);
    return 0x00E42C;
}
/* The same search at $00E3B4, which no call the tracer saw reaches. */
static uint32_t enemies_free2(uint32_t pc) {
    if (!on()) return original(pc);
    push32(D(2));                       /* movem.l d2,-(a7) */
    A(0) = 0xFFFFF500; setw(&D(2), 4);
    return 0x00E3BE;
}
static uint32_t enemies_count(uint32_t pc) {
    if (!on()) return original(pc);
    A(1) = 0xFFFFF100; D(2) = 4; D(0) = 0;
    return 0x00E510;
}
static uint32_t enemies_scan(uint32_t pc) {
    if (!on()) return original(pc);
    A(2) = 0xFFFFF100; D(2) = 4;
    return 0x00E52C;
}
static uint32_t enemies_near(uint32_t pc) {
    if (!on()) return original(pc);
    A(0) = 0xFFFFF100; D(2) = 4;
    return 0x00E758;
}

/* Who an enemy goes for: sub_00F282 and sub_00F3A8 weigh up player 1 (a0) and player 2 (a1).
 * The third fighter takes player 2's place when player 2 is out or further away. */
static bool in_play(uint32_t slot) { uint32_t s = rd16(slot); return s && s < 0x32; }

static int distance(uint32_t a, uint32_t b) {
    int dx = (int16_t)rd16(a + 0x20) - (int16_t)rd16(b + 0x20), dz = (int16_t)rd16(a + 0x24) - (int16_t)rd16(b + 0x24);
    return (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz);
}

static uint32_t rival(void) {
    uint32_t me = A(2);
    if (in_play(SLOT) && (!in_play(0xFFF000) || distance(SLOT, me) < distance(0xFFF000, me))) return 0xFFFFF600;
    return 0xFFFFF000;
}

static uint32_t target_a(uint32_t pc) {
    if (!on()) return original(pc);
    A(0) = 0xFFFFEF00; A(1) = rival();
    return 0x00F28A;
}
static uint32_t target_b(uint32_t pc) {
    if (!on()) return original(pc);
    A(0) = 0xFFFFEF00; A(1) = rival();
    return 0x00F3B0;
}
/* sub_00E624: is player 2 there to go for? Carry set: no. */
static uint32_t rival_there(uint32_t pc) {
    if (!on() || (A(1) & 0xFFFF) != (SLOT & 0xFFFF)) return original(pc);
    FLAG_C = in_play(SLOT) ? 0 : 0x100;
    return pop32();
}

/* The enemies' attacks, past player 2: one turn more, on the third fighter. */
static uint32_t hit_loop_end(uint32_t pc) {
    if (on() && joined() && (A(3) & 0xFFFF) == 0xF000) { A(3) = 0xFFFFF600; return 0x004FC6; }
    return original(pc);
}

void third_hooks(void) {
    static const uint32_t after_p2[] = { 0x005E0C, 0x007692, 0x007A2E };
    for (size_t k = 0; k < sizeof after_p2 / sizeof *after_p2; k++) machine_override(after_p2[k], update_hook);
    machine_override(0x00DD9E, enemies_update);
    machine_override(0x00E426, enemies_free);
    machine_override(0x00E3B4, enemies_free2);
    machine_override(0x00E508, enemies_count);
    machine_override(0x00E526, enemies_scan);
    machine_override(0x00E752, enemies_near);
    machine_override(0x00511E, hit_loop_end);
    machine_override(0x00F282, target_a);
    machine_override(0x00F3A8, target_b);
    machine_override(0x00E624, rival_there);
    ran = playing = false;
    held = timer_a = timer_b = 0;
    saved_health = 0; saved_lives = 2;
}

/* The frame's graphics, as the V blank does for the players: the DMA's length is at +$B4
 * and its source at the frame's data, +$3C. */
static void stream(void) {
    if (!(rd8(SLOT + 0x1E) & 0x20)) return;
    wr8(SLOT + 0x1E, rd8(SLOT + 0x1E) & ~0x20u);
    uint32_t len = rd32(SLOT + 0xB4), f = rd32(SLOT + 0x3C);
    uint32_t words = (len >> 16 & 0xFF) << 8 | (len & 0xFF);
    /* $93xx $94xx: the length; which is which follows the registers' numbers. */
    if ((len >> 24) == 0x93) words = (len & 0xFF) << 8 | (len >> 16 & 0xFF);
    uint32_t src = ((rd16(f + 4) & 0x7F) << 16 | (rd16(f + 2) & 0xFF) << 8 | (rd16(f) & 0xFF)) << 1;
    uint32_t bytes = words * 2;
    if (bytes > sizeof vdp_third_tiles) bytes = sizeof vdp_third_tiles;
    for (uint32_t i = 0; i < bytes; i++) vdp_third_tiles[i] = (uint8_t)rd8(src + i);
}

/* In a round: its first fighter is on the street, in control. */
static bool can_join(void) {
    return rd16(P1) == 2 && !rd8(0xFFFD10) && !(rd8(0xFFFC12) & 0x20);
}

/* The first of the four fighters (+$0C: 0, 2, 4, 6) neither player is. */
static uint32_t free_fighter(void) {
    uint32_t a = rd16(P1 + 0x0C), b = rd16(0xFFF000 + 0x0C);
    bool two = in_play(0xFFF000);
    for (uint32_t c = 0; c < 8; c += 2) if (c != a && !(two && c == b)) return c;
    return a;
}

static void enter(void) {
    memcpy(bus.ram + (SLOT & 0xFFFF), bus.ram + (P1 & 0xFFFF), SIZE);
    uint32_t c = free_fighter();
    wr16(SLOT + 0x0C, c);
    /* The fighter's numbers, as sub_007E9C sets them. */
    wr16(SLOT + 0x66, rd16(0x007ED6 + c * 4));
    wr16(SLOT + 0x68, rd16(0x007ED8 + c * 4));
    wr16(SLOT + 0x6A, rd16(0x007EDA + c * 4));
    wr16(SLOT + 0x0A, VDP_THIRD_TILE);
    wr32(SLOT + 0x74, HUD_OFF);
    wr32(SLOT + 0x86, 0); wr32(SLOT + 0x8A, 0); wr32(SLOT + 0xAA, 0);
    wr16(SLOT + 0x50, 0x78);            /* a moment of safety, as on a new life */
    wr16(SLOT + 0x02, rd16(SLOT + 0x02) + 40);
    wr16(SLOT + 0x20, rd16(SLOT + 0x20) + 40);
    wr16(SLOT + 0x80, saved_health ? saved_health : 0x68);
    wr8(SLOT + 0x83, saved_lives);
    wr8(SLOT + 0x1E, rd8(SLOT + 0x1E) | 0x20);
}

void third_frame(void) {
    vdp_third_on = on();
    if (!on()) return;
    /* A new area clears the enemy slots, this one with them: the fighter comes back, as it
     * was, once the round is under way. Out of lives (state $34 up), it waits for Start. */
    if (!joined()) {
        if (((third_pad & PAD_START) || playing) && can_join()) { enter(); playing = true; }
        return;
    }
    if (rd16(SLOT) >= 0x34) {
        wr16(SLOT, 0);
        playing = false;
        saved_health = 0; saved_lives = 2;
        return;
    }
    if (settings.cheat_health && (int16_t)rd16(SLOT + 0x80) < 0x68) wr16(SLOT + 0x80, 0x68);
    if (settings.cheat_lives && (int8_t)rd8(SLOT + 0x83) < 2) wr8(SLOT + 0x83, 2);
    if (rd16(SLOT) == 2) { saved_health = (uint16_t)rd16(SLOT + 0x80); saved_lives = (uint8_t)rd8(SLOT + 0x83); }
    stream();
}
#else
void third_hooks(void) {}
void third_frame(void) {}
#endif
