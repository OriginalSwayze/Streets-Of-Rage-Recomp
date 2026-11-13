## What's new in 2.9.5: Genesis-style pads' C and Z

Fixed:

- **C and Z couldn't be bound on some Genesis-style controllers (#31).** Many of these pads send C or Z as a shoulder trigger rather than a button, and the Controls page only took buttons. The triggers now bind like buttons: they show as LT and RT, and work in play and in the menus. Existing bindings stay as they are.

Not fixed yet:

- **Sprites flickering in 7A's robot factory when it's crowded (#21).**
- **A light black flicker with high frame rate (Streets of Rage 2),** **round 6's screen unlock glitch (#20, the other parts)** and **the background jitter at the start of a level (#17).**

Everything from 2.9.4 is in this release. If you have 2.9 or later, the collection offers this update when it opens. On Linux with the native build, run the new source's `tools/install.sh` again to update.
