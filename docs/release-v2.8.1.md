## What's new in 2.8.1: Streets of Rage 3's hidden fighters, and two widescreen fixes

**Hidden fighters cheat.** Streets of Rage 3's secret fighters, without the game's codes and feats. Turn on **Hidden fighters** in the launcher's Cheats tab or the pause menu, then start a new game.

- **Roo** (Victy in Bare Knuckle III) is a fifth portrait on the select screen.
- **Shiva and Ash** are in the game's own fighter choice in the HUD, which a player gets on joining mid-round or continuing: Left and Right go through the fighters. The USA cartridge still has Ash.
- In Bare Knuckle III DX the select screen keeps four portraits, and all three are in the HUD choice.

Fixed:

- **Widescreen: a talk inside a round showed the street at the sides (#20).** Talks drawn for 4:3 in the middle of a round, like round 6's roof after the boss, now keep to 4:3 with the sides masked, and are no longer slid off centre at a map's end. Play is the same either way.
- **Widescreen: sprites from the last fight stayed on screen into the ending (#21).** Story scenes and the ending could still show the extra sprites the wide play area adds past the cartridge's 80. Those are now dropped once the game stops drawing them.

Checked, not changed:

- With the cheats off, all three games and all four Streets of Rage 3 editions still match the interpreter frame for frame. With widescreen, 40,000 frames of rounds 1 to 3 play exactly as before.

Not fixed yet:

- **Round 6's screen unlock glitch (#20, the other parts)**: the half screen left behind after a door unlocks, and the switchboard's top line, haven't shown up in the test runs. A save state from the spot (F2, from the game's `states` folder) would let them be reproduced.
- **The background jitters at the start of a level (#17):** the test frames show no shift.

Everything from 2.8 is in this release.
