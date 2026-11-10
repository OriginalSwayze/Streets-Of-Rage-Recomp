# Streets of Rage Collection

Native PC ports of **Streets of Rage**, **Streets of Rage 2** and **Streets of Rage 3**
(Sega Genesis / Mega Drive) in one collection, each built from your own ROM. No game data is
included in this repository: every tool reads your ROM and checks its SHA-1.

| Game | ROM it needs | SHA-1 |
|---|---|---|
| Streets of Rage | Streets of Rage (World) (also sold as Bare Knuckle) | `3d74dbc81f3472a5bde45bf265e636a72a314667` |
| Streets of Rage 2 | Streets of Rage 2 (USA) | `8b656eec9692d88bbbb84787142aa732b44ce0be` |
| Streets of Rage 3 | Streets of Rage 3 (USA) | `40a33dd6f9dab0aff26c7525c9b8f342482c7af6` |
| Streets of Rage 3: Bare Knuckle III edition | Bare Knuckle III (Japan) | `b9b2b4a98a9d8f4c49aa1e5395e2279339517fdb` |
| Streets of Rage 3: Bare Knuckle III, in English | Bare Knuckle III (Japan) with Twilight Translations' English patch 1.00 (`bk3_100.ips`) applied | `d1dc85abe0889ff8e880edcaf908a6b195b75952` |
| Streets of Rage 3: Bare Knuckle III DX | Bare Knuckle III (Japan) with the Bare Knuckle III DX 1.5 patch (`BKIII 1.5.ips`) applied: a 4 MB ROM | `e8e19ece9ad6966e9e635516d8bb851a2b74b36a` |

**Bare Knuckle III** is the Japanese original of Streets of Rage 3, with its story, its own
difficulty and the fighters' original colours. It plays as an edition of Streets of Rage 3:
give Setup its ROM as well (its own row under Streets of Rage 3), and pick it under Edition in
the launcher's Game tab or the pause menu's More tab. Each edition has its own saves, save
states and launcher art. For it in English, give Setup Twilight Translations' patch too
(`bk3_100.zip` or `bk3_100.ips`, from its translators) on the row below: Setup applies it to
your Japanese ROM and keeps both editions. The Europe and Asia cartridges are not editions here (Europe's
runs at 50 Hz).

**Bare Knuckle III DX** is a fan hack of Bare Knuckle III, the Director's Cut 1.5. It puts back areas cut
from the cartridge (round 6's extra rooms, the motorbike stages), has Red Comet's English translation, and
gives Shiva new moves. For it, give Setup the hack's patch (`BKIII 1.5.ips`, or a zip with it) on the last
row: Setup applies it to your Japanese ROM, and it is another edition. It plays as the hack was left, with its
own unfinished parts (bosses picking up weapons, a few sprite mix-ups, round 6 ending once the general is
saved).

They are static recompilations, not decompilations: at build time each game's 68000 machine
code is translated into C from your ROM, and anything not yet covered runs on an
interpreter. To play, see [Playing it](#playing-it).

The engine comes from [Beyond Oasis Recomp](https://github.com/OriginalSwayze/beyond-oasis-recomp):
the recompiler, the Genesis hardware, the renderer, the pause menu, save states and rewind
work for any cartridge. Each game's own addresses (its objects, health, camera, sound driver)
are mapped in [docs/ram.md](docs/ram.md), [docs/ram-sor2.md](docs/ram-sor2.md) and
[docs/ram-sor3.md](docs/ram-sor3.md).

## Playing it

**You need your own ROMs.** They are not included and never will be, and there are no
ready-made game programs to download: a built game contains the game's own code, translated
to C, so it can only be made on your PC from your copy. Setup does that for you:

1. Download `Streets-of-Rage-Collection-Setup-<version>.exe` from [Releases](../../releases)
   and run it. If Windows warns about an unknown program, choose More info, then Run anyway.
2. Pick the install folder (your user's Programs folder by default: no administrator
   rights needed).
3. Pick your ROMs: `.md`, `.bin`, `.gen` or a `.zip` with one inside. Each is checked as you
   pick it (a ROM picked for the wrong game moves to the right one). Skip any game you do not
   have; run Setup again later to add it. Streets of Rage 3 has three optional rows under it:
   Bare Knuckle III (Japan), its English patch and the DX patch. Bare Knuckle III is built on the USA ROM,
   so pick that one too.
4. Setup downloads the build tools it needs into the install folder, unless they are already
   on your PC: [Node.js](https://nodejs.org/), [CMake](https://cmake.org/),
   [Ninja](https://ninja-build.org/) and [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw)
   (Clang), about 260 MB, each from its publisher's own release and checked against a pinned
   SHA-256. It then builds each game from its ROM (a few minutes each) and adds the
   collection to the Start menu (and the desktop, if you like).

**Streets of Rage Collection** opens on the three games. Each has its own launcher, themed
after the game, with its settings, its soundtrack and its fighters' colours; its settings,
saves and save states live in the install folder's `games\<game>` folder. To add a game,
rebuild, or remove the collection: Settings > Apps > Streets of Rage Collection (Modify runs
Setup again; Uninstall can keep your saves).

## What it adds

In all three games:

* **Widescreen** (16:9 and other shapes), with the play area widened to match: the players walk
  to the new edges and enemies walk in from beyond them. At either end of a street the view
  stops at the street's end and takes its width from the other side, so the picture is the
  game's own scenery edge to edge; scenes, rooms and talks drawn for 4:3 keep to it, with blurred
  sides (or black)
* **High frame rate**: drawn at your screen's refresh rate (120, 144 Hz or more), the scrolling
  and every sprite gliding between the game's 60 updates (a fighter's sprites together, in
  one piece)
* **Internal resolution** up to 6x with dither-aware scalers (Edge, Smooth/xBR, Sharp),
  sharpening, integer scaling and CRT, scanline and LCD filters
* **Neon glow** (the scenery's signs and lamps light up) and an **HDR look**
* **AI partner** for player 2, or a second controller (in Streets of Rage 2, pick 2 PLAYERS)
* **Cheats**: infinite health and lives, attack power up to one-hit kills; the timer and the
  specials where the game has them; the **hidden options** of each game's own Options screen
  (round select and lives; in Streets of Rage 2 also Very Easy and Mania) without the codes
  and, in Streets of Rage 3, its **hidden fighters**: Roo on the select screen, and Shiva and
  Ash in the fighter choice a player gets on joining or continuing (in DX all three are there)
* **Combat info** (damage numbers, health bars over ordinary enemies), rumble, a play timer and stats
* **Fighters' colours**: every playable fighter's clothes, hair and skin in any of the
  Genesis' 512 colours, still or animated, with a live preview
* Save states, rewind, fast-forward, a slowdown fix and a sprite flicker fix
* Sound at the console's own balance, as emulated or cleaned up (no aliasing, hum or
  clipping), with a bass setting
* **Your own music**: put recordings in a `music` folder beside the game, named by the
  launcher's Track number (`07.flac`, or `07 - Go Straight.mp3`), and the game plays them
  instead of its songs, looping where the songs loop, with its sound effects on top. MP3,
  FLAC, WAV, M4A/AAC or WMA (Windows decodes them); the Music files setting turns them off
* A launcher and pause menu in a neon night-street style, in each game's own pair of neon
  colours, with its own music and sounds

In Streets of Rage 2:

* A **third player** (experimental, off by default): with three controllers, Start on the third
  joins a round as a third fighter

In Streets of Rage 3:

* **Bare Knuckle III**, the Japanese original with its story, as an edition (also in English
  with Twilight Translations' patch, and as the DX hack, which Setup applies): see
  [above](#streets-of-rage-collection)
* A **six-button pad**, as the game reads on a Mega Drive: Z for the back attack, X for the
  full-strength blitz moves

## Testing

The native build of each game must behave exactly like the interpreter:

```
StreetsOfRage2.exe --headless 4000 --press <script> --hash-log a.log
StreetsOfRage2.exe --headless 4000 --press <script> --hash-log b.log --interpret
                                                      # a.log and b.log must be identical
```

`--press` takes a script (`frame:buttons:frames,...`, e.g. `300:S,500:S,1700:R:400`) to play
past the title. Widescreen and the cheats change the game on purpose (their patches only run
natively); with them off every game matches the interpreter frame for frame. To get further,
`--autoplay N` has player 1 play itself from frame N (the AI partner's fighting, walking on
when the street is clear); with `--cheat health` it gets through the first rounds. `--poke-at F CODES` writes RAM
codes (`FFxxxx:VV`) once, at frame F.

New code is found by playing: the runtime records every instruction it runs in
`coverage.bin`, and `node tools/save-coverage.mjs --game <name>` adds it to the game's
`coverage.txt`, so every build recompiles it.

## Building by hand

Setup does all of this. By hand, you need:

* Windows 10 or 11 (the menus draw their text with Windows' fonts)
* [Git](https://git-scm.com/), [Node.js](https://nodejs.org/) 18 or newer,
  [CMake](https://cmake.org/) 3.20 or newer, [Ninja](https://ninja-build.org/) and
  Clang ([LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw))
* Your own ROMs (above)

Clone with the submodules (the third-party code below):

```
git clone --recursive <this repository's URL>
```

Then, for each game (`sor1`, `sor2`, `sor3`), in the repository's folder:

```
node tools/setup.mjs --game sor2 "<path to its ROM>"
GAME=sor2 node tools/disasm.mjs
cmake -S . -B build/sor2 -G Ninja -DGAME=sor2 -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/sor2
```

Each game's folder in [games/](games) holds its ROM's details and features (`game.json`), the
patches at its own addresses (`patches.json`), extra code entry points (`entries.json`) and
the code seen running in play (`coverage.txt`). A game's other editions (`editions` in
`game.json`, e.g. Bare Knuckle III) are recompiled into the same game: run
`node tools/setup.mjs` for each one's ROM too. Their code moved, so the addresses above are
found in each by the bytes around them: `node tools/map-editions.mjs --game sor3` writes
`editions.json` (addresses only) after an address is added or changed. `build/sor1` also builds the collection's
front end (`Streets of Rage Collection.exe`) and the installer (`Setup.exe`);
[tools/install.ps1](tools/install.ps1) is what Setup runs.

Default controls (all remappable in the pause menu's Controls page):

| Action | Keyboard | Controller (Xbox layout) |
|---|---|---|
| Move | Arrows | D-pad (the left stick always works too) |
| A / B / C | Z / X / C | X / A / B |
| Start | Enter | Start |
| X / Y / Z (Streets of Rage 3) | A / S / D | Y / left stick / right stick |
| Rewind (hold) | Backspace | LB |
| Fast-forward (hold) | Tab | RB |
| Pause menu | Esc | Back (Guide and Esc always work too) |

Fixed keys: F2 save state, F4 load, F6/F7 previous/next slot (1-9, files in `states/`),
F11 fullscreen.

## Third-party code

* [Musashi](https://github.com/kstenerud/Musashi) (MIT): 68000 interpreter fallback
* [SDL2](https://github.com/libsdl-org/SDL) (zlib): window, input, audio
* [z80](https://github.com/superzazu/z80) (MIT): Z80 sound CPU
* [ymfm](https://github.com/aaronsgiles/ymfm) (BSD-3-Clause): YM2612 FM synthesis

## License and legal

The code in this repository (the runtime, tools and installer) is under the MIT License:
see [LICENSE](LICENSE). The third-party code above keeps its own licenses.

This is an unofficial fan project, not affiliated with or endorsed by Sega. Streets of Rage,
Bare Knuckle and Sega are trademarks of Sega Corporation. No part of the games (code,
graphics, music or text) is included: you need your own copies, and everything made from
them stays on your computer.
