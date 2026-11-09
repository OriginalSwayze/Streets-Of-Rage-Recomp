## What's new in 2.7: a neon look, a clearer Setup, and Streets of Rage 3 in widescreen fixed up

**New look.** The launcher, the pause menu and the collection screen have a night-street style: navy glass panels framed by neon tubes, with sign lettering that glows and flickers now and then. Each game has its own pair of neon colours: pink and teal for Streets of Rage, cyan and magenta for Streets of Rage 2, orange and violet for Streets of Rage 3. The Music tab's player card no longer covers the last rows of the track list.

**Clearer Setup (#15).** Streets of Rage 3's page now has one row per ROM: Streets of Rage 3 (USA), then two optional rows under it for Bare Knuckle III and its English patch. Each row has its own Browse, Skip, file check and message. Bare Knuckle III is built on the USA ROM, so if you pick it without that ROM, Install waits and tells you why. If you choose an edition in the launcher that Setup didn't install, the game names it and says how to add it. Before, it said "Not an edition of the game".

Fixed:

- **Pausing muted your own music files (#23):** a song from the `music` folder now carries on after the pause menu, with the game's own tune staying quiet.
- **Bare Knuckle III's talks and story scenes sat at the left in widescreen (#18, #20, #21):** they are drawn 256 pixels wide. They are now centred, with the sides masked.
- **Streets of Rage 3's ends of rooms in widescreen:**
  - Round 5's rooms no longer start masked or show the next room (#19).
  - The round 3 bar ends at its last block instead of showing the round's start again (#17).
  - The HUD stays put on whichever layer the game draws it on, and copies of it no longer show beside it (round 1's deck).
- **Streets of Rage 3: places drawn for 4:3 now keep to 4:3** (sides masked, players inside the original edges), because enemies wait or graphics sit just past the original edges there (#16, #17, #18):
  - round 1's deck at sunset (the boat)
  - round 3's bulldozer and lift
  - round 4's subway, cart room and shrine
- **Streets of Rage 3: one-screen rooms keep to 4:3** where the wide view can't fill them without a black side: round 2's bar and round 7's lab rooms (#21).
- **Streets of Rage, words of the "ROUND START" title stood in the wide sides** for about a second at the start of each round. The sides now stay masked until the title has gone.

Checked, not changed:

- Streets of Rage (all 8 rounds) and Streets of Rage 2 (all 8 rounds and the ending) were played through again in widescreen, as were rounds 1–7 of Streets of Rage 3 and Bare Knuckle III. With widescreen and the cheats off, all three still match the interpreter frame for frame.

Not fixed yet:

- **Sprites left over from round 7 into the ending (#21):** not reproduced. It seems to need Bare Knuckle III's good-ending route, which the test player cannot play.
- **Round 6's screen unlock glitch (#20):** not reproduced. The test player gets stuck in the round's first corridor.
- **The background jitters at the start of a level (#17):** the test frames show no shift in any game. It may only appear with frame smoothing in the window.

Everything from 2.6.2 is in this release.
