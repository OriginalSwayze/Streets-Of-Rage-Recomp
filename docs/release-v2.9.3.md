## What's new in 2.9.3: Linux, natively or through Proton

New:

- **Play on Linux, natively (#28).** Download the new `Streets-of-Rage-Collection-Source-v2.9.3.zip` below, unzip it, and in its `src` folder run `bash tools/install.sh --sor1 <rom> --sor2 <rom> --sor3 <rom>` (any of the games; `--bk3`, `--bk3-en` and `--bk3-dx` add Bare Knuckle III and its editions as patched ROMs; `--help` lists the rest). It builds each game as a Linux program, installs the collection to `~/.local/share/streets-of-rage-collection` and adds it to your applications menu. If a build tool is missing it tells you which packages to install. Music files can be MP3, FLAC, WAV or Ogg Vorbis there. To update, run a newer release's `install.sh`.
- **Setup installs under Wine and Proton (#28).** It no longer needs Windows' PowerShell, tar or curl: it unpacks, downloads and checks the build tools by itself. On the Steam Deck, or if you'd rather not build natively, run the Setup `.exe` through Proton (as a non-Steam game) or Wine.
- **The menus' text without Windows' fonts.** Under Wine and on Linux, Impact, Segoe UI and Bahnschrift aren't there, so headings overlapped. Each program now carries open-licence look-alikes set to the same sizes and widths. On Windows nothing changes.

Tested on Windows 11, under Wine 9.0 and natively on Ubuntu 24.04: the games play frame for frame the same on all three. They haven't been tried yet on a real Linux desktop, a Steam Deck or Proton itself: if something goes wrong there, please say so on #28.

Not fixed yet:

- **Sprites flickering in 7A's robot factory when it's crowded (#21).**
- **A light black flicker with high frame rate (Streets of Rage 2),** **round 6's screen unlock glitch (#20, the other parts)** and **the background jitter at the start of a level (#17).**

Everything from 2.9.2 is in this release. If you have 2.9 or later, the collection offers this update when it opens.
