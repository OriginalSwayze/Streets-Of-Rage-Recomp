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
| `+$6D` | byte | the most health it can have |
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

There is no round timer. Player 2 can join a one-player game with Start, as in Streets of
Rage.

## Widescreen

games/sor3/patches.json moves the screen edges out by the extra width (`patch_sor_wide`):

| Patch | Where | What |
|---|---|---|
| `sorWideLimit` | `$0101C0`, `$01024A` | the players' limits, `$20` and `$128` pixels from the camera (the left one never before the level's start) |
| `sorWideAdd`, `sorWideEdge3` | `$03167A`/`$03167E`, `$0318D8`/`$0318DC`; `$03171A`, `$031978` | the planes' column drawers (camera + `$148` - 8 moving right, camera - 8 moving left) and row drawers (64 cells from camera - 8), for plane B (camera `$FF8880`) and A (`$FF8890`) |
| `sorCullList` | `$001BF4`/`$001BFA`, `$002066`/`$00206C`, `$002172`/`$002178` | sprite pieces are drawn between screen X `$60` and `$1C0` (+`$80`) |

The enemies' own on-screen tests (camera + 1 to + `$130` at `$01AC22`, and others like it)
are left alone: widening Streets of Rage 1's made waves wait for enemies that never came.
