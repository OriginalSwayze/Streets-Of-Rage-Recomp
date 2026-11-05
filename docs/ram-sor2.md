# Streets of Rage 2: RAM map

What the port's game-specific features read and write for Streets of Rage 2 (USA). Found
from the disassembly (`build/sor2/game.asm`) and RAM dumps (`--dump`, `--watch`);
addresses are 68000 RAM. Streets of Rage 1's map is in [ram.md](ram.md).

## Objects

$100-byte slots from `$FFEF00`: player 1 at `$FFEF00`, player 2 at `$FFF000`, then
enemies and items from `$FFF100` to `$FFF900`.

| Offset | Size | What |
|---|---|---|
| `+$00` | word | state: 0 = free; 2 while in play (4 and others while hit or thrown, `$30` while swinging a pipe); a player who has not joined is `$34`/`$36` |
| `+$02` | word | world X (`+$04`: its fraction) |
| `+$06` | word | depth: the Y position on the street |
| `+$20` | word | X again (`+$24` depth), the copy the sprite code reads |
| `+$78` | 8 bytes | the character's name in ASCII ("AXEL", "BLAZE", "GALSIA", "Y.SIGNAL", ...); items and props have none |
| `+$80` | word | health: `$68` full for the players, `$18` for a Galsia; 0 or below is dead |
| `+$83` | byte | lives left after this one: the players' (the HUD shows one more), and a boss's (Barbon has 1; ordinary enemies 0) |
| `+$8A` | long | during a hit: the attacker's object |
| `+$8E` | word | during a hit: the damage |
| `+$A8` | word | the health the HUD bar shows (it follows `+$80`) |

Hits take `+$8E` off `+$80` in `sub_00541A` (`sub.w d0,$80(a2)` at `$005460`) and at
`$00E9E0`, the target in a2 and its attacker at `$8A(a2)`. A player's special move costs 8
health when it lands (`subq.w #8,$80(a2)` at `$00AED6` and `$00AFAA`). All four go through
`sor_hit` (games/sor2/patches.json, `sor2Hit`) for the infinite health, free specials and
attack power cheats.

## Game

| Address | Size | What |
|---|---|---|
| `$FFFC18` | word | mode: 0 one player, 1 two players (the title menu); player 2 can only play in a two-player game |
| `$FFFC3C` | byte | round timer, BCD seconds (set to `$99` with `move.w #$9900` at the start of each life) |
| `$FFFD10` | byte | nonzero during the demo: `sub_00089E` then plays the recorded pads from `$FFFD20` |

## The third fighter

`runtime/src/third.c`, with `settings.third_player`. `sub_00B4CC` is the whole fighter update:
a2 the fighter, a1 the other one, d7 the pad (held, pressed), called from the play loops at
`$005DE4`, `$00766A` and `$007A06`. The third runs on it from slot `$FFF600`. Enemy-only loops
and searches are held below that slot (`$00DD9E`, `$00E3B4`, `$00E426`, `$00E508`, `$00E526`,
`$00E752`); the fighters' attacks and the sprite builder cover it as they are. The enemies'
attacks loop over two players (`$004FC2`, extended at `$00511E`), and `$00F282`/`$00F3A8` pick
whom to go for, with `sub_00E624` saying whether player 2 is there.

The V blank streams frames only for `$FFEF00`, `$FFF000` and `$FFF100`, 80 tiles each, and
video memory is full, so the third's frame goes to `vdp_third_tiles`, which sprites reach
through tile numbers from `$600` (the planes' tables: never sprite graphics). All four
fighters share palette 0. A new area clears the slot (`sub_00DD12`): the fighter is put back
with the health and lives it had.

## Widescreen

The game is written for 320 pixels; games/sor2/patches.json moves its screen edges out by the
extra width (`patch_sor_wide`, 0 without widescreen, when every patch is the original
instruction):

| Patch | Where | What |
|---|---|---|
| `sorWideAdd` | `$00B48A`/`$00B48E`, `$00D6D4`/`$00D6D8` | the players' limits, camera + `$10` and camera + `$130` (the left one never before the level's start, the right one never past an area's end: `+r`, `patch_sor_reach_right`) |
| `sorWideAdd` | `$003BF8`, `$003C3A`, `$003C5A` | the background streamer, which draws the next column of the level at camera + `$150` |
| `sorCullList` | `$020DB0`/`$020DB6`, `$021E2C`/`$021E32`, `$021F28`/`$021F2E` | sprite pieces are drawn between screen X `$60` and `$1C0` (+`$80`) |
| `sorWideLoad`, `sorWideRowLen` | `$003CDC`, `$003D2A`, `$003D4E`, `$003D92`; `$003DB2`, `$003DBA`, `$003DD6` | the row drawer (`sub_003CCA`, as the camera moves down) draws 44 cells (352 pixels) from the camera; it starts at the new left edge and draws as many more cells |
| `sorCullList` | `$00E236`/`$00E23E` | `sub_00E21A` creates the enemies of the level's spawn table once they are within -32 to 360 pixels of the screen |

The camera is `$FFFC22`, negated (it counts down as the screen scrolls right). Each stage
(`$FFFC42`, 2 a stage) is a list of sections (`$FFFC44`, 2 a section) in the table at
`$003996` (a word offset per stage, from the word itself), two 4-byte entries a section: the
first word of the first is the section's camera limit, negated. At an area's end (a stage's last section, or
one followed by a new area with a smaller limit: round 2's bridge, the truck's container, the
truck) the map ends at that limit + 320, where the original screen's right edge stops. The
widescreen columns past it hold whatever was drawn there earlier, so `widescreen_frame` draws
them black (`vdp_opts.mask_right`) and the players stop at the map's end.

Each area's map starts at camera X 0 (the camera, `$FFFC22` negated, goes back to 0 for round 1's
bar and for the alley after it). At either end the wide view slides back inside the map
(`vdp_opts.view_slide`, widescreen.c): at an area's start it shows the columns the row drawer
filled when the area began (456 pixels), with sprites past hardware X `$1FF` (the game writes
the whole word; the VDP reads a 10th bit then, `vdp_opts.sprite_x_wide`); at its end the
columns the planes still hold left of the camera. The HUD is the window plane (its top 40
lines, register 18 = 5), which stays put.

Inside round 2's truck plane A holds still (scroll 0, the whole screen) while plane B, the
wall, scrolls with the camera: plane A is a picture drawn for 320 columns, with other graphics
kept beside it (its back door, the city). A plane holding still while the camera moves has its
widescreen columns cleared (`vdp_opts.still_planes`), so plane B shows there.

Player 1's state is `$32` on the round's title card ("STAGE 1 START"), before play begins:
the sides are masked then, keeping it centred.
