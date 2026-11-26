# DoDonPachi Recomp

A native PC port of **DoDonPachi** (Cave, 1997), the arcade shooter, made by translating the arcade board's
68000 program into C ahead of time and running it on [Cave68kSDK](https://github.com/CapsLock4284/Cave68kSDK),
a model of Cave's 68000 board. It is the second game of the family, after
[DonPachi Recomp](https://github.com/CapsLock4284/DonPachi-Recomp), and uses the same tools.

**Status: early, playable.** The whole program translates to C, builds on Cave68kSDK and plays in a window with
its sound, and it runs exactly as the original does on MAME's 68000 core, slowdown and all (checked frame by frame
over 150,000 VBLANKs). The window port is DonPachi Recomp's, cut down for now: its options menu (Escape, or the
gamepad's View button), a main menu with training, replays, achievements and statistics, high scores kept, and its
own side panels.

You need your own ROM set: MAME's `ddonpach` (International, Master Ver. 97/02/05) or its Japanese clone
`ddonpachj`. No ROMs, and nothing extracted from them, are part of this repository.

## Download and play

1. Get the latest release from [**Releases**](https://github.com/CapsLock4284/DoDonPachi-Recomp/releases/latest):
   `DoDonPachi-x.y.z-win64.zip` for Windows, `DoDonPachi-x.y.z-linux-x64.tar.gz` for Linux.
2. **Windows:** unzip it somewhere you can write to (not inside *Program Files*) and run `ddonpach.exe`.
   Windows may show *"Windows protected your PC"* because the program isn't signed: click **More info → Run anyway**.<br>
   **Linux:** unpack it (`tar xzf DoDonPachi-x.y.z-linux-x64.tar.gz`) and run `./ddonpach`. It uses the system's SDL2
   (`sudo apt install libsdl2-2.0-0` on Debian/Ubuntu, `libsdl2-compat` on Ubuntu 25.10 and later, `dnf install SDL2`
   on Fedora, `pacman -S sdl2` on Arch, if it isn't there already).
3. On the first run a window asks for the ROM set: drag `ddonpach.zip` onto it. The game extracts what it needs once
   (`ddonpach.dat`, beside the program); after that the .zip isn't needed. From then on the game offers each new
   version itself (UPDATE TO V... on the main menu).

**The Japanese version:** VERSION on the main menu switches to MAME's `ddonpachj` (Japan, Master Ver. 97/02/05). It
has its own program chips only (`u27.bin`, `u26.bin`); a merged `ddonpach.zip` has them inside (in `ddonpachj/`), so
its data is taken on the first run with the World one; with separate sets, put `ddonpachj.zip` beside
`ddonpach.zip`, or drop it on the first run's window after `ddonpach.zip` (or later, when JAPAN is first chosen). It
is extracted to `ddonpach_jpn.dat`. Each version keeps its own high scores, stage bests and arcade settings;
achievements and statistics are shared.

The release's `README.txt` has the controls, hotkeys and the list of files the game keeps. What's new in each
version: [CHANGELOG.md](CHANGELOG.md). Making a release: `tools/make_release.ps1` (Windows) and
`tools/make_release.sh` (Linux, or WSL), after a Release build; the changelog's section for the version first.

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

The Japanese version's program, the same way (its names carried over from the International program's, where the code
is the same: `tools/version_map.py`); the build puts it in the game beside the other when `src_jpn/` is there:

```sh
python tools/decompile.py --set japan # the Japanese program's C, into src_jpn/
```

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
| Shot (held: the laser) | Z | A |
| Bomb | X | B |
| Auto-fire (the game's own: "C BUTTON FULL-AUTO", set by the port) | C | X |
| Start, coin | 1, 5 | Start |
| Service switch (held 2 s at the title: the test mode) | F2 | |

Any other gamepad or arcade stick works too: those SDL knows by their layout (more of them from
`gamecontrollerdb.txt`, shipped beside the program), and any other read as it is: its d-pad and stick move, buttons
1-3 are shot, bomb and auto-fire, its last two coin and start, and its buttons, hats and axes can be bound on the
CONTROLS page (BTN 3, HAT UP, AXIS 2+). `ddonpach.log` names each device found and how it is read.

Training (TRAINING on the main menu): any stage of either loop, from its start or at its boss (from its WARNING,
as the boss comes), and 2-7, the second loop's hidden Area 7: its boss Hachi, or the true last boss Hibachi (Hachi
shot down for you on the way). The ship, its style (shot or laser), the power (ON PACE, the default: what a player
has there from the power-ups on the way, shown beside it, "ON PACE (2)"; or 1 to MAX) and the bombs (3, or the
most, 6) are chosen. The game is run there unseen
from its reset (a few seconds: LOADING...), and kept as a save state: RETRY TRAINING in the options goes back to
it, and with RETRY ON MISS a miss goes back to it a second later. TRAINING MENU goes back to the training page.
SPEED plays the game slower, 50% to 100% in steps of 10, to learn a place and work up to the arcade's: the game is
the same frame for frame (its slowdown and patterns as they are), its frames only come further apart, and the sound
is slowed with it (lower, as a tape played slow); the speed shows at the foot of the screen.
Training games don't count for high scores or the stages' bests. Its choices are kept in `[training]` in
`ddonpach.ini`.

Achievements (ACHIEVEMENTS on the main menu): 28 goals, from clearing 1-1 to a one-credit clear of both loops,
a stage without dying or without bombs, all 13 bees, hit combos, scores, full power, an extra life, every ship,
the hidden 1UP, 2-7 and Hibachi.
Each earned is shown over the right side panel ("ACHIEVEMENT UNLOCKED") and kept in `ddonpach.achievements`.
Statistics (STATISTICS on the main menu): games, continues, play time, stages and loops cleared, deaths, bombs,
bees, the best score and hit combo, the longest life, the furthest stage, each stage's clears, deaths and best
bees, each ship's games and best score; kept in `ddonpach.stats`. Both count arcade games only: not training,
replays, a game with a cheat or a save state, or one with the slowdown off.

Overlays (HITBOX, AURA and BULLETS in the game options, `hitbox`, `aura` and `bullets` in `ddonpach.ini`; off by
default): the ship's hitbox as a white glowing dot (the box the game tests the enemies' bullets against: much
smaller than the ship), and, while the laser is fired, its aura as a light blue box (where the laser hits enemies
close up, around the ship). BULLETS marks each enemy bullet's point, a white pixel in a small green cross: the game
tests a bullet only by that point (the middle of its sprite), so a bullet hits only when its point gets inside the
ship's hitbox, however big it looks. Read from the game's own records each frame and drawn over the picture only:
the game is not changed.

Replays: every game is recorded as it is played. At its game over the game asks whether to keep its replay (saved
in `replays/` beside the program, a file of about 14 KB a minute that can be shared); REPLAYS on the main menu lists
them (the date, how far, the score) to watch or delete. Playback runs from the board's reset as the game did (the
frames before the game at once, unseen), checking the board as it goes; fast forward works, STOP REPLAY in the
options ends it. A game with a cheat, a loaded save state or training in it is not recorded.

The main menu comes up as the program starts: START GAME, TRAINING, REPLAYS, SETTINGS (the sound, the game options, graphics and
controls, before a game) and QUIT TO DESKTOP, with the high score kept. The game waits at its start while it is up,
and runs from there with START GAME, as the board switched on. MAIN MENU in the options menu leaves the game for it
(the game reset). MAIN MENU AT START in the game options (`main_menu` in `ddonpach.ini`) turns it off: straight into
the game.

Updates: as the program starts it looks up the newest release on GitHub (in the background; the game does not
wait). If there is a newer one, the main menu shows UPDATE TO V... at its top; choosing it asks first, then
downloads the release's package (checked against the SHA-256 GitHub gives for it), puts its files in place of the
program's own (the replaced ones kept as `.old` until the next start) and starts the program again. Your settings,
high scores, replays, achievements, statistics, EEPROM and the game's data are never touched. CHECK FOR UPDATES in
the game options (`check_updates` in `ddonpach.ini`) turns the look-up off. The download is WinHTTP on Windows and
the `curl` command on Linux.

F3 resets the game, F4 holds the service switch for you (the test mode, from the title or the attract mode), Tab
(LB) fast-forwards, F5 / F8 save and load a state (F6 / F7 choose the slot, of 5), F9 shows the FPS meter, F11 or Alt+Enter
switches full screen, Escape (View on the gamepad) opens the options menu: video, sound, controls (remapping), the game's options,
save states, cheats, QUIT TO DESKTOP (with the side panels on, the right one explains the chosen entry).
Player 2: R F D G, A, S, 2, 6. Everything can be changed there or in `ddonpach.ini`,
written beside the program on its first run: the keys and buttons, the window's size, the video (scaling, scanlines, the
CRT look, the pixel-art scalers, on the whole picture or by layer, a monitor on its side), the sound's volumes,
the arcade's slowdown.

Graphics settings (SETTINGS > GRAPHICS, and `[video]` in `ddonpach.ini`). None of them change the game itself, only
how its picture is shown:

| Setting | Choices | What it does |
|---|---|---|
| SCALING | FIT, INTEGER, STRETCH | FIT: as large as the window allows, in the arcade's shape. INTEGER: whole multiples of the arcade's pixels only, so every pixel is the same size (the sharpest; may leave a border). STRETCH: fills the whole window, out of shape |
| SMOOTHING | OFF, ON | Softens the pixels' edges as the picture is scaled up; off, the pixels stay sharp squares |
| FILTER | OFF, SCALE2X, SCALE3X, XBR 2X, XBR 3X, XBR 4X, MMPX | A pixel-art scaler: a larger copy of the picture whose jagged edges are smoothed while the art stays sharp. Scale2x and Scale3x are the lightest, MMPX keeps the art's own look (thin lines and single pixels kept), xBR is the smoothest. Not used with the CRT look |
| BY LAYER | OFF, ON | Runs FILTER on each of the game's layers and its sprites on their own, then puts them back together as the arcade board does: sprite outlines stay clean instead of smearing into a busy background, and lines that cross keep their shape. Scale2x, Scale3x and MMPX only: with xBR it shows NO (XBR) and xBR stays on the whole picture. Costs a few more milliseconds a frame |
| SCANLINES | OFF, LIGHT, MEDIUM, HEAVY | Dark lines like an arcade monitor's. They run up and down the picture, as the cabinet's monitor stands on its side; none when the picture is too small for them |
| CRT | OFF, LIGHT, FULL | The arcade monitor's look: a curved screen with its lines, phosphor stripes and glow (SCANLINES and FILTER are set aside while it is on). Best in full screen |
| VSYNC | OFF, ON | On: no tearing, the picture waits for the monitor (a little more delay). Off (the default): the least delay |
| RENDERER | AUTO, DIRECT3D, DIRECT3D11, DIRECT3D12, OPENGL, SOFTWARE | How the picture reaches the screen, used from the next start. AUTO is Direct3D 12 on Windows, the quickest to show |
| FPS METER | OFF, ON | The frame rate and the PC's time a frame (F9 too): on the right side panel, never over the game; over the picture's corner only when the panels are off or the picture is rotated |
| ROTATE | OFF, RIGHT, LEFT | Turns the picture for a monitor on its side (TATE); the side panels are hidden then |
| SIDE PANELS | ON, OFF | Panels in the empty space beside the picture on a wide screen: the game's information beside the picture (see Side panels below). They widen to fill the screen; the game's picture keeps its size |
| WINDOW SIZE | 1X to 8X | The window's size, as times the arcade's picture (3X by default) |
| FULL SCREEN | OFF, BORDERLESS, EXCLUSIVE | BORDERLESS: a window over the whole screen, quick to switch away from. EXCLUSIVE: the display is the game's alone, which may lag a little less. F11 or Alt+Enter switches it too |

Cheats (GAME OPTIONS > CHEATS): the port's own P1 INVINCIBLE and P2 INVINCIBLE (nothing can hit that player's ship:
the time without harm the game gives after a life, kept up; the ship looks as always), for seeing the whole game. With
MAME's cheat file for DoDonPachi (`cheats/ddonpach.xml` beside the program, from Pugsy's Cheats; not included) its
cheats are listed after them. A game with a cheat on counts for nothing: no high scores, stage bests, achievements,
statistics or replay.

The menus have sounds of the game's own (its ship select's cursor and choice, and a short one for going back), and
an achievement earned plays the extra life's; at the game's own levels, under VOLUME as the game is.

Sound (SETTINGS > SOUND, `[sound]` in `ddonpach.ini`): VOLUME for all of it, and MUSIC, EFFECTS and VOICES each on
its own under it, in steps of 5%. While one is changed an example plays at the level being set (stage 1's music,
shots and enemies shot down, a voice). The game's own table of its 66 sounds says which are music (the last 11, all
on the chip's voice 0); the voices (#44-#54) were chosen by ear; the rest are effects. Only what you hear changes:
the game, its timing and the sound chip's interrupt are the same at any levels (checked through both loops; at 100%
the sound is the same as before, sample for sample).

Difficulty (DIFFICULTY in the game options): EASY, NORMAL (the arcade's default), HARD or VERY HARD, the test mode's
own GAME DIFFICULTY setting, so there is no need to go into the test mode for it. It sets each stage's base rank (how
hard the enemies' patterns are): EASY keeps it at the lowest all game, NORMAL raises it stage by stage, HARD starts
higher, VERY HARD at the top from the start. The game reads it as it starts: it takes effect after an F3 reset or a
restart. It is kept in the EEPROM file (`ddonpach.eeprom`), as the arcade board keeps it; replays keep their own.

Free play (on by default; FREE PLAY in the game options, `free_play` in `ddonpach.ini`): the game has no free play
setting of its own, so Start drops in the coins a game needs first, then presses Start until the game takes it. The
game runs as on the board (coin sound, credit). On the warning screen the game takes no start: the credit stays for
the next Start.

High scores are kept in `ddonpach.hiscore` beside the program (the arcade board forgets its table when it is
switched off). A game that went back to a save state or had a cheat on doesn't count: its scores are dropped when it
ends. SAVE HIGH SCORES in the game options turns this off.

The 31-second WARNING screen the game shows as it starts is skipped: the port runs its frames at once, unseen (the
game itself runs every one of them, as on the board). SKIP WARNING in the game options shows it again.

The FPS meter (F9, or FPS METER in the graphics options) shows, each second: GAME, the game's own frames (fewer
while the game slows down, as the arcade board does: SLOWDOWN OFF in the game options to compare); BOARD, the
board's frames run (57.55 a second, the arcade's rate: fewer means the PC can't keep up); WINDOW, the pictures
shown; LATE, the frames the PC ran late; and WORK, a frame's milliseconds on average and at most. With the side
panels on it is drawn on the right one (over the score's graph), so it never covers the game; without them, or with
the picture rotated, over the picture's top corner.
While the meter is on, each second is also written to `ddonpach_fps.log` beside the program, with the settings,
the renderer, and the time each part of a frame takes (the filter and the uploads to the graphics card among them).

Side panels (SIDE PANELS in the graphics options, `side_panels` in `ddonpach.ini`; on by default) fill the screen
beside the picture. The game's picture is never changed by them: it stays the arcade's 240 x 320, at the same scale,
and the panels only take the black space at its sides (as wide as the screen leaves room for: wider on a 16:9 or
21:9 screen than on a 16:10 one). Not with the picture rotated.
- Left: the ship and its style; its shot and laser power, 1 to MAX (each power-up raises both by one: four reach
  MAX; a life lost takes the style's own weapon down one and the other back to 1); the bombs; and the board's
  speed: the game's frames a second over the last second (the board's 57.55, less its frames of slowdown), the
  slowdown now, the enemies' bullets on screen, and a graph of the last 5 seconds (a bar for the bullets, red
  where the game slowed down: it shows what slows it, bullets or a crowd of enemies), the slowdown this stage and
  this game; the stage's 13 bees, each greyed until it is taken (in its own
  colours then), with the count ("9/13"); and the controls, as DonPachi Recomp's: the stick (a 3 x 3, the way it
  points lit) and the S, B and C buttons, lit while held, as the board is given them each frame.
- Right: the stage and the score, and the high score table chased one place at a time: #5 first, then #4 once it
  is passed, up to #1 ("NEXT #3", the score, and how much more it takes); then how much more score the next extra
  life takes ("1UP IN": the game gives one at 6,000,000 and one at 20,000,000; NO MORE after them), also a dotted
  line on the graph when it is in sight. The graph shows the score through the
  game against the place being chased (the dashed line at the top); each place passed leaves a line in its colour
  where it was passed, with its number. Under it, LOOP 2: whether this game still gets the second loop, from the
  game's own rule (below), in the first loop: YES and how once it is sure, the misses left while those alone would
  do it, or what is still short; NO after a continue or with two players. In the second loop it is HIBACHI: AFTER
  2-6, then 2-7 NOW! Below: the loop's six stages one under the other, a record of the game so
  far: each stage finished with its score and how it went against the best ever for that stage (+ or -, gold for a
  new best), the stage being played as it goes (yellow), and those to come with their best to beat. The stages' best
  scores are kept in `ddonpach.stages` beside the program, from clean
  games only (no cheat, no save state, the slowdown on); a stage after a continue doesn't count either.

**The second loop** comes after 1-6 when the player played alone, never continued, and has any one of: 2 misses or
fewer (lives lost); a MAX HIT of 624 or more with type A, 768 with B or 816 with C; a score of 50,000,000 or more; 4
or more stages with all 13 bees (the 13th taken with no miss since the stage began). That is the game's own check,
read from its program and tested against it at each edge (`tests/loop2_check`); the 270, 300 and 330 hits some
guides give are not what the game asks. The second loop can't be continued, and nothing more is asked for Hibachi:
clear 2-6 and 2-7 follows.

If the game stops or crashes, it says so in a message box, and leaves two files beside the program: `ddonpach.log`
(what happened and where) and `ddonpach_crash_DATE_TIME.inp`, the inputs since the last reset. The game is the same
from the same inputs, so the run can be played again up to the crash (`tests/replay_crash`); send both. The log of
the run before is kept as `ddonpach_previous.log`.

Input latency (measured with `tests/latency_check`): the game reads the controls as each VBLANK starts and shows a
press two frames later, on the board as in MAME. INPUT TIMING in the game options chooses:
- ARCADE (`arcade_timing = true`): exactly the board's two frames.
- FAST (the default): one frame less. The port runs on through the next frame from a saved state, shows its
  picture and puts the state back, so the game itself runs exactly as it would without it (slowdown included).

The display adds its own delay after that. GRAPHICS > RENDERER chooses how the picture reaches the screen
(`[video] renderer`). Measured with PresentMon on the owner's PC (60 Hz monitor, vsync off, 20 seconds each,
about 1,100 frames), from the finished frame to the screen:

| Setup | How Windows shows it | Finished frame to screen (median) | 95th percentile |
|---|---|---|---|
| direct3d, borderless (SDL's own choice) | through the compositor | 26.7 ms | 34.1 ms |
| direct3d11, borderless or exclusive | waits for the monitor's refresh | 10.4 ms | 18.5 ms |
| direct3d, exclusive | straight to the screen | 1.6 ms | 1.8 ms |
| opengl, borderless | straight to the screen | 1.4 ms | 1.5 ms |
| **direct3d12, borderless** (the default on Windows: auto) | straight to the screen | **1.4 ms** | 2.4 ms |

- direct3d (Direct3D 9) in a window: Windows' compositor copies each picture before it is shown.
- direct3d11: past the compositor, but it waits for the monitor's refresh as if vsync were on; SDL's exclusive
  full screen changes nothing for it.
- direct3d12 and opengl: straight to the screen in borderless full screen, as quick as exclusive full screen
  without its slow switching.

With vsync off the picture can tear (a line where two frames meet); VSYNC ON stops that for a little delay.

## Playtest: the whole game, by itself

To see a new build go all the way through without playing it, start it with `--playtest`:

```powershell
build\bin\ddonpach.exe --playtest            # every ship and style, one after another
build\bin\ddonpach.exe --playtest B-laser    # or one: A-shot, A-laser, B-shot, B-laser, C-shot, C-laser
```

The game plays itself as fast as the PC can run it, showing the picture now and then, with player 1 kept from harm
and at MAX power: both loops, 2-7, Hibachi and the ending, for each ship. The window's title shows where it is, and
a message at the end says whether every run passed, with each stage's time in `ddonpach_playtest.txt` beside the
program. It uses the board's own settings and saves nothing (no high scores, achievements, statistics, replays or
settings). Esc stops it. A run takes this player about an hour and a quarter of the game's time; at about 40 times
the arcade's speed, all six take around 10 minutes. It fails if the ship is lost, the game ends early, the ending
comes without Hibachi, or a stage doesn't move on for 35 minutes of the game's time.

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

The Japanese program the same way, with `VERSION=1` before both tools (`cref_timed` reads `ddonpachj`'s chips from
the same .zip, merged, or `ddonpachj.zip` beside it; `tests/build.sh` builds `src_jpn/` in when it is there).

When they differ, `TRACE_FRAME=N` on both (the port built from `tools/decompile.py --trace`) and
`tests/trace_compare.py` show where the port first goes another way.

## Licence

BSD-3-Clause: see `LICENSE`. The board code is Cave68kSDK's, the translator is arcade-decomp's; each says
what it follows (MAME's drivers and devices, BSD-3-Clause). The window port is DonPachi Recomp's (BSD-3-Clause); the
icon is the program's own (`tools/make_icon.py`).
