# Streets of Rage: RAM map

What the port's game-specific features read and write. Found from the disassembly
(`build/disasm/game.asm`) and RAM dumps (`--dump`, `--watch`); addresses are 68000 RAM.

## Objects

$80-byte slots from `$FFB800`: player 1 at `$FFB800`, player 2 at `$FFB880`, then enemies,
items and effects from `$FFB900`. A slot is free when its first word is 0.

| Offset | Size | What |
|---|---|---|
| `+$00` | word | type (players `$01xx` / `$02xx`; enemies `$22xx`, `$24xx`, ...); 0 = free |
| `+$10` | word | world X |
| `+$14` | word | depth: the Y position on the street (smaller is further back) |
| `+$1C` | long | X velocity (16.16) |
| `+$20` | long | depth velocity (16.16) |
| `+$28` | word | screen X + `$80` (sprite coordinates) |
| `+$2C` | word | screen Y + `$80` |
| `+$30` | word | state (`$300` = knocked down and dying) |
| `+$32` | word | health. Players start at `$48` of a maximum `$50` (the bar shows health / 8); below 0 is dead |
| `+$34` | byte | attack power: what this object's hits subtract from the target's health |
| `+$37` | byte | bit 1: killed |
| `+$7E` | word | the enemy this player last targeted (low word of its slot address) |

Hits subtract the attacker's `+$34` from the target's `+$32` (`sub.w d0,$32(a0)`, the
attacker in a1) at `$009BD6`, `$00A14E`, `$00A17A`, `$00A1F8`, `$00A9FC`, `$00F08A`,
`$00F396`, `$013FA0`, `$014A58`, `$0155EE` and `$015632`, and 2 at `$00A9F2`. Hits on the
players go through `sub_004E6C` instead: `$3524` takes the attacker from `+$7E`, negates
its power and adds it at `$004E6E` (`add.w d7,$32(a0)`); health pickups add there too, and
it caps at `$50`. Some of these only run in parts of the game the tracer cannot reach on
its own, so they are listed in `config/entries.json`.

## Players

| Address | Size | What |
|---|---|---|
| `$FFFF20` / `$FFFF23` | byte | lives, player 1 / player 2 |
| `$FFFF21` / `$FFFF24` | byte | special attacks (the police car) |
| `$FFFF22` / `$FFFF25` | byte | game over (out of lives) |
| `$FFFF18` | byte | in the game: bit 0 player 1, bit 1 player 2 (a held Start, 6+ frames, joins player 2) |
| `$FFFF2A` | byte | the attract demo is playing (its input comes through `$FFFF2C` / `$FFFF30`) |
| `$FFFF08` | long | player 1 score (BCD, shown with a trailing 0) |

## Round

| Address | Size | What |
|---|---|---|
| `$FFFB01` | byte | round timer (BCD) |
| `$FFFB1E` | word | enemies active |
| `$FFE002` | word | camera X |
