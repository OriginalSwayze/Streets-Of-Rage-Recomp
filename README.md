# Streets of Rage Recomp

A native PC port of **Streets of Rage (World, Sega Genesis / Bare Knuckle)** built from your
own ROM. No game data is included in this repository: every tool reads your ROM and checks
its SHA-1 (`3d74dbc81f3472a5bde45bf265e636a72a314667`).

It is a static recompilation, not a decompilation: at build time the game's 68000 machine
code is translated into C from your ROM, and anything not yet covered runs on an
interpreter. To build it, see [Usage](#usage).

The engine comes from [Beyond Oasis Recomp](https://github.com/OriginalSwayze/beyond-oasis-recomp):
the recompiler, the Genesis hardware, the renderer, the pause menu, save states and rewind
work for any cartridge. The features that read a game's own RAM or call its own code are
switched on per game in [`runtime/src/game.h`](runtime/src/game.h); for Streets of Rage they
are off until its addresses are found.

## Status

| Part | State |
|---|---|
| ROM verification (`tools/setup.mjs`) | done |
| Code tracer and disassembly (`tools/disasm.mjs`) | done: ~42 KB of code in 4,510 blocks, with the runtime's coverage |
| 68000 → C recompiler (`tools/recomp.mjs`, built automatically by CMake) | done: 99.99% of 68000 time runs as native C |
| Hardware: memory map, VDP, pads, Z80 + YM2612 + PSG sound | done |
| Save states and rewind | done |
| Pause menu: save/load slots, display options, controls | done |
| Internal resolution, sharpening, display filters, lighting, slowdown fix | done |
| Controller hot-plugging, fast-forward | done |
| Two-player co-op | to do |
| Cheats (infinite lives, police specials, one-hit kills), combat info | to do: needs the game's RAM map |
| Smooth motion | to do: needs the game's sprite routines |
| Widescreen | to do: needs the game's camera and enemy spawn edges |
| Launcher | off: Beyond Oasis's runs its sound driver; a Streets of Rage one is to do |

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

You need:

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
