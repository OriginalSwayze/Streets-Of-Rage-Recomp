## What's new in 2.5.2: the launcher's music, and a closer look at your reports

Fixed:

- **Streets of Rage 3, the launcher's soundtrack player with Overclock on** (issue #11): the player lost instrument settings, so FM channels played wrong or not at all, and one song nearly stopped. It now always runs at the console's own speed, whatever the Overclock setting, for all three games. Checked at 300%: four of the songs that went wrong now play the same notes as the game's own sound test. At normal speed all 36 songs already matched it.
- **Streets of Rage 3, widescreen, round 1's pier** (issue #8): a piece of the ship's hull hung at the top left of the screen as you walked on. The layer it is on stops scrolling there, with the hull just outside the 4:3 picture. A layer that holds still like this now keeps out of the sides, at any position.
- **Bare Knuckle III editions run less on the interpreter:** 2.5.1's work had not reached them. They now spend 0.16% of their time interpreting in the first 20,000 frames, instead of about 1%. More of each game was recompiled too, from later rounds: 1,264 new instructions in Streets of Rage, 5,314 in 2 and 5,038 in 3.

Checked, not changed:

- **Streets of Rage 3's character select** (issue #11): its song matched the sound test register for register at 100% and 300%, on the USA edition, so no channel went dead here. If it still happens, the edition, settings and the instrument that drops out would help.
- **Max's voice in Streets of Rage 2** (issue #11): it played in full on every thunder tackle: 30 of 30 at 100%, 30 of 30 at 300%, 38 of 38 with music files. Any later sample on that channel would replace it, as the game's driver has no priority for samples, but that did not happen in these tests.
- **Played through with the test pilot, in widescreen:** Streets of Rage 2 to its final boss, ending and credits; Streets of Rage 3 into each of rounds 4 to 7; Streets of Rage into rounds 5, 7 and 8. The final bosses and endings of Streets of Rage and 3 were not reached.

Everything still plays exactly as the interpreter does, frame by frame over 20,000 frames of play, in all three games and both Bare Knuckle III editions.
