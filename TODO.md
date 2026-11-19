# TODO

## Translating the program (step 4 of the SDK plan)

- [x] The ROM set read (both versions); the program's chips interleaved, checked by the reset vector.
- [x] The translator's profile: the memory map, the frame wait (0571AC, as DonPachi's).
- [x] DonPachi's call tables carried over by matching code (tools/match_code.py).
- [x] `--check` clean: all 1,650 routines translate (DoDonPachi's own calls added: the records' callbacks, the stage routines, a movem with A7).
- [x] The C builds against Cave68kSDK (all 1,662 routines; tests/build.sh, include/game_board.h).
- [x] It boots: the warning screen, the title screen and the attract mode's demo, drawn right (tests/boot.c).
- [x] The whole game checked against MAME's 68000 (owner, 18-11-2026): player 1 kept from harm (INVINCIBLE=1 in
      tests/slowdown_check and tools/cref/cref_timed: $102CCA, the time without harm after a life, held), both loops to
      the last boss. The base player (tests/player.h) and the six of tests/players/ (types A, B, C, shot and laser, button
      C, bombs): 300,000 VBLANKs each, the same at every game frame (270,000-290,000 each), but type B laser's (below).
      Found and fixed: 02C52C's command 4 (call_tables.py PAST_THE_STUBS: the objects' timed scripts), and the movems
      with A7 (MOVEM_WITHOUT_SP) 8 cycles short each (their time now the 68000's). The C now built with
      `decompile.py --checks`: a jump-table value with no case stops the program (invalid_jump, the crash log and the
      replay), never a silent skip. A sweep with every switch reporting (all players, the attract mode, continues,
      2 players, the test mode) found no other. Tools: CYCLELOG=FILE (each frame's cycle at its wait, both sides),
      cref_timed TRACE_CYCLES=1 (each traced instruction's cycle)
- [x] **An interrupt over the stack's temporary bytes, as the board has it** (owner: "this needs to mirror the
      arcade board, that's the point"): 057A70 (a number to
      decimal digits) builds its digits below the stack pointer (movea.l A7, A0; move.b Dn, -(A0) x3, clr.b -(A0))
      and reads them back as a long; an interrupt in between (the YMZ280B's) pushes its PC and status register there
      on the 68000, and the digits read back are those bytes: a glitch the arcade and MAME show, the port not (the C
      keeps the bytes in its own variables). Rare: once in about 2 million frames of the checks (type B laser's run,
      game frame 47,518: 672 cycles more on the board, the game going its own way after). To match it: the runtime
      counting the interrupts taken and the status register as they came, the C of this routine writing the frame's
      bytes into its slots when one came between its writes and its read.
      Done 18-11: the frame's bytes there are the PC's alone (the 68000 pushes the PC at A7-4..A7-1, its status
      register below), so no flags are needed. tools/call_tables.py STACK_DIGITS (057A6E and its twin 001F98, the
      only two: a scan for "movea.l A7, An"), applied by tools/decompile.py after the core writes src/
      (interrupt_frames: after each of the 4 writes, if program_interrupts() moved, the slots get the next
      instruction's address). Checked: all 7 full-game runs the same as MAME's 68000 at every frame, type B laser's
      included (290,204 game frames)
- [ ] The Japanese version (`ddonpachj`).

## The board (in Cave68kSDK)

- [x] The picture: the 8-bit text layer, the palette's banks of 256, the sprites' byte order (the stage backgrounds
      are sprites: layers 0 and 1 stay empty).
- [x] The picture checked against MAME's source: the layers' offsets, the background colour, the colour banks and
      the priorities are MAME's (cave_v.cpp, sprite013.cpp, init_ddonpach). The small dark shapes are the wrecks
      ground enemies leave (their colour bank is dark). The layers' row-scroll and row-select added to Cave68kSDK
      (MAME's), in case a later stage uses them (the first stage and the demo do not).
- [ ] The picture side by side with MAME's own, from a later stage (needs a player that gets further).
- [x] **The arcade's slowdown matched** (owner: very important, for every game on this board). The original program on
      MAME's 68000 core at the board's timing (tools/cref/cref_timed) against the port (tests/slowdown_check), the
      same inputs (tests/player.h): over 150,000 VBLANKs (about 42 minutes), the same board at all 147,446 game
      frames, on the same VBLANKs, 2,553 VBLANKs lagged in both; and with a second player (sweeping faster, a bomb now
      and then) 147,002 of 147,002. Found and fixed on the way: three jumps past their stub tables, a routine reading
      its caller's stack, the core's DIVU by a constant (it lost the remainder), a jump table whose fourth case is the
      code after its three stubs (053E20: sprites of priority 3), and four object types whose first pointer is code,
      not a count (147, 149, 180, 206: made by 03622A, which calls that pointer at 036278).
- [ ] The same check with a player that goes further into the game (the test player only plays the first stage's
      start; the busiest scenes are later): the attract demo and the first minutes are matched now.
- [x] The YMZ280B sound chip (in Cave68kSDK, after MAME's ymz280b.cpp): its voices, the mix, and its interrupt at the
      sample a voice ends, in the port and in tools/cref alike. DoDonPachi's interrupt handler tells the chip's
      interrupt from the VBLANK's by the time (the cause's bit 2), so it is part of the slowdown: checked again over
      150,000 VBLANKs, the same at all 147,505 game frames (2,494 lagged in both), and 147,399 of 147,399 with the
      second player (2,600 lagged).
- [ ] Listen to it against MAME (the levels, and the samples' order u6 then u7).

## The port

- [x] The game in a window (ports/sdl2, DonPachi Recomp's port cut down): the picture with its video settings
      (scaling, scanlines, the CRT look, the pixel-art scalers, rotation), the sound, keys and gamepads
      (ddonpach.ini), the EEPROM kept, save states, fast forward, MAME's cheat files, the ROM drop window, its own
      icon. The test mode: the service switch held 2 seconds at the title (the game ignores it on its warning screen
      and at the reset, so F4 holds it instead of resetting).
- [x] Auto-fire on button C (owner): the test mode's "C BUTTON FULL-AUTO", as DonPachi Recomp's home default (EEPROM
      words 1 and 4, bit 2 of word 4); [game] c_button_full_auto, on by default, sets it as the game starts.
- [x] Type C's pods (owner): they showed for a frame and never fired. A translation bug: the jump table at 00E008
      (by the ship's type) had type C's code right after its 2 stubs, which the translator missed. Fixed in
      tools/call_tables.py (PAST_THE_STUBS); every ship type and style checked against the original on MAME's 68000
      with button C held, the laser and bombs: exact over 20,000 VBLANKs each.
- [ ] Better graphics, never touching the game (owner idea): the picture is made from the board's memories after the
      frame's work, so the renderer can change freely. First: the layers and sprites scaled one by one (sharper than
      the whole frame's scalers), an option off by default, in Cave68kSDK's video.c for every game on the board.
      Later perhaps: replacement art packs, a GPU shader pipeline; a wider picture is a judgment call (the game
      only places things on its own 240 x 320).
- [x] **The side panels, DoDonPachi's own** (18-11-2026; `ports/sdl2/widgets.c`, GRAPHICS > SIDE PANELS,
      `[video] side_panels`, on by default): as wide as the screen leaves room for (owner: DonPachi's left black at
      the sides of a 16:9 screen): main.c works the width out from the screen's shape, WIDGET_W (120) to 256 game
      pixels, at the picture's own scale; the game's picture is never changed (owner: it must stay the arcade's).
      Each widget is laid out for any width. The game's memory they read is listed in widgets.h (found by
      watching whole games of the test players). Checked with `tests/panels_shot` (pictures of the panels beside
      the game at 120, 156 and 251 wide) and the port itself (SDL's offscreen driver).
- [x] **Widget ideas for the side panels** (owner, 18-11-2026):
  - [x] The power level, 1 to MAX: DoDonPachi keeps two levels, 0-4 each ($102CAC the style's own weapon, $102CAE
        the other): a power-up raises both, so there are no halves; a life lost takes the first down one and the
        second back to 0. Shown as two rows of five (SHOT, LASER: LV 1-4, MAX)
  - [x] The stage's score against the best ever for that stage (`ddonpach.stages`: the 12 stages of both loops,
        from clean games; a stage after a continue doesn't count). As a list (owner, 18-11-2026): the loop's six
        stages one under the other, each finished one's score and + or - against its best, the one being played
        live, those to come with their best
  - [x] The board's FPS with the percentage of slowdown (over the last second, this stage, this game)
  - [x] **The bee tracker, from DonPachi** (owner, 18-11-2026): the stage's 13 bees on the left panel, greyed until
        taken ($103F8E: the bees since the stage began or the last miss; the panel counts its rises in each stage;
        the icon from the bee's own sprite, $4496 in palette $17, at half size)
  - [x] **The high score chase** (owner, 18-11-2026): the score against #5 of the table, then #4 once passed, up to
        #1; a line in each place's colour on the graph where it was passed
- [x] **Fixed** (owner's playtest, 18-11-2026): a game started from the ranking after a game over (Start pressed
      there, the attract mode never coming) was taken for the last one going on: its score back to 0 read as a
      continue, so its stages showed UNRANKED, and the graph and stage list went on from the last game. The game's
      mode ($1017A6: $B the ship select, 7 playing, 8 the continue's count down, 4 the ranking) now says when a game
      is on and when a new one begins; the high score file's "a new game" (a tainted game's next) the same. A
      real continue still makes its stage UNRANKED (its score starts again from 0 part way); the stages after count.
- [x] **The input display, as DonPachi Recomp's** (owner, 18-11-2026): at the left panel's foot, under the bees, the
      stick (3 x 3 cells, the way it points lit) and the S, B and C buttons, lit while held: what the board is given
      each frame (IN0, or IN1 when the panel follows player 2), so what the game reads. The bees' section a little
      tighter for the room (at 120 wide: the bees in two rows, the controls under them). Checked on pictures
      (`tests/panels_shot` at 120, 156 and 251 wide: the stick and SHOT lit as the test player holds them)
- [x] **Bullet count with the slowdown** (owner, 18-11-2026): on the left panel's BOARD section, under the FPS: the
      slowdown now, BULLETS (the enemies' bullets on screen), and a graph of the last 5 seconds (288 board frames,
      the newest at the right): a bar for the bullets (the most in a column's frames), red where a frame was slowed,
      gold where not, against the most in that time (40 at least, to 20s). It replaces the slowdown's bar; "OF
      57.55" beside the FPS where the panel is wide enough. The bullets: the records at $106CB6, $40 each, 245 of
      them (to $10A9F6; func_0057EE tests the first 225 in loop 1, all in loop 2), +0 bit 15 in play (the slots
      not in play keep their old positions); counted on the frames that aren't slowed (the memory half way through
      a frame otherwise). Checked: each counted bullet marked on the picture lands on one (114 in 1-2's boss fight);
      the table at $10A9F6 ($20 each, 160, tested after them with the ship's box moved) is not the bullets. Found
      with it: the slowdown isn't only bullets (1-2: 33% slowed with 9 on screen, a crowd of tanks and explosions)
- [ ] **MAYBE: the second loop's qualification** (owner, 18-11-2026: maybe): whether this game can still reach loop 2
      ("LOOP 2: ON TRACK, 1 MORE MISS ALLOWED"), from the game's own rule (the lives lost and bombs used in loop 1, a
      continue; to read from the code that decides it at 1-6's end, not from memory), and the true last boss's own
      conditions in loop 2 the same way
- [ ] **MAYBE: a death map** (owner, 18-11-2026: maybe): where each life was lost and each bomb used in a stage, as
      marks on a bar of the stage's progress (its scroll position, $10198E), kept across games (in a file beside
      `ddonpach.stages`), so the stages' hard places show
- [ ] **MAYBE: the next extra life** (owner, 18-11-2026: maybe): the score of the next extend and how far there is to
      go, from the game's own extend scores (to find, where it gives the life: game_watch.c's "extended" event)
- [ ] **MAYBE: a hit chain meter** (owner, 18-11-2026: maybe): the live hit count large, the chain's timer draining as
      a bar, and the best chain this stage and this game ($101842: player 1's MAX HIT; the count and its timer to
      find)
- [ ] Side panel ideas still open: the lives; the hit counter's chain (the GP gauge's DoDonPachi equivalent);
      the boss's health; the stage's progress (its scroll position); the run's statistics (bombs used, misses by
      stage); the high score chase and the stage bests per ship and style
- [x] The options menu (Escape / View): DonPachi Recomp's, in DoDonPachi's own font (its text layer's 8x8 tiles,
      8 bits a pixel, ASCII as they are): options, game options (slowdown, C button auto-fire, fast forward speed,
      the test mode, cheats), graphics, controls (remapping, both players, the hotkeys). Silent for now (DonPachi
      Recomp's menu sounds are that game's samples).
- [ ] The menu's sounds, from DoDonPachi's own samples (the YMZ280B's).
- [x] **Input latency checked against the arcade, exact** (owner, 18-11-2026: "I want this to be exact"): measure,
      frame by frame, how many frames pass from a key press to the first picture that answers it, and compare with
      the board. On the board: the inputs are read by the game's code at a known point of its frame (in the vblank
      interrupt or the main loop: find where IN0 is read), and the picture the board shows is the one the 013 and the
      layers have at VBLANK. In the port: the key read at the frame's start (read_keys), the game's frame, the
      look-ahead's picture, then the window's present (and the monitor's own lag after it). A test: a scripted press
      at a known frame, the frame its effect first appears in the picture (the ship's sprite moving), both on MAME
      (cref) and the port, with INPUT TIMING FAST and ARCADE; then the port's own extra from SDL's input and the
      present (the time from the key's event to the present, logged as the FPS log does). The aim: ARCADE timing the
      board's latency exactly, FAST one frame less (the look-ahead), and nothing else added by the port.
      Done 18-11 (tests/latency_check): the game reads IN0 in its vblank interrupt (level_1_interrupt), as VBLANK
      starts; read_keys sets the inputs just before that interrupt runs, so the port samples where the board does.
      A press read at VBLANK N first shows in the picture after VBLANK N+2 on the board (the game moves and fires a
      frame after it reads, the board shows a frame's work after the next VBLANK): 2 frames. ARCADE: 2 window frames
      from the read_keys that takes it, the board's exactly; FAST: 1 (one better). Both keys (left, shot), at 3
      places in the game. The port adds the PC's own work before the picture is shown (2.7 ms a frame on the owner's
      PC, the board's blanking 2.0 ms) and what Windows and the monitor add after the present.
- [x] The display's own delay (after the present): the owner's PC uses SDL's "direct3d" renderer (Direct3D 9) in
      borderless full screen, which Windows' compositor (DWM) may hold up to a refresh before the monitor shows it.
      To look at: Direct3D 11 (flip model: "independent flip" in borderless full screen skips the compositor) or
      exclusive full screen, checked with PresentMon (its present mode and latency). The [video] renderer setting
      (GRAPHICS > RENDERER: auto, direct3d, direct3d11, direct3d12, opengl, software) is in, and auto is now
      direct3d12 on Windows: PresentMon on the owner's PC (borderless, vsync off) showed it 1.4 ms from present to
      screen against 27 ms for direct3d (README). Done
- [ ] Look-ahead for input latency, "better than arcade" (owner; an option, after the exact check above): predictive
      emulation, as rollback in fighting games
      (RetroArch calls it run-ahead). Each frame: the game kept (program_state_save), run on 1-2 frames with the
      inputs held now, that frame shown, then put back and run the one real frame; the sound from the real frames
      only. It hides the frames the game itself waits before it answers an input, without changing what it does
      (the real run is the same as without it, slowdown included). Costs 2-3 times the game's work a frame; the
      saved state must take the sound chip and the frame's timing with it.
- [x] The quicker picture (owner: input lag as low as possible): each frame shown as its VBLANK comes, a frame
      sooner. Generic, no addresses: a look-ahead of one frame (state saved, next frame run silently, its interrupt
      draws the picture, state put back). Checked: the same game at every frame over 30,000 VBLANKs, each picture the
      same as the next real frame's, type C with button C the same as MAME over 18,000 VBLANKs. The save states now
      keep the program's timing and the EEPROM's transfer (Cave68kSDK). INPUT TIMING (FAST / ARCADE) in the game
      options. It is the base for the run-ahead above (more frames, the inputs held).
- [x] Free play (on by default): Cave68kSDK's free_play.c, now for both games (each board description says where
      its credits are: DoDonPachi's word at $1013AA, 2 a game when bit 0 of $101297, DISCOUNT CONTINUE, is set).
      Start drops the coins, then presses Start (6 frames on, 6 off) until the game takes it, at most 5 s: on the
      title the game may take it 150 frames after the credit. Checked: player 1 from the attract and the title,
      player 2 joining, 2 credits a game; DonPachi's the same as before. With DISCOUNT CONTINUE a continue takes 1:
      free play drops 2, one left over.
- [x] High scores (owner): ports/sdl2/hiscore.c, DonPachi Recomp's with DoDonPachi's table ($1016EA-$10174D: five
      BCD scores, five names, then five words each: area, ship and style, max hits, continues). The game's HIGH
      ($101626) and its last digit ($101750: the score's last digit is the continues used) put back from #1. Not
      kept from a game with a save state or a cheat: the file's table and HIGH put back when it ends (between games:
      $1017A5 bit 7). Checked: a new #1 entered and saved, put back after a boot (the SCORE RANKING shows it), a
      save-state game's entry dropped and the HIGH put back, the file untouched.
- [x] The start's WARNING screen skipped (owner): its 1,809 frames run unseen at once (0.15 s), the game unchanged;
      SKIP WARNING in the game options.
      Fixed 18-11 (owner: the FPS log's burst right after starting): a game started before the reset's frame 2,400
      cleared the between-games flag again, and up to 577 frames of it ran unseen too; the skip now ends for good
      when the warning is over (until the next reset).
- [x] FPS meter (owner: "feels slower than the original"): F9 / FPS METER, GAME (the game's frames), BOARD (the
      board's: 57.55), WINDOW, LATE, WORK. The picture is now drawn only when it is shown (the look-ahead's, or with
      arcade timing the last frame's): half the drawing a frame, nothing drawn in fast forward's unseen frames or the
      warning's skip (which had drawn each of its 1,809 frames: seconds, not 0.15 s). In the cloud (software
      renderer): 57.5 / 57.5 / 57.5, LATE 0, WORK 6 ms (was 11).
- [x] Frames dropped in heavy action on the owner's PC (owner: about 42 fps in explosions): the picture's drawing
      was the cost (4.1 ms a frame on average, 13.5 at most; twice a frame before the look-ahead drew only its own).
      Cave68kSDK's video.c: the layers in one pass, the sprites clipped once: 1.1 ms (identical pictures, every frame
      of 14,000, both games). The meter's log (ddonpach_fps.log) times each part of a frame.
- [x] Crash in stage 3 (owner's report; the attract mode too, at its frame 8,436): "the code keeps a stack the C does
      not have, at 02C5C2". 02C5B8 keeps a long for each of its objects on the stack (pushed in one loop, popped into
      the objects last first in the next: all read before any written); the translation followed the stack 2 places
      deep. tools/call_tables.py MORE_DEPTHS gives it 16 (its callers: 10 and 9 objects).
- [x] The crash log (owner): a message box; ddonpach.log kept from the run before (ddonpach_previous.log); the inputs
      since the reset written at a stop or crash (ddonpach_crash_DATE_TIME.inp, crash_log.h), played again by
      tests/replay_crash (the same EEPROM, slowdown, high score table put back). Not replayable after a save state, a
      cheat or a change of the slowdown setting (the log says so).
- [ ] The slowdown against the real MAME (owner's report): our checks compare MAME's 68000 core on the SDK's board
      model; a recording of MAME's own ddonpach (its frames of slowdown over a known input run) would check the
      board model's timing itself.
- [x] **The main menu** (18-11-2026; menu.c's PAGE_FRONT, as DonPachi Recomp's): START GAME, SETTINGS, QUIT TO
      DESKTOP, the title "DODONPACHI" between two of the game's bees, the version (International or Japanese),
      the high score kept and the program's version; as the program starts ([game] main_menu, MAIN MENU AT START in
      the game options) and from the options' MAIN MENU (the board reset). The game waits at its start while it is
      up. Its entries to come as they are made: TRAINING, REPLAYS, ACHIEVEMENTS, STATISTICS, VERSION.
      Found with it: a reset (F3, the menu's RESET, MAIN MENU) showed the WARNING screen again although SKIP
      WARNING was on (the reset comes with the next frame, and the work RAM still held the last run's flag): fixed
      in main.c's warning_showing.
- [x] **Training** (18-11-2026; `ports/sdl2/training.c`, the main menu's TRAINING, `[training]`): a stage's start or
      its boss, 1-1 to 2-6, the ship, style, power (both weapons, 1-4 or MAX) and bombs (3 or 6), RETRY ON MISS;
      in a training game's options RETRY TRAINING and TRAINING MENU. The way there: from the reset, a coin, Start,
      the ship and style chosen; 1-1 played kept from harm ($102CCA), not firing, until it is cleared ($1017A4 = 3),
      when the stage and loop are set ($101978, $10197A) to the one before the place's (2-1: 1-6, through the loop's
      end); a boss: on to where its stage's scroll ends ($101996 = $FFFF). There: score 0 and the HUD's digits
      cleared ($10162A: 8 sprites), 3 lives ($101965), the bombs ($102CB0), the power. All 24 places reached
      (`tests/training_check`: 0.3-4.6 s each on the build machine, the bosses of 1-3 and 2-3 the longest).
      Still to do, as DonPachi Recomp's: a point part way through a stage, the mid-bosses, the difficulty, ON PACE
      power, the true last boss (2-6's second), a quicker way (1-1's boss is waited out)
- [x] **Replays** (18-11-2026; `ports/sdl2/replay.c`, on the crash log's recording: crash_log_view): each game, from
      its ship select ($1017A6 = $B) to its end (the mode out of 7/8), asked about at its game over (SAVE THIS GAME'S
      REPLAY?), saved to replays/ as "DDREPLAY": the run from the board's reset (the EEPROM, slowdown, the high score
      table put back, each frame's inputs), the game's first frame, and the board's hash every 600 frames. The main
      menu's REPLAYS: the list (date, stage, score), WATCH, DELETE. Playback: the board reset with the replay's own
      EEPROM, slowdown and table (the player's back after), the frames before the game run unseen; "THE REPLAY IS OUT
      OF STEP" if a check differs; STOP REPLAY, a reset or the main menu end it. Checked: `tests/replay_check` (each
      game of a run recorded from the power on, played back from a reset: in step all the way, the same end) and the
      port (a game over, YES, the file played back in step). Still to do, as DonPachi Recomp's: names for replays,
      training attempts as replays, a replay of a game quit part way, pause and frame step in playback
- [x] **Achievements and statistics** (18-11-2026; `ports/sdl2/achievements.c`, `stats.c`, on `game_watch.c`:
      the game's events each frame from its memory, `game_memory.h`): 24 achievements (the stages and loops, one
      credit, no miss, no bomb, the bees, hit combos 100/500/1000 ($101842, player 1's MAX HIT), scores 10M/50M/100M,
      full power, 6 bombs, an extra life, two players, every ship, both styles, 5 minutes without dying), a toast on
      the right panel; the statistics' totals, records, each stage's and each ship's. Checked headless over a whole
      two-loop game (tests via the test players) and the port's pages. Player 1 only for the lives, the combo and
      the score (player 2's places to find).
- [ ] Achievements, statistics: more (DonPachi Recomp's area targets have DoDonPachi equivalents to find: the
      hidden 1-ups, the true last boss)
      each reads DonPachi's memory; DoDonPachi's addresses to find.
- [x] **The hitbox and aura overlays** (18-11-2026; `ports/sdl2/hitbox.c`, GAME OPTIONS > HITBOX and AURA): the
      ship's box from its record (+06/+08 the position, +10..+16 the reach: the bullets' test, func_0057EE), and
      the aura's own record ($103BAE player 1, $103ECE player 2: in play while the laser is fired, at the ship;
      type A shot's reaches 56 pixels ahead, 16 behind and to the sides). Drawn two frames back (three with arcade
      timing): measured, the ship's sprite against its record. Checked on pictures (headless and the port).
- [x] **The aura overlay, not a priority** (owner, 18-11-2026: "would be cool to see"): DoDonPachi's ship has an aura, a small space around it where enemies take extra damage from its shots. An overlay as DonPachi Recomp's HITBOX shows the ship's hitbox, drawing the aura's range around the ship (an option, off by default; never changing the game). To find: where the game tests an enemy against the aura (its range, from the ship's position in its record at $102C8C, and whether it differs for shot and laser), in the code that adds an enemy's damage
- [x] **An invincibility cheat** (owner, 18-11-2026: to see every stage before the release, "a very difficult game"):
      `ports/sdl2/own_cheats.c`, P1 INVINCIBLE and P2 INVINCIBLE on the CHEATS page (cave/cheats.h's own cheats, there
      without MAME's cheat file; `--cheat "P1 INVINCIBLE"` on the command line). The ship's record's +$3E (byte), the
      time without harm after a life ($102CCA for player 1, $102D0A for player 2: not 0, the ship isn't hit, counted
      down; $FF the game's own, left alone), held at $80 while the player is in a game, as the checks' INVINCIBLE=1.
      Checked: `tests/cheats_check` (the test player with the cheat on as the menu sets it, from the power on: both
      loops, 2-6's true last boss and the ending, no life lost, in 39 s; without it, 20 lives lost in 1-1's first
      30,000 VBLANKs), and the port (the ship drawn as without it: the glow every other frame is the laser's aura, as
      the arcade's)
- [ ] **Bullet hitboxes** (owner, 18-11-2026; DonPachi Recomp's TODO has the same): beside HITBOX and AURA, the
      enemies' bullets as the game tests them, so a player sees what can hit the ship and what can't. The bullets
      are the records at $106CB6 ($40 each) that func_0057EE tests against the ship's box (+$10..+$16 of its record);
      to find: whether a bullet is a point or has a box of its own (its reach, by its type), the table's length and
      which records are in play. Drawn from the same frame as the sprites (two frames back, three with arcade timing),
      as the hitbox. An option (BULLETS, off by default), only drawn over the picture: the game is not changed.
      Later perhaps the enemies' own boxes (where the ship's shots land)
- [ ] **Separate volumes for music, sound effects and voices** (owner, 18-11-2026, players asked for it; DonPachi
      Recomp's TODO has the same): three settings beside VOLUME. The YMZ280B's eight voices play samples from the
      ROM, each at its own start address: a table of which sample is music, which an effect and which a voice (the
      samples logged as they start in the menus, the stages and the bosses, then checked by ear) gives each voice its
      level as it starts, in Cave68kSDK's mix. Only the mix changes: the game, its timing, the chip's interrupt and
      the slowdown are untouched
- [x] **Update from inside the program** (owner, 18-11-2026: "ask, then install"; the repository made public for
      it; `ports/sdl2/updater.c`, the same file in DonPachi Recomp): as the program starts ([game] check_updates,
      CHECK FOR UPDATES in the game options, on by default) GitHub's releases/latest is looked up on a thread
      (WinHTTP on Windows, the curl command on Linux); a newer tag with an asset ending "-win64.zip" (Linux
      "-linux-x64.tar.gz") puts UPDATE TO V... at the top of the main menu; its page asks (YES, UPDATE / NOT NOW),
      downloads with a percentage, checks the asset's SHA-256 (GitHub's "digest"), unpacks it (the runtime's
      inflate; .zip and .tar.gz; its top folder left off; no ".." or drive in a name) into update_new/, renames each
      file it replaces to .old (Windows lets a running .exe be renamed) and moves the new one in (all put back if one
      fails), then the program saves and closes and starts the new one with the same arguments; the next start
      deletes the .old files (update_old.txt). The player's files are never in the package. Checked:
      `tests/updater_check` against DonPachi Recomp's real 0.9.0 release (its JSON, both packages, the install and
      restart, the clean-up), and the port itself pointed at that release (offscreen: the entry, the question, the
      download, the install, the restart)
- [ ] A release package (tools/make_release.ps1, as DonPachi Recomp's): the asset's name must end "-win64.zip"
      (and "-linux-x64.tar.gz" for Linux) and the release's tag be the version ("v0.2.0"), for the updater to find it.
