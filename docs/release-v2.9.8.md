## What's new in 2.9.8: faster HD pictures, the noise channel's true pitch

Faster:

- **HD pictures take about a quarter less time.** At 6x with sharpening, the neon glow and the HDR look on, a picture went from about 29 ms to about 21 ms of work on the test PC. Putting the layers together reuses the last result while the pixels repeat. The glow is worked out once per row and blended in whole numbers, and sharpening looks its weights up in a table. Frames drop less at high internal resolutions and high refresh rates.
- Sharpening now leaves flat areas and smooth gradients alone, so the glow no longer shows bands. Edges between two colours of the same brightness are no longer outlined either (a thin bright rim the old sharpening added around some dark reds).

Fixed:

- **The noise channel played an octave too high.** The Genesis' sound chip moves its noise on every other count, and the emulation moved it on every count. Hiss, crashes and some drums now sound at the console's pitch.

Not fixed yet:

- **Sprites flickering in 7A's robot factory when it's crowded (#21).**
- **A light black flicker with high frame rate (Streets of Rage 2),** **round 6's screen unlock glitch (#20, the other parts)** and **the background jitter at the start of a level (#17).**

Everything from 2.9.7 is in this release. If you have 2.9 or later, the collection offers this update when it opens. On Linux with the native build, run the new source's `tools/install.sh` again to update.
