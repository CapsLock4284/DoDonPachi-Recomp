# DoDonPachi Recomp

A native PC port of **DoDonPachi** (Cave, 1997), the arcade shooter, made by translating the arcade board's
68000 program into C ahead of time and running it on [Cave68kSDK](https://github.com/CapsLock4284/Cave68kSDK),
a model of Cave's 68000 board. It is the second game of the family, after
[DonPachi Recomp](https://github.com/CapsLock4284/DonPachi-Recomp), and uses the same tools.

**Status: early.** The whole program translates to C, builds on Cave68kSDK and runs headless: its warning, title
and attract-mode demo are drawn right, and it runs exactly as the original does on MAME's 68000 core, slowdown and
all (checked frame by frame over 150,000 VBLANKs). There is no sound yet and no window to play in yet.

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

To run it headless (Linux or MSYS2; the board from `runtime/`, the C from `src/`):

```sh
sh tests/build.sh                     # tests/bin/boot, tests/bin/diag
cd rom && ../tests/bin/boot 3000 ddonpach.zip    # pictures every 100 frames (boot_NNNNN.ppm)
```

## Checking it against the original

`tools/cref/cref_timed` runs the original 68000 program on MAME's 68000 core (from `tools/core`) with the same
board model, at the board's own timing, and `tests/slowdown_check` runs the port; both play the same inputs
(`tests/player.h`) and write a line for each frame of the game: the frame, the VBLANK it was done by, and a hash of
the whole board. Any difference in what the game did, or in which frames slowed down, shows.

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
what it follows (MAME's drivers and devices, BSD-3-Clause).
