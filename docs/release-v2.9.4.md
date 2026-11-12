## What's new in 2.9.4: clean sound under Proton

Fixed:

- **Crackling sound through Proton and Wine (#30).** The games kept no sound in reserve, so whenever the sound device asked for more before the next frame was ready, it ran out and crackled. Wine's sound asks in bigger pieces, so there it happened about 9 times a second. The games now keep about 50 ms of sound ready. In testing it no longer ran out at all, under Wine or on Windows (where it had happened now and then too). The native Linux build gets the same fix.

Not fixed yet:

- **Sprites flickering in 7A's robot factory when it's crowded (#21).**
- **A light black flicker with high frame rate (Streets of Rage 2),** **round 6's screen unlock glitch (#20, the other parts)** and **the background jitter at the start of a level (#17).**

Everything from 2.9.3 is in this release. If you have 2.9 or later, the collection offers this update when it opens. On Linux with the native build, run the new source's `tools/install.sh` again to update.
