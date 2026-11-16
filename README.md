# DoDonPachi Recomp

A native PC port of **DoDonPachi** (Cave, 1997), the arcade shooter, made by translating the arcade board's
68000 program into C ahead of time and running it on [Cave68kSDK](https://github.com/CapsLock4284/Cave68kSDK),
a model of Cave's 68000 board. It is the second game of the family, after
[DonPachi Recomp](https://github.com/CapsLock4284/DonPachi-Recomp), and uses the same tools.

**Status: early, playable.** The whole program translates to C, builds on Cave68kSDK and plays in a window with
its sound, and it runs exactly as the original does on MAME's 68000 core, slowdown and all (checked frame by frame
over 150,000 VBLANKs). The window port is DonPachi Recomp's, cut down for now: no options menu yet (the settings
are in `ddonpach.ini`), and none of DonPachi Recomp's extras (training, replays, high scores, achievements, side
panels).

You need your own ROM set: MAME's `ddonpach` (International, Master Ver. 97/02/05) or its Japanese clone
`ddonpachj`. No ROMs, and nothing extracted from them, are part of this repository.

## How it works

DoDonPachi was built on DonPachi's engine, and much of its code is DonPachi's, moved around and with other
addresses in it. The translator (`tools/core`, the shared arcade-decomp core) turns the program into C, one C
function for each of the original's routines. The game-specific settings are small:

| File | What it says |
|---|---|
| `tools/ddonpach_rom.py` | the ROM set: its files by CRC32 and SHA1, and how the program's two chips are interleaved |
| `tools/game_profile.py` | the board's memory map as the program sees it, and where the main loop waits for each frame |
| `tools/call_tables.py` | how the engine's code is written (jump tables of stubs, shared tails, the object types' routines), carried over from DonPachi Recomp with DoDonPachi's addresses |
| `tools/match_code.py` | finds where a DonPachi routine went in DoDonPachi's program, by its instructions |
| `symbols.txt` | the names of the program's routines and variables |

## Building

Clone with the submodules:

```sh
git clone --recursive https://github.com/CapsLock4284/DoDonPachi-Recomp.git
```

Put the ROM set's .zip in `rom/`, then translate the program (Python 3.10 or newer):

```sh
python tools/decompile.py --check     # what does not translate yet
python tools/decompile.py             # the C, into src/
```

The C is made from your ROMs, so it is never committed. (A run takes a while: about an hour.)

Then build the game in a window (CMake, a C compiler, and SDL2: on Windows its development package,
`SDL2-devel-2.x.x-VC.zip`, unpacked in `build/downloads/sdl2`; on Linux the system's, e.g. `libsdl2-dev`):

```sh
cmake -S . -B build/msvc                              # Windows (Visual Studio)
cmake --build build/msvc --config Release             # build/bin/ddonpach.exe
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release  # Linux
cmake --build build/linux -j                          # build/bin-linux/ddonpach
```

On its first run the game reads the ROM set from `rom/` (or asks for it to be dropped on its window) and keeps its
contents in `ddonpach.dat` beside the program; it reads that from then on.

## Playing

| | Keys (player 1) | Gamepad |
|---|---|---|
| Move | arrows | d-pad, left stick |
| Shot (held: the laser) | Z | X |
| Bomb | X | A |
| Start, coin | 1, 5 | Start |
| Service switch (held 2 s at the title: the test mode) | F2 | |

F3 resets the game, F4 holds the service switch for you (the test mode, from the title or the attract mode), Tab
(LB) fast-forwards, F5 / F8 save and load a state (F6 / F7 choose the slot, of 5), F11 or Alt+Enter switches full
screen, Escape twice quits. Player 2: R F D G, A, S, 2, 6. Everything can be changed in `ddonpach.ini`, written
beside the program on its first run: the keys and buttons, the window's size, the video (scaling, scanlines, the
CRT look, the pixel-art scalers, a monitor on its side), the sound's volume, the arcade's slowdown.

## Running it headless

For the checks (Linux or MSYS2; the board from `runtime/`, the C from `src/`):

```sh
sh tests/build.sh                     # tests/bin/boot, tests/bin/diag
cd rom && ../tests/bin/boot 3000 ddonpach.zip    # pictures every 100 frames (boot_NNNNN.ppm)
```

## Checking it against the original

`tools/cref/cref_timed` runs the original 68000 program on MAME's 68000 core (from `tools/core`) with the same
board model, at the board's own timing, and `tests/slowdown_check` runs the port; both play the same inputs
(`tests/player.h`) and write a line for each frame of the game: the frame, the VBLANK it was done by, and a hash of
the whole board. Any difference in what the game did, or in which frames slowed down, shows.

The sound chip is part of that timing: DoDonPachi's interrupt handler serves both the VBLANK and the YMZ280B, and
tells them apart by how long ago the VBLANK began, so when a voice ends matters to the game. The board model raises
the chip's interrupt at the sample where the voice reads its stop address, as the chip itself does, in both tools
alike. (MAME's own driver signals it at its next sound update instead, a little later.)

```sh
sh tools/cref/build.sh && sh tests/build.sh
cd rom
../tests/bin/cref_timed --rom ddonpach.zip --frames 72000 --play --waits mame_waits.txt
../tests/bin/slowdown_check 72000
python3 ../tests/slowdown_compare.py mame_waits.txt slowdown_waits.txt
```

When they differ, `TRACE_FRAME=N` on both (the port built from `tools/decompile.py --trace`) and
`tests/trace_compare.py` show where the port first goes another way.

## Licence

BSD-3-Clause: see `LICENSE`. The board code is Cave68kSDK's, the translator is arcade-decomp's; each says
what it follows (MAME's drivers and devices, BSD-3-Clause). The window port is DonPachi Recomp's (BSD-3-Clause); the
icon is the program's own (`tools/make_icon.py`).
