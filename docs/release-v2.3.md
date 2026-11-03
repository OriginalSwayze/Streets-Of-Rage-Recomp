## What's new in 2.3: widescreen edge to edge

**At the ends of a street, the view slides instead of showing black or leftovers.** Where an area's map starts or ends, the widescreen view used to show past it: in 2.2 leftovers of other scenes, in 2.2.1 black. Now the view stops at the street's end and takes its width from the other side, so the picture is the game's own scenery edge to edge. The game's camera and the HUD don't move; the players still stop at the street's end. Enemies are drawn wherever the view goes.

- **Streets of Rage 2:** at the start and end of every area (each round's start and end, round 1's bar and the alley after it, round 2's bridge end and the truck, and the others like them).
- **Streets of Rage 3 and Bare Knuckle III:** the same, at the start and end of every area.
- **Streets of Rage:** the game draws its own street past both edges, so its picture already reached the sides (checked at the starts and ends of rounds 1 and 2).

**Blurred sides for scenes drawn for 4:3.** The intro, story scenes and anything else with nothing of the game beside it now show the picture's own edge, blurred and dimmed, instead of black. Prefer black? Set **Widescreen sides** to Black (launcher **Effects** tab, pause menu **Effects** tab).

Also fixed:

- **Streets of Rage 2: swinging a pipe no longer counts as being out of the game.** While Axel or the others swung a pipe, the game's state for it looked like "not joined": the widescreen sides blinked, combat info and the fighters' colours paused for those frames, and infinite health skipped them.
