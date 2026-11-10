# Streets of Rage 3: RAM map

What the port's game-specific features read and write for Streets of Rage 3 (USA). Found
from the disassembly (`build/sor3/game.asm`) and RAM dumps (`--dump`, `--watch`);
addresses are 68000 RAM. Streets of Rage 1's map is in [ram.md](ram.md), 2's in
[ram-sor2.md](ram-sor2.md).

## Objects

$100-byte slots from `$FFDF00`: player 1 at `$FFDF00`, player 2 at `$FFE000`, then
enemies and items from `$FFE100`. `$FFDE00` lists the slots in drawing order.

| Offset | Size | What |
|---|---|---|
| `+$00` | word | state: 0 = free; a player who has not joined is `$5C`/`$5E`, and `$5A` while picking a fighter in the HUD |
| `+$02` | word | world X (`+$04`: its fraction) |
| `+$06` | word | depth: the Y position on the street |
| `+$36` | word | during a hit: the damage (low 6 or 7 bits) |
| `+$60` | word | during a hit: the attacker's object (low word) |
| `+$6C` | byte | health: `$34` full for the players, 0 is dead |
| `+$6D` | byte | the most health it can have: up to `$20` for ordinary enemies, `$30` for round 1's boss |
| `+$6E` | byte | the health the HUD bar shows (it follows `+$6C` a step a frame) |
| `+$8B` | byte | players: lives left after this one |
| `+$C8` | word | during a throw or slam: the damage |

Hits take their damage off `+$6C` with `sub.b dN,$6C(a0)` in many places, one per kind of
attack: from `+$36` at `$00C4E8`, `$011BA6`, `$01EC10`, `$023628`, `$024AB0`, `$026B00`
and `$027E3A`; from `+$C8` (throws) at `$00CF3E`, `$00CFAE`, `$00D11A`, `$00D23A`,
`$00D38E`, `$00D508`, `$00D6C0`, `$00D794` and `$00DD84`; on a1 at `$00CA30`; and one
point at `$02B610` and `$02F0DA` (`subq.b #1`). All go through `sor_hit`
(games/sor3/patches.json, `sor3Hit`); the ones the tracer does not reach alone are in
games/sor3/entries.json. Found by scanning the ROM for the instructions.

## Game

| Address | Size | What |
|---|---|---|
| `$FFF710` | word | camera X (the planes' scroll table at `$FF8E00` holds its negative) |
| `$FFFB13` | byte | nonzero during the demo: the pads are read from the recording at `$FFF7B0` |
| `$FFFF18`, `$FFFF1A` | word | each player's fighter: 0 Ash, 2 Shiva, 4 Zan, 6 Axel, 8 Blaze, `$A` Skate, `$C` Roo (Victy in Bare Knuckle III); `$FFFF` for no player 2 |
| `$FF8850`, `$FF8854` | word | the select screen's cursors (0-3, or 0-4 with Roo), turned into fighters by the table at `$04846A` |
| `$FF88EF` | byte | Roo on the select screen: set by Up+B+Start held on the title (`$013770`), cleared for a new game |
| `$FFFB0C`, `$FFFB0D` | byte | Ash, and Shiva, in the fighter choice a player gets in the HUD on joining or continuing (`$00E980`) |
| `$FF88F0` | byte | both players may pick the same fighter |

The game modes (`$FFFB00`) run from the table at `$00040A`: `$14` sets up the select screen,
`$16` runs it (`$047B32`), `$10` is a level select of every round and scene that no code opens,
`$0E` is play. The cheats' Hidden fighters holds the three flags.

There is no round timer. Player 2 can join a one-player game with Start, as in Streets of
Rage.

## Widescreen

games/sor3/patches.json moves the screen edges out by the extra width (`patch_sor_wide`):

| Patch | Where | What |
|---|---|---|
| `sorWideLimit` | `$0101C0`, `$01024A` | the players' limits, `$20` and `$128` pixels from the camera (the left one never before the level's start) |
| `sorWideAdd`, `sorWideEdge3` | `$03167A`/`$03167E`, `$0318D8`/`$0318DC`; `$03171A`, `$031978` | the planes' column drawers (camera + `$148` - 8 moving right, camera - 8 moving left) and row drawers (64 cells from camera - 8), for plane B (camera `$FF8880`) and A (`$FF8890`) |
| `sorCullList` | `$001B6E`/`$001B74`, `$001BF4`/`$001BFA`, `$002066`/`$00206C`, `$002172`/`$002178` | sprite pieces are drawn between screen X `$60` and `$1C0` (+`$80`); the first pair is the unflipped pieces' (signed compares), the second the flipped ones' |
| `sor3SpriteList` | `$001B48`, `$001BBE`, `$00203C`, `$0020F6`; `$001AD4`, `$002088`, `$002100`; `$001A86`/`$001A8E` | the sprite list at `$FFF200` (count at `$FFF606`) stops at 80 entries, with the objects taken in the opposite order every other frame, so a crowd blinks; it goes on to 127 entries at `SPRITES_EXTRA_ADDR` (runtime/src/genesis.h), which the VDP draws after the 80 |

The enemies' own on-screen tests (camera + 1 to + `$130` at `$01AC22`, and others like it)
are left alone: widening Streets of Rage 1's made waves wait for enemies that never came.

Each area's camera starts at its map's left end (X 0). The map is a table of 256-pixel blocks,
16 a row: plane A's at `$FF8C00` (B's at `$FF8A00`), RAM offsets of each block (32 × 32 cells),
zero past the map's end (round 1's first area is 8 blocks, 2048 pixels; its camera stops at
`$6C0`). At either end the wide view slides back inside the map (`vdp_opts.view_slide`,
widescreen.c), as in Streets of Rage 2, and the players' right limit moves out only as far as
the map goes (`patch_sor_reach_right`). `$FF8864` is the camera's limit for the current
section (set by each round's script), not the map's end, except in an area's last block: there
the map ends where the camera stops, limit + 320 (round 3's bar is two blocks with the camera
held at 96: its art ends at 416).

Each plane has its own camera: `$FF8880` for plane B, `$FF8890` for plane A. The map is the
plane whose camera is the game's (`$FFF710`), mostly plane A. In round 3's bar plane B is the
bar (7 blocks, the camera stopping at 1472) and plane A a foreground going by half as fast
again, its table one pattern repeated over all 16 blocks; in round 5's rooms (each an area of
its own, `$FFFB03`) plane A's table is empty and the room is plane B's two blocks. The round is
`$FFFB05` (from 0), the area in it `$FFFB03`.

The HUD is the top 40 lines (5 strips of the 8-line scroll, scrolled 0; no window plane) of
the plane that is not the map: plane B's mostly, plane A's in the bar and round 5's rooms. They
stay put as the view slides (`vdp_opts.hud_b_lines`, `hud_plane`).

Bare Knuckle III's talks (in a round, and between rounds) are drawn 256 pixels wide (H32): the
renderer centres them in the wide picture (`vdp_side_width`) and masks the sides.

Plane B scrolls in 8-line strips, each at its own speed. On round 1's ship deck the strips at
the deck's edge (lines 112-151) hold still: a picture drawn for 320 columns, with the boat's
graphics kept in the plane beside it (purple blocks in the widescreen columns). Strips that
hold still while the camera has moved on are mirrored into those columns
(`vdp_opts.still_planes`; a strip of plane A holding still, at any scroll, has its sides
cleared instead: on the pier after the deck plane A stops at -831 as the camera goes on, and
the ship's hull beside it would hang at the left of the view).

## Sound

The driver is 68000 code at `$1A8000` (update, once a frame from the V blank), `$1A8004`
(request in d0: `$01xx` songs, `$02xx` voices and hits on the Z80's PCM channel, `$03xx` and
`$04xx` effects) and `$1A8008` (setup). Its RAM is `$FF9F00`-`$FFA6FF`.

| Address | Size | What |
|---|---|---|
| `$FF9F06` | byte | the Z80 was busy (its RAM `$0004`) at the end of the last update: voice requests wait |
| `$FF9F07` | byte | a voice is ready to hand to the Z80 |
| `$FF9F08` / `$FF9F09` / `$FF9F0A` | byte | a waiting voice request: flag, number, priority |
| `$FF9F1C` | byte | the last voice's priority (kept after it ends; cleared only by a stop) |
| `$FFA457` | byte | the PCM channel is playing |

A waiting voice plays only if its priority is at least `$FF9F1C`, even after that voice has
ended. Overclocked, the update ends while the Z80 is busy every frame, so every voice waits,
and after one of priority `$FF` no hit or voice plays again. `sor3VoiceQueue` (`$1A8106`)
compares with 0 there while overclocked and nothing plays or waits, as the request's direct
path does.

## Bare Knuckle III

The Japanese original (games/sor3/game.json, editions `jp` and `en`, the latter with
Twilight Translations' English patch, which leaves the code where it is) has the same RAM
map and the same sound driver at `$1A8000`; its code moved. `tools/map-editions.mjs` finds
the addresses above in it by the USA code around them (games/sor3/editions.json). What the
search cannot find is in game.json:

- `$023628` is at `$02439C`: Bare Knuckle III calls a routine before it, so the branch after
  it is longer (`placed`).
- `$026B00` (a hit that leaves the target at 10 health instead of knocking it out) is not in
  Bare Knuckle III (`absent`).
- It has two hits the USA cartridge does not, both `sub.b d0,$6C(a0)`: `$010EA2` (a quarter
  of the damage when a player is the attacker) and `$0113EC` (`patches`).

Found by scanning both ROMs for every `sub.b dN,$6C(aN)` and `subq.b #N,$6C(aN)`: the USA
cartridge has 33, Bare Knuckle III 34. The console reports itself as Japanese (`$A10001` =
`$20`) for these editions.
