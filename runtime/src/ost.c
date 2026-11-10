/* The sound driver lives at $060000: `jsr $060000` once a frame (from the game's V blank)
 * runs the music, `jsr $060004` with a command in d0 starts a song or sound effect.
 *
 * ost_start() parks the game: the 68000 loops on a `bra.s *` in work RAM with interrupts
 * masked, so the game's code never runs again and nothing else touches the Z80 or the
 * sound chips (the game running its attract demo underneath could leave the Z80 where the
 * driver waits for it forever). Each driver call is then made from outside: the CPU is
 * pointed at the driver with a return address in the cartridge header (never executed by
 * the game), where hand-written code starts the next queued call or goes back to the loop. */
#include "ost.h"

#include "recomp_rt.h"

#if GAME_SOR == 3
/* Streets of Rage 3: the V blank calls $1A8000 once a frame (again on a 50 Hz console, to
 * keep the tempo); `jsr $1A8004` with a request in d0 starts a song ($01xx) or an effect.
 * $1A8008 sets the driver up at power on. Its RAM is $FF9F00-$FFA6FF. Bare Knuckle III DX,
 * rebuilt from a disassembly, has it at $0BBED4 (#26). */
#include <string.h>
uint32_t ost_driver(void) { return game && !strcmp(game->id, "dx") ? 0x0BBED4 : 0x1A8000; }
#define DRIVER_UPDATE  (ost_driver())
#define DRIVER_CMD     (ost_driver() + 4)
/* The songs: $0100-$0123, every one plays (--sound-probe). */
static const uint16_t songs[OST_SONGS] = {
    0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0106, 0x0107,
    0x0108, 0x0109, 0x010A, 0x010B, 0x010C, 0x010D, 0x010E, 0x010F,
    0x0110, 0x0111, 0x0112, 0x0113, 0x0114, 0x0115, 0x0116, 0x0117,
    0x0118, 0x0119, 0x011A, 0x011B, 0x011C, 0x011D, 0x011E, 0x011F,
    0x0120, 0x0121, 0x0122, 0x0123,
};
const char *ost_song_name(int song) { (void)song; return NULL; }
int ost_title_song(void) { return -1; }
#elif GAME_SOR == 2
/* Streets of Rage 2: the V blank calls $040000 then $040004 once a frame (the music, then
 * the PCM channel on the Z80); `jsr $040008` with a request in d0 (d1 = 0) starts a song
 * ($01xx) or a sound effect ($02xx-$07xx, the high byte its priority). $04000C sets the
 * driver up at power on. */
#define DRIVER_UPDATE  0x040000
#define DRIVER_UPDATE2 0x040004
#define DRIVER_CMD     0x040008
/* The songs: every request from $0100 to $011B that plays ($010C and $0110 are silent), in
 * the driver's order (--sound-probe). */
static const uint16_t songs[OST_SONGS] = {
    0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0106, 0x0107,
    0x0108, 0x0109, 0x010A, 0x010B, 0x010D, 0x010E, 0x010F, 0x0111,
    0x0112, 0x0113, 0x0114, 0x0115, 0x0116, 0x0117, 0x0118, 0x0119,
    0x011A, 0x011B,
};
const char *ost_song_name(int song) { (void)song; return NULL; }
int ost_title_song(void) { return -1; }
#elif GAME_SMPS
/* Streets of Rage: Sega's SMPS 68k driver. The V blank calls its update at $072914 once a
 * frame (twice every sixth on a 50 Hz console, bit 6 of $FFFF44, to keep the tempo; this
 * one is 60 Hz); requests go into the 3-byte queue at $FFF00A, which the update reads
 * ($81-$9F songs, $A0-$DF effects, $E1 stops everything). */
#define DRIVER_UPDATE 0x072914
#define DRIVER_CMD    0                 /* (no command entry: requests go in the queue) */
#define QUEUE         0xF00A
#define SMPS_STOP     0xE1

/* The songs in the player's order: the title, the eight rounds, then the rest. */
static const struct { uint8_t id; const char *name; } songs[OST_SONGS] = {
    { 0x8A, "Title" },    { 0x81, "Round 1" }, { 0x8B, "Round 2" }, { 0x84, "Round 3" },
    { 0x8C, "Round 4" },  { 0x85, "Round 5" }, { 0x89, "Round 6" }, { 0x86, "Round 7" },
    { 0x88, "Round 8" },  { 0x87, "Boss" },    { 0x8E, "Round Clear" }, { 0x83, "Intro" },
    { 0x82, NULL },       { 0x8F, NULL },      { 0x90, NULL },      { 0x91, NULL },
};
const char *ost_song_name(int song) { return song >= 0 && song < OST_SONGS ? songs[song].name : NULL; }
int ost_title_song(void) { return 0; }

static uint8_t requests[8];
static int nrequests;
#else
#define DRIVER_UPDATE 0x060000
#define DRIVER_CMD    0x060004
const char *ost_song_name(int song) { (void)song; return NULL; }
int ost_title_song(void) { return -1; }
#endif
int ost_song_of_request(uint16_t request) {
    for (int k = 0; k < OST_SONGS; k++) {
#if GAME_SMPS
        if (songs[k].id == request) return k;
#elif GAME_SOR >= 2
        if (songs[k] == request) return k;
#else
        if (k == request) return k;
#endif
    }
    return -1;
}

#define RETURN_HERE   0x0001FC          /* in the cartridge header */
#if GAME_SOR >= 2
#define PARK          0xFFE000          /* object and sprite tables: the drivers keep to $FF9F00-$FFA6FF */
#define PARK_SP       0xFFE800
#elif GAME_SMPS
#define PARK          0xFFC9C0          /* the idle loop and its stack, in game RAM the driver */
#define PARK_SP       0xFFCF00          /* does not use (its own is $FFF000-$FFF3FF) */
#else
#define PARK          0xFFF000          /* the idle loop, in game RAM the driver does not use */
#define PARK_SP       0xFFFE00
#endif

typedef struct { uint32_t entry; uint16_t d0; } Call;

static Call queue[16];
static int queued;
static bool running, started;

static void start_next(void) {
    Call c = queue[0];
    queued--;
    for (int k = 0; k < queued; k++) queue[k] = queue[k + 1];
    push32(RETURN_HERE);
    D(0) = c.d0;
    D(1) = 0;
    REG_PC = c.entry;
    running = true;
}

static uint32_t back_from_driver(uint32_t pc) {
    (void)pc;
    USE_CYCLES(8);
    running = false;
    if (queued) start_next();
    else REG_PC = PARK;
    return REG_PC;
}

void ost_start(void) {
    machine_override(RETURN_HERE, back_from_driver);
    bus.ram[PARK & 0xFFFF] = 0x60;      /* bra.s * */
    bus.ram[(PARK & 0xFFFF) + 1] = 0xFE;
    m68ki_set_sr(0x2700);
    REG_SP = PARK_SP;
    REG_PC = PARK;
    m68ki_cpu.stopped = 0;
    queued = 0;
    running = false;
    started = true;
#if GAME_SMPS
    nrequests = 0;
#endif
}

void ost_end(void) {
    if (!started) return;
    machine_override(RETURN_HERE, NULL);
    started = false;
}

static bool enqueue(uint32_t entry, uint16_t d0) {
    if (!started || queued >= (int)(sizeof queue / sizeof *queue)) return false;
    queue[queued++] = (Call){ entry, d0 };
    return true;
}

#if GAME_SOR >= 2
bool ost_request(uint16_t id) { return enqueue(DRIVER_CMD, id); }

bool ost_command(uint16_t cmd) {
    if (!cmd) return enqueue(DRIVER_CMD, 0x0000);                     /* stops everything */
    if ((cmd >> 8) == 1) return songs[(cmd & 0xFF) % OST_SONGS] && enqueue(DRIVER_CMD, songs[(cmd & 0xFF) % OST_SONGS]);
    return enqueue(DRIVER_CMD, cmd);
}

void ost_frame(void) {
    enqueue(DRIVER_UPDATE, 0);
#ifdef DRIVER_UPDATE2
    enqueue(DRIVER_UPDATE2, 0);
#endif
    if (!running && queued) start_next();
}
#elif GAME_SMPS
bool ost_request(uint16_t id) { return ost_command(id); }

bool ost_command(uint16_t cmd) {
    if (!started || nrequests >= (int)sizeof requests) return false;
    requests[nrequests++] = !cmd ? SMPS_STOP : (cmd >> 8) == 1 ? songs[(cmd & 0xFF) % OST_SONGS].id : (uint8_t)cmd;
    return true;
}

void ost_frame(void) {
    /* One request a frame: the update keeps only the highest-priority request in its
     * queue and clears the others, so a stop and a song together would lose one. */
    if (nrequests && !bus.ram[QUEUE] && !bus.ram[QUEUE + 1] && !bus.ram[QUEUE + 2]) {
        bus.ram[QUEUE] = requests[0];
        for (int i = 1; i < nrequests; i++) requests[i - 1] = requests[i];
        nrequests--;
    }
    enqueue(DRIVER_UPDATE, 0);
    if (!running && queued) start_next();
}
#else
bool ost_request(uint16_t id) { return ost_command(id); }

bool ost_command(uint16_t cmd) { return enqueue(DRIVER_CMD, cmd); }

void ost_frame(void) {
    enqueue(DRIVER_UPDATE, 0);
    if (!running && queued) start_next();
}
#endif
