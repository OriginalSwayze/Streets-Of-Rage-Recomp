## What's new in 2.8: Bare Knuckle III DX, and figures that glide in one piece

**Bare Knuckle III DX (#22).** The Director's Cut fan hack of Bare Knuckle III (version 1.5) is now an edition of Streets of Rage 3, next to the USA cartridge, Bare Knuckle III and its English translation. It puts back areas cut from the cartridge (round 6's extra rooms, the motorbike stages of rounds 7A and 7B), has Red Comet's English translation, and gives Shiva new moves. To add it, run Setup and give the new **Bare Knuckle III DX** row the hack's patch (`BKIII 1.5.ips`, or a zip with it). Setup applies it to your Bare Knuckle III (Japan) ROM, so pick that one too, and the USA ROM it is built on. Then choose it under Edition in the launcher's Game tab or the pause menu's More tab.

- It plays with the collection's options, widescreen and cheats included.
- In widescreen the motorbike stages show the whole wide road. The hack has its own level select, listing every restored scene.
- The hack is played as it was left, with its own unfinished parts: bosses picking up weapons, a few sprite mix-ups, and round 6 ending once the general is saved.

Fixed:

- **High frame rate: a fighter's sprites came apart for a frame (#5).** When an animation changed step, pieces of a figure could glide to the wrong place between the game's updates. Walking with a pipe in Streets of Rage 2 showed the legs twice and the pipe's end loose. The sprites of one figure now move together.

Checked, not changed:

- All three games, and all four Streets of Rage 3 editions, still match the interpreter frame for frame.

Not fixed yet:

- **Round 6's screen unlock glitch (#20)** and **sprites left over into the ending (#21)**: the test player still can't reach them. A save state from the spot (F2, from the game's `states` folder) would let them be reproduced.
- **The background jitters at the start of a level (#17):** the test frames show no shift.

Everything from 2.7 is in this release.
