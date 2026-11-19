# Changelog

What has changed in each release of DoDonPachi Recomp, newest first. Changes made since the last release go under
**Unreleased**; when a release is made, that section becomes its version (and is pasted into the GitHub release's
notes). The release zip carries this file as `CHANGES.txt`.

## Unreleased

Nothing yet.

## v0.1.0 (2026-11-19)

The first release: Cave's 1997 arcade shooter DoDonPachi, rebuilt from the arcade program as a native Windows and
Linux game. You need your own ROM set (MAME's `ddonpach.zip`, the International version).

### The arcade game, exactly
- The whole program translated to C and checked against the original running on MAME's 68000 core: the same at every
  frame through both loops, the true last boss and the ending, with every ship and style.
- **The arcade's slowdown**: the game slows down at the same moments as the real board, as it counts the 68000's time
  instruction by instruction (checked frame by frame against MAME). GAME OPTIONS > SLOWDOWN turns it off.
- **Low input lag**: the board's own timing as an option (INPUT TIMING: ARCADE), or a frame quicker (FAST, the
  default); the picture reaches a Windows screen through Direct3D 12 (about 1.5 ms after a frame is finished).

### Playing
- **Main menu**: START GAME, TRAINING, REPLAYS, ACHIEVEMENTS, STATISTICS, SETTINGS.
- **Training**: any stage of either loop, from its start or at its boss, with your choice of ship, style, power and
  bombs; RETRY ON MISS.
- **Difficulty** in GAME OPTIONS (EASY to VERY HARD): the arcade's own setting, without the test menu.
- **Side panels** on wide screens: your power and bombs, the board's speed and slowdown, the enemy bullets on screen
  with a 5-second graph, the stage's 13 bees, your stick and buttons; the score chasing the high score table, and
  each stage's score against your best for it.
- **Replays** of every arcade game, kept if you want them, to watch again or share.
- **24 achievements** and **lifetime statistics** (arcade games only).
- **High scores** kept between sessions.
- **Hitbox and aura displays**, save states, fast forward, free play (on by default).
- **Graphics**: a CRT look, pixel-art filters (Scale2x/3x, xBR, MMPX), scanlines, scaling modes, full screen.
- **Cheats**: P1 / P2 INVINCIBLE, and MAME's cheat file if you add it (a game with a cheat on doesn't count).
- **Menu sounds** from the game's own samples.
- **Updates from inside the game**: from this version on, a newer release is offered on the main menu (UPDATE TO
  V...) and installed when you say yes; your settings, scores and replays are kept. GAME OPTIONS > CHECK FOR UPDATES
  turns it off.
