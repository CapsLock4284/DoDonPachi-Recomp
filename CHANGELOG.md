# Changelog

What has changed in each release of DoDonPachi Recomp, newest first. Changes made since the last release go under
**Unreleased**; when a release is made, that section becomes its version (and is pasted into the GitHub release's
notes). The release zip carries this file as `CHANGES.txt`.

## Unreleased

### New
- **Help in the menus**, as DonPachi Recomp has: while a menu page is up (with the side panels on), the right panel
  explains the chosen entry: its name, then what it does. Every entry has one, from the training's places and ON
  PACE to the sound's groups, BY LAYER and VERSION.
- **LOOP 2 on the right panel**: whether this game still gets the second loop, from the game's own rule read from
  its program: YES (and how) once it is sure, the misses left while those alone would do it ("1 MISS LEFT"), what is
  still short otherwise, NO after a continue or with two players. In the second loop it becomes HIBACHI: AFTER 2-6,
  then 2-7 NOW!. The rule: alone, no continue, and any one of 2 misses or fewer, MAX HIT 624 / 768 / 816 (types A, B,
  C), 50,000,000 points or 4 stages with all 13 bees (README). The score graph is a little shorter for it.

## v0.3.0 (2026-11-21)

### New
- **The Japanese version** (VERSION on the main menu: WORLD / JAPAN): MAME's `ddonpachj`, the original Japanese
  release. Checked frame by frame against the arcade program on MAME's 68000, through both loops into Area 7, as the
  World one was. A merged `ddonpach.zip` already has it inside: its data is taken on the first run with the World one;
  with separate sets, put `ddonpachj.zip` beside `ddonpach.zip` or drop it on the first run's window (it can be
  skipped and added later, when JAPAN is chosen). Each version keeps its own high scores, stage bests and arcade
  settings; achievements and statistics are shared.
- **Faster xBR filters**: xBR 2x, 3x and 4x take about a quarter less time a frame (the same picture, pixel for
  pixel), and all the filters now share their work across up to 8 of your processor's cores instead of 4.
- **The FPS log** (`ddonpach_fps.log`) times the filter and the uploads to the graphics card on their own, so a
  slow picture shows where its time goes.
- **The next extra life** on the right side panel: how much more score it takes ("1UP IN"; the game gives one at
  6,000,000 and one at 20,000,000), with a dotted line on the score graph once it is in sight.
- **The FPS meter moved off the game**: with the side panels on, F9's meter is drawn on the right panel instead of
  over the picture's corner. Without the panels (or rotated) it stays in the corner.
- **Filters by layer** (SETTINGS > GRAPHICS > BY LAYER, `[video] filter_layers`, off by default): the FILTER now
  runs on each of the game's layers and its sprites on their own, then puts them back together the way the arcade
  board does. Sprite outlines stay clean instead of smearing into the background, and lines that cross (the mesh in
  stage 3) keep their shape. Works with Scale2x, Scale3x and MMPX (xBR mixes colours, so it stays on the whole
  picture). It costs a few more milliseconds a frame; the game itself is unchanged.
- **Four more achievements** (28 now): HIDDEN EXTEND (the stage 3 ship's hidden 1UP: open its six side boxes with
  the laser, then destroy its tower, no bomb), HIDDEN STAGE (reach 2-7), HIBACHI (face the true last boss) and TRUE
  ENDING (shoot Hibachi down).
- **Bullet points** (GAME OPTIONS > BULLETS, `[game] bullets`, off by default): each enemy bullet's point, a white
  pixel in a small green cross. The game tests a bullet only by that point, the middle of its sprite, so a bullet
  hits you only when its point gets inside your hitbox, however big it looks: with HITBOX on you see exactly what
  can hit you. Drawn over the picture only; the game is not changed.

### Fixed
- **A short hitch about once a minute** in play: the lifetime statistics were saved every minute in the middle of
  a frame, and on some PCs writing the file takes 10-20 ms (once over 100 ms, a visible stutter). The port's files
  (statistics, achievements, high scores, the stages' bests) are now written by a helper thread, so the game never
  waits for the disk.

## v0.2.0 (2026-11-20)

### New
- **Separate volumes for the music, the sound effects and the voices** (SETTINGS > SOUND, `[sound]` in
  `ddonpach.ini`): VOLUME sets all of it as before; MUSIC, EFFECTS and VOICES set each on its own, under it, in steps
  of 5% (VOLUME too, now). While you change one, an example plays at the level you are setting (a piece of stage 1's
  music, shots and enemies shot down, a voice), over and over until you move off it. Players asked for it. The game
  itself is untouched: only what you hear changes (checked through both loops: the board the same, frame by frame,
  whatever the levels; at 100% the sound the same as before, sample for sample).
- **Training: ON PACE power** (TRAINING > POWER, now the default): the power a player would have at the place from
  the power-ups on the way there, none missed; the page shows what it gives ("ON PACE (2)"). It rises from level 1 at
  1-1's start to MAX at 1-3's boss, and stays MAX in the second loop. So a boss is practised with the power it is
  really met with. 1 to MAX can still be chosen.
- **Training: the true last boss** (TRAINING > STAGE 2-7): the second loop's hidden Area 7, at its boss Hachi or at
  Hibachi, the true last boss (Hachi is shot down for you on the way, unseen: the loading takes a few seconds more).
- **Training: SPEED** (TRAINING > SPEED, 50% to 100% in steps of 10): the game played slower, to learn a place and
  work up to full speed. The game itself is the same, frame for frame (its slowdown, patterns and timings as they
  are), only slower; the sound is slowed with it (lower in pitch, as a tape played slow), and the speed shows at the
  foot of the screen while it isn't 100%.
- **Training loads faster**: about twice as fast to a stage's start and a third faster to a boss (the way there run
  unheard and without the slowdown, and 1-1's boss shot down on the way instead of waited out), and a place already
  loaded this session (the same choices) comes back at once.

### Fixed
- **Training: a boss place began too late into the fight** in 1-3 to 1-6 and 2-3 to 2-6 (up to 24 seconds in, the
  boss already firing): it began where the stage's scroll ends, which in those stages comes long after the boss.
  Every boss place now begins at the boss's WARNING, as the boss's music starts. In 1-5, 1-6, 2-5 and 2-6 the
  enemies the unseen run had left alive were then waiting there, firing as the ship came in (a miss at once): the
  way to those bosses is now flown firing, as a player would, and the items it drops are taken away; and every place
  now begins with 3 seconds without harm, as after a life (the ship blinks), so nothing left on the screen can take a
  life as you start. The HIGH on the screen is no longer raised by the way there (2-6's: the second loop's 6,000,000
  area bonus).

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
