/* Built-in cheats: RAM values held every frame, like a Pro Action Replay, plus recompiler
 * patches (config/patches.json).
 *
 * Streets of Rage (docs/ram.md): sorHit routes every hit's damage through sor_hit
 * (recomp_rt.h) for infinite health and the attack power; lives ($FFFF20/23), specials
 * ($FFFF21/24) and the round timer ($FFFB01) are held in RAM.
 *
 * Streets of Rage 2 (docs/ram-sor2.md): sor2Hit does the same for its hits and for what a
 * special move costs; health (+$80), lives (+$83) and the timer ($FFFC3C) are held in RAM.
 *
 * Streets of Rage 3 (docs/ram-sor3.md): sor3Hit for the hits; health (+$6C, its maximum
 * +$6D) and lives (+$8B) are held in RAM. It has no round timer.
 *
 * Beyond Oasis: playerDamage for infinite health, hitDamage for the attack power (double,
 * quadruple, one-hit kills), playerSpeed for the move speed. RAM used:
 *   $FF19E8         player object; word +$00 is 2 while the player exists
 *   $FF1A70/$FF1A72 max / current health  (player +$88/+$8A)
 *   $FF1A74/$FF1A76 max / current spirit  (player +$8C/+$8E; max is 0 before the first spirit)
 *   $FF0D80         8 weapon slots of (id, uses); uses $FF = unbreakable (the knife) */
#include "cheats.h"

#include <stdlib.h>
#include <string.h>

#include "genesis.h"
#include "settings.h"
#include "recomp_rt.h"

int patch_no_damage;
int patch_attack;
int patch_free_specials;
int patch_speed = 100;
int patch_random;

typedef struct { uint16_t addr; uint16_t value; bool word; } Code;

static Code codes[64];                  /* --poke codes */
static int code_count;
static Code user_codes[64];             /* settings.cheat_codes */
static int user_count;
static char user_spec[sizeof settings.cheat_codes];
static uint8_t weapon_id[8], weapon_uses[8];

static int parse(const char *spec, Code *out, int *count, int cap) {
    int added = 0;
    while (*spec && *count < cap) {
        char *end;
        unsigned long a = strtoul(spec, &end, 16);
        if (*end != ':') break;
        const char *v = end + 1;
        unsigned long val = strtoul(v, &end, 16);
        if (end == v) break;
        out[(*count)++] = (Code){ (uint16_t)(a & 0xFFFF), (uint16_t)val, end - v > 2 };
        added++;
        while (*end == ' ') end++;
        spec = *end == ',' ? end + 1 : end;
        while (*spec == ' ') spec++;
    }
    return added;
}

int cheats_add_codes(const char *spec) { return parse(spec, codes, &code_count, 64); }
void cheats_clear_codes(void) { code_count = 0; }

static void apply(const Code *c, int n);
void cheats_poke_once(const char *spec) {
    Code once[16];
    int n = 0;
    parse(spec, once, &n, 16);
    apply(once, n);
}

static uint16_t peek16(uint16_t a) { return (uint16_t)(bus.ram[a] << 8 | bus.ram[(uint16_t)(a + 1)]); }
static void poke16(uint16_t a, uint16_t v) { bus.ram[a] = (uint8_t)(v >> 8); bus.ram[(uint16_t)(a + 1)] = (uint8_t)v; }

static void apply(const Code *c, int n) {
    for (int i = 0; i < n; i++) {
        if (c[i].word) poke16(c[i].addr, c[i].value);
        else bus.ram[c[i].addr] = (uint8_t)c[i].value;
    }
}

/* Hold a current value at its max, when the max looks like a real one. */
static void fill(uint16_t max_addr, uint16_t cur_addr) {
    uint16_t max = peek16(max_addr);
    if (max > 0 && max < 1000 && peek16(cur_addr) < max) poke16(cur_addr, max);
}

/* Weapons lose a use per hit (sub at $8530) and vanish at 0. Restore the uses they had,
 * so turning the cheat off leaves the real counts; a weapon on its last use gets 2. */
static void hold_weapons(void) {
    for (int k = 0; k < 8; k++) {
        uint8_t id = bus.ram[0x0D80 + 2 * k], uses = bus.ram[0x0D81 + 2 * k];
        bool breakable = id >= 2 && id <= 0x0C && uses != 0xFF;
        if (breakable && id == weapon_id[k] && uses < weapon_uses[k]) bus.ram[0x0D81 + 2 * k] = uses = weapon_uses[k];
        if (breakable && uses == 1) bus.ram[0x0D81 + 2 * k] = uses = 2;
        weapon_id[k] = id;
        weapon_uses[k] = uses;
    }
}

/* ---- Randomizer -------------------------------------------------------------------------- *
 * Item IDs (the game's own names): $01-$0C weapons (Knife, Sword, Broad, Death, Omega, Bow,
 * Fire, Metal, Bomb, Power, Hyper, Atm Bow), $0D-$10 keys, $11 HP Up, $12-$15 the spirit
 * stones, $16-$19 lasting upgrades, $1A-$1D the spirit calls, $1E-$29 the restoratives
 * (HP, SP, HP+SP: 1/4, 1/2, 3/4, Max), $2A Elixir, $31 L. Cube. Only restoratives (and, at
 * the higher setting, weapons) are swapped, each for one of its own kind: the keys, upgrades,
 * stones, calls, the Knife Ali starts with and the Bomb (which the way on may need) never
 * change, so the game can always be finished. */
static const uint8_t restoratives[] = { 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A };
static const uint8_t weapons[] = { 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C };

static bool in(const uint8_t *set, size_t n, uint32_t id) { for (size_t i = 0; i < n; i++) if (set[i] == id) return true; return false; }

uint32_t random_drop_swap(uint32_t item, uint32_t place) {
    item &= 0xFFFF;
    uint32_t id = item >> 8;
    if (item >= 0x8000 || !id) return item;           /* the one-time table, or nothing */
    const uint8_t *set;
    size_t n;
    if (in(restoratives, sizeof restoratives, id)) { set = restoratives; n = sizeof restoratives; }
    else if (patch_random >= 2 && in(weapons, sizeof weapons, id)) { set = weapons; n = sizeof weapons; }
    else return item;
    /* The same seed, area, place and item always give the same: a chest keeps its prize. */
    uint32_t h = (uint32_t)settings.randomizer_seed * 2654435761u;
    for (uint16_t a = 0x16F6; a < 0x16FE; a++) h = (h ^ bus.ram[a]) * 16777619u;
    h = (h ^ place) * 16777619u;
    h = (h ^ item) * 16777619u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
    return (uint32_t)set[h % n] << 8 | (item & 0xFF);
}

/* Streets of Rage (docs/ram.md). */
static void cheats_sor1(void) {
    /* Damage goes through sor_hit (recomp_rt.h): none for the players with infinite
     * health, more from their hits with the attack power. */
    patch_no_damage = settings.cheat_health;
    patch_attack = settings.cheat_attack;
    static uint8_t timer_held;
    uint8_t in = bus.ram[0xFF18];
    if (bus.ram[0xFF2A] || !(in & 3)) timer_held = 0;      /* the demo, or between games */
    else {
        for (int p = 0; p < 2; p++) {
            if (!(in & (1 << p))) continue;
            uint16_t obj = p ? 0xB880 : 0xB800, hp = peek16(obj + 0x32);
            if (settings.cheat_health && peek16(obj) && hp < 0x50) poke16(obj + 0x32, 0x50);
            if (settings.cheat_lives && bus.ram[0xFF20 + 3 * p] < 3) bus.ram[0xFF20 + 3 * p] = 3;
            /* Not in the last two rooms (round 8, part 5 on: the two bosses and Mr. X): there
             * the police car never finishes and the game waits for it for good, in the
             * interpreter too, so the cheat takes the call away rather than give one. */
            if (settings.cheat_specials) {
                bool no_car = bus.ram[0xFF03] == 7 && bus.ram[0xFF05] >= 5;
                if (no_car) bus.ram[0xFF21 + 3 * p] = 0;
                else if (!bus.ram[0xFF21 + 3 * p]) bus.ram[0xFF21 + 3 * p] = 1;
            }
        }
        /* The timer never goes down: held at the highest it has been this stretch (it
         * starts each part of a round full). */
        uint8_t t = bus.ram[0xFB01];
        if (!settings.cheat_time || t > timer_held) timer_held = t;
        else if (t < timer_held) bus.ram[0xFB01] = timer_held;
    }
}

/* Streets of Rage 2 (docs/ram-sor2.md). */
static void cheats_sor2(void) {
    patch_no_damage = settings.cheat_health;
    patch_attack = settings.cheat_attack;
    patch_free_specials = settings.cheat_specials;
    static uint8_t timer_held;
    if (bus.ram[0xFD10]) { timer_held = 0; return; }            /* the demo */
    for (int p = 0; p < 2; p++) {
        uint16_t obj = p ? 0xF000 : 0xEF00;
        uint16_t state = peek16(obj);
        if (!state || state == 0x34 || state == 0x36) continue;  /* not playing ($34/$36: not joined) */
        if (settings.cheat_health && peek16(obj + 0x80) && peek16(obj + 0x80) < 0x68) poke16(obj + 0x80, 0x68);
        if (settings.cheat_lives && bus.ram[obj + 0x83] < 2) bus.ram[obj + 0x83] = 2;   /* the HUD shows one more */
    }
    /* The timer (BCD seconds) never goes down: held at the highest it has been since the
     * last refill. */
    uint8_t t = bus.ram[0xFC3C];
    if (!settings.cheat_time || t > timer_held) timer_held = t;
    else if (t < timer_held) bus.ram[0xFC3C] = timer_held;
}

/* Streets of Rage 3 (docs/ram-sor3.md). */
static void cheats_sor3(void) {
    patch_no_damage = settings.cheat_health;
    patch_attack = settings.cheat_attack;
    if (bus.ram[0xFB13]) return;                                 /* the demo */
    for (int p = 0; p < 2; p++) {
        uint16_t obj = p ? 0xE000 : 0xDF00, state = peek16(obj);
        if (!state || state == 0x5C || state == 0x5E) continue;  /* not playing */
        uint8_t hp = bus.ram[obj + 0x6C], max = bus.ram[obj + 0x6D];
        if (settings.cheat_health && hp && hp < max) bus.ram[obj + 0x6C] = max;
        if (settings.cheat_lives && bus.ram[obj + 0x8B] < 2) bus.ram[obj + 0x8B] = 2;
    }
}

void cheats_frame(void) {
    if (!GAME_KNOWS_RAM) {
        if (strcmp(user_spec, settings.cheat_codes)) {
            memcpy(user_spec, settings.cheat_codes, sizeof user_spec);
            user_count = 0;
            parse(user_spec, user_codes, &user_count, 64);
        }
        if (GAME_SOR == 1) cheats_sor1();
        if (GAME_SOR == 2) cheats_sor2();
        if (GAME_SOR == 3) cheats_sor3();
        apply(user_codes, user_count);
        apply(codes, code_count);
        return;
    }
    if (strcmp(user_spec, settings.cheat_codes)) {
        memcpy(user_spec, settings.cheat_codes, sizeof user_spec);
        user_count = 0;
        parse(user_spec, user_codes, &user_count, 64);
    }
    bool player = peek16(0x19E8) == 2;
    patch_no_damage = settings.cheat_health;
    patch_attack = settings.cheat_attack;
    patch_random = settings.randomizer;
    static const int speeds[3] = { 100, 150, 200 };
    patch_speed = speeds[settings.cheat_speed >= 0 && settings.cheat_speed < 3 ? settings.cheat_speed : 0];
    if (player && settings.cheat_health) fill(0x1A70, 0x1A72);
    if (player && settings.cheat_spirit) fill(0x1A74, 0x1A76);
    if (player && settings.cheat_weapons) hold_weapons();
    else memset(weapon_id, 0, sizeof weapon_id);
    apply(user_codes, user_count);
    apply(codes, code_count);
}


/* ---- Hidden options ------------------------------------------------------------------------ *
 * Each game's Options screen has entries its makers kept behind a code; the code sets a flag
 * the screen reads. With cheat_unlock the flag is set where the code would set it, as the
 * screen starts, so whatever pads are held:
 *   Streets of Rage:   A+B+C+Right held on controller 2 as Options is picked ($001164) sets
 *                      $FFFA7B: the round select and the lives setting.
 *   Streets of Rage 2: A+B held on controller 2 as the Options screen starts ($00FC58) sets
 *                      $FFA902: Very Easy and Mania, up to 9 lives and the stage select.
 *   Streets of Rage 3: Up+B held as Options is picked on the title menu ($013DC2 in the US
 *                      edition: game mode $1C) sets $FFF525: the stage select. Found by its
 *                      bytes, as the Japanese editions have it elsewhere. */
static uint32_t unlock_original(uint32_t pc) {
    RecompBlock fn = machine_generated(pc);
    return fn ? fn(pc) : (pc | BAIL);
}

#if GAME_SOR == 1
static uint32_t unlock_hook(uint32_t pc) {
    if (settings.cheat_unlock) bus.ram[0xFA7B] = 1;
    return unlock_original(pc);
}
#elif GAME_SOR == 2
static uint32_t unlock_hook(uint32_t pc) {
    if (settings.cheat_unlock && !peek16(0xA902)) poke16(0xA902, 1);
    return unlock_original(pc);
}
#elif GAME_SOR == 3
static uint16_t unlock_flag;
static uint32_t unlock_hook(uint32_t pc) {
    if (settings.cheat_unlock && peek16(0xFB00) == 0x1C) bus.ram[unlock_flag] = 1;
    return unlock_original(pc);
}
#endif

void cheats_hooks(void) {
#if GAME_SOR == 1
    machine_override(0x001174, unlock_hook);   /* after the check at $001164, either way */
#elif GAME_SOR == 2
    machine_override(0x00FC80, unlock_hook);   /* after the check at $00FC58, either way */
#elif GAME_SOR == 3
    /* cmpi.w #$1C,($FB00).w; bne.s; btst #4,($F704).w; beq.s; btst #0,($F704).w; beq.s;
     * move.b #1,($xxxx).w; move.l #sound,d0; jsr (sound).l: hooked after it, where the
     * ways meet. */
    static const uint8_t code[] = { 0x0C, 0x78, 0x00, 0x1C, 0xFB, 0x00, 0x66, 0x22, 0x08, 0x38, 0x00, 0x04, 0xF7, 0x04,
                                    0x67, 0x1A, 0x08, 0x38, 0x00, 0x00, 0xF7, 0x04, 0x67, 0x12, 0x11, 0xFC, 0x00, 0x01 };
    for (uint32_t a = 0; a + 42 <= ROM_SIZE; a += 2)
        if (!memcmp(bus.rom + a, code, sizeof code)) {
            unlock_flag = (uint16_t)(bus.rom[a + 28] << 8 | bus.rom[a + 29]);
            machine_override(a + 42, unlock_hook);
            break;
        }
#endif
}
