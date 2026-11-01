# Streets of Rage 2: RAM map

What the port's game-specific features read and write for Streets of Rage 2 (USA). Found
from the disassembly (`build/sor2/game.asm`) and RAM dumps (`--dump`, `--watch`);
addresses are 68000 RAM. Streets of Rage 1's map is in [ram.md](ram.md).

## Objects

$100-byte slots from `$FFEF00`: player 1 at `$FFEF00`, player 2 at `$FFF000`, then
enemies and items from `$FFF100` to `$FFF900`.

| Offset | Size | What |
|---|---|---|
| `+$00` | word | state: 0 = free; 2 while in play (4 and others while hit or thrown); a player who has not joined is `$34`/`$36` |
| `+$02` | word | world X (`+$04`: its fraction) |
| `+$06` | word | depth: the Y position on the street |
| `+$20` | word | X again (`+$24` depth), the copy the sprite code reads |
| `+$78` | 8 bytes | the character's name in ASCII ("AXEL", "BLAZE", "GALSIA", "Y.SIGNAL", ...); items and props have none |
| `+$80` | word | health: `$68` full for the players, `$18` for a Galsia; 0 or below is dead |
| `+$83` | byte | players: lives left after this one (the HUD shows one more) |
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
