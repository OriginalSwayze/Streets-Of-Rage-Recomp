# Streets of Rage Recomp

A native PC port of **Streets of Rage (World, Sega Genesis / Bare Knuckle)** built from your
own ROM. No game data is included in this repository: every tool reads your ROM and checks
its SHA-1 (`3d74dbc81f3472a5bde45bf265e636a72a314667`).

It is a static recompilation, not a decompilation: at build time the game's 68000 machine
code is translated into C from your ROM, and anything not yet covered runs on an
interpreter. To play it, see [Playing it](#playing-it).

The engine comes from [Beyond Oasis Recomp](https://github.com/OriginalSwayze/beyond-oasis-recomp):
the recompiler, the Genesis hardware, the renderer, the pause menu, save states and rewind
work for any cartridge. The features that read a game's own RAM or call its own code are
switched on per game in [`runtime/src/game.h`](runtime/src/game.h); the Streets of Rage
ones are mapped in [`docs/ram.md`](docs/ram.md).

## Playing it

**You need your own Streets of Rage (World) ROM.** It is not included and never will be,
and there is no ready-made .exe to download: the built game contains the game's own code,
translated to C, so it can only be made on your PC from your copy. The builder does that
for you:

1. Download `Streets-of-Rage-Recomp-<version>.zip` from [Releases](../../releases) (not
   "Source code": that leaves out the third-party code the build needs).
2. Unzip it somewhere with a short path, for example your Desktop.
3. Double-click **Build Streets of Rage.bat**. If Windows asks whether to run it, choose
   Run (or More info, then Run anyway).
4. Pick your ROM when asked. It has to be the World release (Bare Knuckle and the USA/EU
   cartridges are the same ROM), unchanged: the builder checks its SHA-1 and tells you if
   it is a different one.

The builder then:

* downloads the build tools it needs into a `toolchain` folder next to it, after asking:
  [Node.js](https://nodejs.org/), [CMake](https://cmake.org/),
  [Ninja](https://ninja-build.org/) and [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw)
  (Clang), a 260 MB download (about 1 GB unpacked), each from its publisher's own release
  and checked against a pinned SHA-256. Nothing is installed into Windows; delete the
  folder to remove them.
  Tools you already have are used instead.
* recompiles your ROM to C and builds the game (a few minutes the first time)
* puts a **Streets of Rage** shortcut on your desktop

The game is `build/cmake/StreetsOfRage.exe`, with its saves, states and `settings.ini` next
to it. It opens on a launcher with the settings; the first start plays the game's intro
once in the background for the launcher's art and the fighters' previews. To update, unzip
the new release over the old folder (your saves are kept) and run the builder again. It
needs 64-bit Windows 10 or 11.

To build it by hand instead, see [Usage](#usage).

## Status

| Part | State |
|---|---|
| ROM verification (`tools/setup.mjs`) | done |
| Code tracer and disassembly (`tools/disasm.mjs`) | done, with the code seen in play (`config/coverage.txt`) |
| 68000 → C recompiler (`tools/recomp.mjs`, built automatically by CMake) | done: 99.99% of 68000 time runs as native C |
| Hardware: memory map, VDP, pads, Z80 + YM2612 + PSG sound | done |
| Launcher in the game's style, with its music | done |
| Pause menu: save/load slots, display, effects, colours, cheats, controls | done |
| Save states, rewind, fast-forward, controller hot-plugging | done |
| AI partner for player 2 (or a second controller) | done |
| Widescreen (16:9), with the play area and enemy spawns widened to match | done |
| Internal resolution with a sharp, dither-aware scaler; display filters | done |
| Neon glow and HDR look | done |
| Fighters' colours: any of 512 colours per part, animated, with a live preview | done |
| Cheats (infinite health, lives, specials and time; attack power up to one-hit kills), combat info, play timer | done |
| One-click builder and release zip | done |

## Testing

The native build must behave exactly like the interpreter:

```
StreetsOfRage.exe --headless 3400 --hash-log a.log <rom>
StreetsOfRage.exe --headless 3400 --hash-log b.log --interpret <rom>
                                                      # a.log and b.log must be identical
```

Add `--press` with a script (`frame:buttons:frames,...`, e.g. `1300:S:4,2500:R:300`) to play
past the title. Verified: intro, title, character select and the first street's fight,
3,400 frames identical; save states and rewind replay identically.

New code is found by playing: the runtime records every instruction it runs in
`coverage.bin`, and the next `node tools/disasm.mjs` / build recompiles it.

## Usage

The builder above does all of this. By hand, you need:

* Windows 10 or 11 (the menus draw their text with Windows' fonts)
* [Git](https://git-scm.com/), [Node.js](https://nodejs.org/) 18 or newer,
  [CMake](https://cmake.org/) 3.20 or newer, [Ninja](https://ninja-build.org/) and
  [Clang](https://releases.llvm.org/)
* Your own Streets of Rage (World) cartridge dump (SHA-1 above). It is not included and
  never will be.

Clone with the submodules (the third-party code below), or the build cannot find them:

```
git clone --recursive <this repository's URL>
```

(Already cloned without them? Run `git submodule update --init --recursive`.)

Then, in the repository's folder:

```
node tools/setup.mjs "<path to Streets of Rage (World) ROM>"
node tools/disasm.mjs
cmake -S . -B build/cmake -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/cmake
build/cmake/StreetsOfRage.exe "<path to ROM>"
```

`setup.mjs` checks the ROM and remembers where it is (`local.json`, never committed); the
build recompiles it into `generated/` (also never committed). The ROM path is also kept in
`settings.ini` next to the executable, so later starts need no argument.

Default controls (all remappable in the pause menu's Controls page):

| Action | Keyboard | Controller (Xbox layout) |
|---|---|---|
| Move | Arrows | D-pad (the left stick always works too) |
| A / B / C (special / attack / jump) | Z / X / C | X / A / B |
| Start | Enter | Start |
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

The code in this repository (the runtime and tools) is under the MIT License: see
[LICENSE](LICENSE). The third-party code above keeps its own licenses.

This is an unofficial fan project, not affiliated with or endorsed by Sega. Streets of
Rage, Bare Knuckle and Sega are trademarks of Sega Corporation. No part of the game (code,
graphics, music or text) is included: you need your own copy, and everything made from it
stays on your computer.
