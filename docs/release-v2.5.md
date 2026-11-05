## What's new in 2.5: six buttons, the games' hidden options, and fixes from your reports

- **Streets of Rage 3 reads a six-button pad** (GitHub issue #13), as the game does on a Mega Drive: **Z** is the back attack without B+C, **X** gives the blitz moves at their full strength (X, then the directions), and **Y** works as it does there. Their keys are A, S and D; on a controller, Y and the two stick buttons. Change them on the pause menu's Controls page.
- **Hidden options** (launcher **Cheats** tab, pause menu **Cheats** tab; issue #14): each game's own Options screen gets the entries its makers kept behind a code, with no second controller needed.
  - Streets of Rage: the round select and the number of lives.
  - Streets of Rage 2: Very Easy and Mania, up to 9 lives, and the round select.
  - Streets of Rage 3: the stage select.

Fixed:

- **Streets of Rage, voices with your own music** (issue #11): with a music file playing, the voices went silent, and stayed silent after switching the files off. They play again. The constant level the stopped song can leave under a file is now filtered out instead.
- **Streets of Rage 2 and 3, widescreen** (issue #12): the bonus tally after each stage and Streets of Rage 3's round title ("ROUND 1 START") are centred with blurred (or black) sides, instead of shifted to one side with leftovers of other screens beside them. In Streets of Rage 3 the rule is the game's own mode (play or not), so its story scenes between rounds are drawn the same way.

Checked, not changed:

- **Difficulty and lives** (issue #14): all three games start at Normal with 3 lives, as on the cartridge. In Streets of Rage 3 (USA) the Options screen draws the unselected lives count in dark grey on the grey background, which makes it easy to miss.
- **Round 1's ship deck in Streets of Rage 3** (issue #8): fixed in 2.4.
