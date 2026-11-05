## What's new in 2.6.1: the third player, tested further

Streets of Rage 2's third player (still experimental, still off by default) went through the tests 2.6 had not had.

Fixed:

- **After loading a save state or rewinding, the third fighter's picture is right at once.** It used to stay wrong until its next move.
- **Save states and rewind keep the third pad's timing** (dashes, the charged special).

Now tested:

- **Three controllers:** three virtual controllers, through the same code as real ones: two players joined from the game's menu, the third with Start, and all three played. Physical controllers still have not been tried.
- **Enemies go for the third fighter:** in the same scene, the third fighter took most of the attacks with this on (health 104 down to 56, player 1 to 96); with it switched off they ignored it (104, and player 1 down to 32).
- **Back in after the last life:** Start brings the fighter back with full health and two more lives.
- **New stages:** the game clears the fighter's slot at each one, and it comes straight back. Played from stage 1 into stage 2.
- **Save state, rewind and replay checks** pass with it in the game.

Not tested yet: the later stages with a third fighter, the final boss and the ending. It still has no health bar or score on screen, and one fewer enemy can be on screen with the setting on.

If a third fighter stands still, enemies go for it and a stage takes longer.

Everything from 2.6 is in this release.
