## What's new in 2.9.6: Mr. X speaks English again

Fixed:

- **Streets of Rage: Mr. X asked his question at the end in Japanese (#32).** The cartridge holds both languages and picks one from the console's region, which Mr. X's lines read from a copy the game makes when it starts. The launcher runs the game once before you play, and the game then took the real start for a reset and skipped making that copy, so only his lines came out in Japanese. A start is now a clean power-on, and he asks in English.

Not fixed yet:

- **Sprites flickering in 7A's robot factory when it's crowded (#21).**
- **A light black flicker with high frame rate (Streets of Rage 2),** **round 6's screen unlock glitch (#20, the other parts)** and **the background jitter at the start of a level (#17).**

Everything from 2.9.5 is in this release. If you have 2.9 or later, the collection offers this update when it opens. On Linux with the native build, run the new source's `tools/install.sh` again to update.
