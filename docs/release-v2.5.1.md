## What's new in 2.5.1: more of each game recompiled

Each game was played through again, in widescreen, as far as the test pilot gets: Streets of Rage to round 4, Streets of Rage 2 to round 8, Streets of Rage 3 to round 4. That found code the recompiler had not met yet, which ran on the slower interpreter: 3,728 new instructions in Streets of Rage, 7,160 in 2 and 5,317 in 3. They are now native code, so in those rounds the games spend 0.02-0.04% of their time interpreting instead of up to 2%, which helps on slower PCs. Every game still plays exactly as the interpreter does, checked frame by frame over 20,000 frames of play. That covers the main editions: the Bare Knuckle III editions got this only in 2.5.2.

Also looked at, with nothing found then:

- The widescreen sides, on one screenshot every 5 seconds of those runs, by a colour script and by eye. That is a sample, not every screen, and it missed the ship's hull at Streets of Rage 3's pier, fixed in 2.5.2.
- Streets of Rage 2's sounds at overclock 100% and 300%, playing as Axel for 12,000 frames: no clear difference. See 2.5.2 for Max's voice.

Everything from 2.5 is in this release: the six-button pad for Streets of Rage 3, the hidden options, and the fixes for voices with your own music and for the bonus tally in widescreen.
