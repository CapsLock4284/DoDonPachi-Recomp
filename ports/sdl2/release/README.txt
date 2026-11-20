DoDonPachi (arcade) for Windows and Linux - version {VERSION}
============================================================

A native Windows and Linux version of Cave's 1997 arcade shooter DoDonPachi,
rebuilt from the arcade program. It does not come with the game's graphics, sound
or data: you need your own copy of the arcade ROM set.


GETTING STARTED
---------------
On Windows (DoDonPachi-{VERSION}-win64.zip):
1. Unzip this folder somewhere you can write to (your Documents or Desktop, for
   example; not inside "Program Files").
2. Double-click ddonpach.exe.
   Windows may say "Windows protected your PC" the first time, because the program
   is not signed. Click "More info", then "Run anyway".

On Linux (DoDonPachi-{VERSION}-linux-x64.tar.gz; a 64-bit Linux):
1. Unpack it somewhere in your home folder:
       tar xzf DoDonPachi-{VERSION}-linux-x64.tar.gz
2. Run ./ddonpach in that folder (or double-click it in your file manager).
   It uses your system's SDL2. If it says libSDL2-2.0.so.0 is missing, install it:
       Debian / Ubuntu / Mint:  sudo apt install libsdl2-2.0-0
       Ubuntu 25.10 and later:  sudo apt install libsdl2-compat
       Fedora:                  sudo dnf install SDL2
       Arch:                    sudo pacman -S sdl2

Then, on both:
3. A window asks for the ROM set. Drag MAME's ddonpach.zip (the International
   version, Master Ver. 97/02/05) onto it. The game takes what it needs from it
   once and saves it as ddonpach.dat beside the program; after that the .zip
   isn't needed. Merged and non-merged MAME sets both work. If your set is
   incomplete, the window lists the files it is missing.

The Japanese version (MAME's ddonpachj) isn't in this version yet.


CONTROLS (change them in SETTINGS > CONTROLS)
---------------------------------------------
                  Player 1        Player 2        Gamepad
  Move            Arrow keys      R F D G         D-pad / left stick
  Shot (hold:     Z               A               A
   laser)
  Bomb            X               S               B
  Auto fire       C               Q               X
  Start           1               2               Start
  Coin            5               6               -

  Esc / gamepad Back     the menu (pause, settings, save states, training)
  F3                     reset the game
  F4                     the arcade's test (service) menu
  F5 / F8                save state / load state (for this session)
  F6 / F7                the save state slot before / after (5 slots)
  F9                     the FPS meter
  Tab / LB (held)        fast forward
  F11 or Alt+Enter       full screen

Free play is on by default: Start drops in the credits a game needs.


FEATURES
--------
  - The arcade's slowdown: when the screen is busy the game slows down at the
    same moments as the real board, as it counts the 68000's time instruction by
    instruction (checked against MAME through both loops).
    GAME OPTIONS > SLOWDOWN turns it off
  - Main menu: START GAME, TRAINING (any stage of either loop, from its start
    or at its boss, with your choice of ship, style, power and bombs, and retry
    on a miss), REPLAYS, ACHIEVEMENTS, STATISTICS, SETTINGS
  - Training extras: ON PACE power (what you would have there from the
    power-ups on the way), SPEED (the game slowed to 50-90% to learn a place)
    and 2-7, the hidden Area 7: Hachi, or the true last boss Hibachi
  - Side panels on wide screens: your ship's power and bombs, the board's speed
    and slowdown, the enemy bullets on screen with a graph of the last seconds,
    the stage's 13 bees, your stick and buttons; the score chasing the high
    score table, and each stage's score against your best for it
  - Difficulty (GAME OPTIONS > DIFFICULTY): the arcade's own, from EASY to VERY
    HARD, without going into the test menu (it takes effect after F3 or a restart)
  - A CRT look (SETTINGS > GRAPHICS > CRT): a curved arcade screen with its
    scanlines, phosphor stripes and glow
  - Pixel-art filters (SETTINGS > GRAPHICS > FILTER): Scale2x, Scale3x, xBR 2x,
    3x and 4x, and MMPX, smoothing the picture's edges while keeping it sharp
  - Save states, fast forward, scanlines, borderless or exclusive full screen
  - Hitbox and aura displays (GAME OPTIONS > HITBOX, AURA): your ship's real
    hitbox (much smaller than the ship), and the laser's aura while it fires
  - High scores kept between sessions (not while cheats or save states were used)
  - 24 achievements (main menu > ACHIEVEMENTS): stage and loop clears, no-miss
    and no-bomb stages, all 13 bees, hit combos, scores and more. Earned in the
    arcade game only: not in training, or in a game with a cheat or a loaded
    save state
  - Replays (main menu > REPLAYS): every arcade game is recorded; when it ends
    you're asked whether to save it (in the replays folder). Watch or delete
    them from the menu, or share the .ddreplay files
  - Lifetime statistics (main menu > STATISTICS): games, play time, stages and
    loops cleared, deaths, bombs, bees, the best score and hit combo, per stage
    and per ship (arcade games only, as the achievements)
  - Sound (SETTINGS > SOUND): VOLUME for all of it, and MUSIC, EFFECTS and
    VOICES each on their own
  - Updates: as it starts, the game looks for a newer release on GitHub. If
    there is one, UPDATE TO V... shows at the top of the main menu: it asks
    first, then downloads it, checks it and starts the game again. Your files
    below are kept. GAME OPTIONS > CHECK FOR UPDATES turns the look-up off


CHEATS
------
The cheats page (SETTINGS > GAME OPTIONS > CHEATS) has the game's own:
P1 INVINCIBLE and P2 INVINCIBLE (nothing can hit that player's ship). For more
cheats, add MAME's cheat file for DoDonPachi (not included here); its cheats are
listed after the game's own:
  1. Download MAME's cheat collection, cheat.7z (from Pugsy's Cheats,
     https://www.mamecheat.co.uk).
  2. Open it with 7-Zip and take out ddonpach.xml.
  3. Make a folder called "cheats" beside the program (ddonpach.exe on Windows,
     ddonpach on Linux) and put ddonpach.xml in it, so you have
     cheats/ddonpach.xml in the same folder as the program. (Or set the full path
     as "cheats =" under [files] in ddonpach.ini.)
A game with a cheat on counts for nothing: no high scores, achievements,
statistics or replay.


FILES THE GAME KEEPS BESIDE THE PROGRAM
---------------------------------------
  ddonpach.dat       the game data taken from your ROM set
  ddonpach.ini       your settings (delete it to go back to the defaults)
  ddonpach.eeprom    the arcade board's own settings (the test menu's, and the
                     difficulty)
  ddonpach.hiscore   the high score table
  ddonpach.stages    your best score for each stage
  ddonpach.achievements   the achievements you've earned, and when
  ddonpach.stats     your lifetime statistics
  ddonpach.log       the program's messages (and ddonpach_previous.log, the last
                     run's): if the game ever closes by itself, this says why, so
                     it helps to send it with a report (with any
                     ddonpach_crash_*.inp file: the inputs that led there)
  replays\           your replays (.ddreplay files: share them, or delete the
                     ones you don't want)
  *.old, update_old.txt   left by an update for a moment: the program's old
                     files, deleted when the game next starts

Keep these if you move the game to another folder or computer. Don't share
ddonpach.dat: it is the contents of the ROM set.

ddonpach.eeprom is in MAME's format: to bring your settings over from MAME,
copy MAME's nvram\ddonpach\eeprom beside the program as ddonpach.eeprom (and
the other way round).

If the game's folder can't be written to (installed somewhere read-only), it
keeps them in your own folder instead: ~/.local/share/DoDonPachi/ on Linux,
%APPDATA%\DoDonPachi\ on Windows.


LICENSES
--------
See the licenses folder: this program's notices, and SDL2's license.
DoDonPachi is (c) 1997 Cave / Atlus. This is an unofficial fan project, not
connected with or endorsed by them.
