## What's new in 2.8.2: no stall in Streets of Rage's round 2, and items that keep still

Fixed:

- **Streets of Rage, widescreen: round 2 could stop for good.** An enemy the round keeps out of sight until the camera reaches it was placed too far right in widescreen, past where the screen locks for the fight. The game waited for that enemy, and the enemy waited for the camera, so the street never moved on. It now waits further left and comes in from the new edge. A save made while stuck still has the enemy where 2.8.1 put it, so start the round again, or load an earlier save.
- **High frame rate: an apple or crate jittered when a fighter walked onto it (Streets of Rage 2).** Since 2.8, sprites that touch glide between the game's frames as one figure, and an item under a fighter was taken as part of the fighter: it glided along and jumped back each frame. An object that keeps moving steadily on its own now keeps its own movement, and a fighter's pieces still stay together.

Checked, not changed:

- All three games still match the interpreter frame for frame.

Not fixed yet:

- **A light black flicker with high frame rate (Streets of Rage 2):** it hasn't shown up in the test runs. Whether it is the whole screen or an edge, and whether it comes while the screen scrolls, would help find it.
- **Round 6's screen unlock glitch (#20, the other parts)** and **the background jitter at the start of a level (#17).**

Everything from 2.8.1 is in this release.
