## What's new in 2.4: your own music

**Play the games with your own recordings of their songs.** Make a folder named `music` in the game's folder (in the install folder: `games\sor1`, `games\sor2` or `games\sor3`) and put the songs in it, each named by its number in the launcher's soundtrack player (the **Track** row in the **Music** tab): `07.flac`, or `07 - Go Straight.mp3`. Whenever the game plays that song, your file plays instead; its sound effects and voices play on top as before.

- MP3, FLAC, WAV, M4A/AAC and WMA, decoded by Windows itself.
- Songs that loop in the game loop; the jingles that end (a round cleared, a game over) end.
- Save states remember which song plays and where.
- **Music files** (launcher **Music** tab, pause menu **More** tab) shows how many songs it found and turns them off.

Also fixed:

- **The collection no longer starts a game again with the press that left it.** Back from a game, it waits until every key and button is let go before taking input.
- **Streets of Rage 3, widescreen, round 1's ship deck:** the purple blocks at the sides (the boat's graphics, kept beside the background) are gone. The background's still strips are pictures drawn for 4:3, and their sides now continue them. (GitHub issue #8)
