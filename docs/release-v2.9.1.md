## What's new in 2.9.1: fights across the whole wide view, and Bare Knuckle III DX's music

Fixed:

- **Widescreen: no hits past the original 4:3 edges (#27).** All three games still treated the wide part of the view as off screen. Enemies there wouldn't attack, and a hit on a fighter there didn't count. Anything in the wide view now fights as it would inside 4:3, except where a room is held to 4:3.
  - Streets of Rage 3: enemies next to a player at the far right now attack (16 hits in a test, none before), and the player's hits land.
  - Streets of Rage: enemies walking in through the wide view can be hit (21 hits past the old edges in a test run, 3 before).
  - Streets of Rage 2: the enemies' "is my target on screen" check is widened the same way as in Streets of Rage 3.
- **Bare Knuckle III DX: your own music files, and the launcher's music (#26).** The hack's sound driver sits at a different address from the cartridge's, so music files were ignored and the launcher's music player was silent. Both work now.

Checked, not changed:

- In widescreen, all three games still finish every fight and move on through the rounds. With widescreen off, every game and edition still matches the interpreter frame for frame.
- Bare Knuckle III DX's 7A scenery matches the hack's own since 2.8.3 (#25).

Not fixed yet:

- **Sprites flickering in 7A's robot factory when it's crowded (#21).**
- **A light black flicker with high frame rate (Streets of Rage 2),** **round 6's screen unlock glitch (#20, the other parts)** and **the background jitter at the start of a level (#17).**

Everything from 2.9 is in this release. If you have 2.9, the collection offers this update when it opens.
