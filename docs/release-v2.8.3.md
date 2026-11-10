## What's new in 2.8.3: no leftover sprites, and Bare Knuckle III DX's bike stages in widescreen

Fixed:

- **Streets of Rage 3: scraps of sprites stayed on screen for the rest of the session (#24).** In widescreen, or with the sprite limit off, a crowd uses more than the cartridge's 80 sprites. The extra ones were never cleared from the high-resolution picture, so pieces of fighters stayed over the scenery, the story scenes and the HUD until the game was closed, even after a rewind. Save-state thumbnails never had them. They're cleared every frame now.
- **Bare Knuckle III DX, widescreen: no hits on the motorbike stages (#25).** With the riders let past the original screen edges, neither the bikes' bumps nor the enemies' attacks landed. On the bike stages the riders now keep to the original edges, and hits work as they do in 4:3.
- **Bare Knuckle III DX, widescreen: dark bars across 7A's fields (#25).** The hack draws its own scenery into the road as it goes, and the extra columns widescreen drew covered it. The bike stages are drawn as the hack draws them, and the whole wide road is right.
- **Bare Knuckle III DX, widescreen: 7B's trucks cut off at the old screen edge (#25).** The hack draws the trucks only as far as the 4:3 view goes. While a truck stands across that edge, the right side is black instead of showing the truck cut off.
- **Bare Knuckle III DX:** the long sprite list no longer reads from the hack's ROM, which fills all 4 MB.

Checked, not changed:

- **Music files and the pause (#23)** were fixed in 2.8: the song holds while the game is paused and goes on after.
- All three games, and all four Streets of Rage 3 editions, still match the interpreter frame for frame.

Not fixed yet:

- **A light black flicker with high frame rate (Streets of Rage 2):** not seen in the test runs yet.
- **Round 6's screen unlock glitch (#20, the other parts)** and **the background jitter at the start of a level (#17).**

Everything from 2.8.2 is in this release.
