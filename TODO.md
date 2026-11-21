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
- [x] The Japanese version (`ddonpachj`, Master Ver. 97/02/05: its own program chips u27.bin and u26.bin, the rest
      the World set's; owner's merged ddonpach.zip has them in ddonpachj/). Done 20-11-2026, as DonPachi Recomp's:
      - The programs compared (tools/version_map.py, DonPachi Recomp's): 97.9% of the words matched, the code moved
        (by 0, -888, -952 bytes and so on); the work RAM the same (the 905 addresses each program names: the same
        ones), so the port's features (training, the panels, achievements, statistics, cheats, hitboxes, sounds) work
        on it as they are.
      - `python tools/decompile.py --set japan` writes src_jpn/ (about 70 minutes): symbols.txt's names carried over
        (build/decompile_jpn/symbols_jpn.txt: none left out), tools/call_tables.py's and game_profile.py's addresses
        through game_profile.here (the International address's place; where the aligner had no run, in 049DF4's five
        repeated copies, a nearby run's shift taken if the instruction is the same there), jpn_names.h naming its C
        jpn_NAME; CMakeLists.txt and tests/build.sh build it in beside the other (the SDK's DONPACHI_JAPAN_PROGRAM),
        the runtime runs the program of the set loaded.
      - Checked against MAME's 68000 (tools/cref/cref_timed and tests/slowdown_check with VERSION=1; cref_timed's
        frame wait the Japanese program's, 056DF4): the attract mode (30,000 VBLANKs), the test player's game
        (60,000) and, kept from harm, both loops into Area 7 (240,000): 309,710 game frames, all the same at every
        frame, the same frames slowed down.
      - The port: VERSION: WORLD / JAPAN first on the main menu (switch_version, from the reset: its data, EEPROM, high
        scores and stage bests, the save states and kept training places let go; DonPachi Recomp's), `[game] version`;
        a Japanese version chosen but not there at the start: the World one, told on the menu. Its data taken on the
        first run with the World one (owner, 20-11-2026: a merged ddonpach.zip or ddonpachj.zip beside it, nothing
        asked; the first run's window asks for ddonpachj.zip after ddonpach.zip, skippable), else asked for when
        JAPAN is first chosen; ddonpachj.zip alone goes over the World data. Checked (Linux, headless): merged,
        World alone and separate sets; started as Japan (its own EEPROM file)

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
      the test mode, cheats), graphics, controls (remapping, both players, the hotkeys). Its sounds: below.
- [x] **The menu's sounds, from DoDonPachi's own samples** (the YMZ280B's; owner, 18-11-2026: before the first
      release; `ports/sdl2/ui_sound.c`, as DonPachi Recomp's): MOVE the ship select's cursor ($2F1A68), CHOOSE its
      choice ($2F2AF1), BACK a short one of the game's ($2F465A), and the achievement pop-up the extra life's
      ($12960A): the owner's choice, by ear, of eleven the game plays (logged as each of the chip's voices starts:
      the coin, Start, the ship select, a power-up, a bee, an extra life; decoded to listen to, never kept in the
      repository). Decoded as the chip does (4-bit ADPCM at the game's frequency $5C, about 16,021 a second), at the
      game's own level for each, to 48 kHz, and mixed over the game's sound; while the menu has the game paused,
      over silence at the board's pace. Checked: the port's own sound output recorded (SDL's disk driver): each cursor
      move, the choice and going back where they were pressed, at the samples' own levels
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
- [x] **Training: ON PACE power and bombs** (owner, 18-11-2026: "so we're not hugely overpowered or underpowered when
      fighting bosses"), as DonPachi Recomp's POWER: ON PACE. Done 20-11-2026 (`training.c`'s on_pace,
      `[training] power = on_pace`, the page showing "ON PACE (2)"): the items logged as they come out (their records
      at $10A9F6, $20 each, 10 of them: $8010 a P, $8012 a bomb) through both loops, the test player with P1
      INVINCIBLE shooting all it can, each P taken seen in the power as it rises (a P raises both weapons a level):
      one P in 1-1, two in 1-2, the last that counts at 1-3's start; so level 1 at 1-1, 2 at its boss and 1-2's
      start, 4 at 1-2's boss and 1-3's start, MAX from 1-3's boss on and through loop 2 (the power carries over).
      Checked: training at each of the 24 places with ON PACE, the power set as the table says. Bombs need none:
      their most is 3 until a life is lost (each loss raises it by one, to 6), and the stock is full at each place
- [x] **Training fixes** (owner, 20-11-2026): ON PACE the default power; the boss places of 1-3 to 1-6 and 2-3 to
      2-6 began well into the fight (the scroll's end, up to 24 s after the boss came: in the owner's words, the
      boss "a bit too early"): every boss now from its WARNING (the owner's picture of 1-1's as the reference), as
      the boss's music (#56, $502B8E in nibbles) starts on the YMZ280B's voice 0; and 2-7, the second loop's hidden
      Area 7 (stage 6 of loop 1: 2-6's end set as the place's way there), at Hachi (its speech over, 38.6 s in) or at
      the true last boss Hibachi (Hachi shot down on the way: the ship at MAX firing from below its middle, about 17,000
      frames, unseen; then from Hibachi's WARNING, the boss's music again). Checked: each of the 14 boss places reached
      (1 to 4.7 s of loading here), pictures of each at its first frame and a second after. Then (owner: 1-5, 1-6,
      2-5, 2-6 a miss as they load): the enemies the run had left alive (it never fires) waited at the WARNING; the
      way to stage 5 and 6's bosses now fires at MAX (the score held at 0), its items taken away at the place, and
      the HIGH and its last digit put back as they were at the ship select, their sprites drawn again (the way to 2-6
      passes 2-5's clear: 6,000,000). The other places' frames unchanged. Checked: standing still at each place, no
      miss in 8 s (1-1 and 2-1: the boss's own fire at 8.2 s), the HIGH 5908065 at 1-5, 2-6 and Hibachi. Still a miss
      at 1-5 and 2-5 on the owner's PC (another setup: found here with EASY and ship C, a hit at 1.9 s): every place
      now starts with the game's own guard, $B4 (3 s, counted down a frame at a time, as after a life); checked at
      every difficulty, ship and style for 1-5 and 2-5, and every place: no miss in 10 s.
      Loading (owner: slow next to DonPachi): the way there run with cave_sound_quiet and the slowdown off (it only
      waits; main.c's menu_training), 1-1 flown firing at MAX when it is only the way (its boss down in seconds, not
      waited out: about 8,600 frames fewer), and the last 8 places reached kept in memory for the session (the same
      choices and EEPROM: put back at once). Measured here, all 26 places: 74.3 s before, 43.1 s after (a start
      0.2-1.5 s, a boss 0.5-2.8 s, Hibachi 3.1 s). Firing on the way to every boss was tried: slower (more on screen)
- [x] **Training: a slower speed to choose** (owner, 18-11-2026: "would let players ramp up to full speed for
      training"). Done 20-11-2026: SPEED on the training page (`[training] speed`, 50-100 in steps of 10), the
      board's frames paced further apart in main.c (game_speed: a training game, the menu closed), so the game is
      the same frame for frame. The sound slowed with it, as a tape (lower): the board's output stretched to
      100/speed as many samples (linear, from chunk to chunk); keeping its pitch (time-stretching) left for later if
      wanted. "SPEED 70%" at the picture's foot while it isn't 100%; the FPS meter shows the board's rate as played
- [x] **Difficulty in the game options** (owner, 18-11-2026: as DonPachi Recomp's, "an easier way of selecting
      difficulty without having to get into the test menu"): DIFFICULTY first in GAME OPTIONS (EASY / NORMAL / HARD /
      VERY HARD), the test mode's GAME DIFFICULTY: EEPROM word 4, bits 8-9 (0 NORMAL, 1 EASY, 2 HARD, 3 VERY HARD,
      as DonPachi's; found by setting each bit before the boot and reading what the game took: $101294 bits 0-1).
      What it does: at a game's start the game picks a table by it (rom_005092: 005092-0050C1), each stage's base
      rank into $101968: EASY 0 in every stage; NORMAL $10, $14 ... $2C; HARD $20, $26 ... $4A; VERY HARD $3F from
      the start. Read at the reset: the menu's note says F3 or a restart. The EEPROM saved as it changes (as the
      auto-fire's). Checked: the port (the page, the EEPROM file's word 4 after the change, kept after a restart).
      The high score table and the stages' bests keep all difficulties together, as the arcade's table does; to
      keep them apart (DonPachi Recomp's statistics keep a best score for each difficulty), later if wanted
- [ ] **MAYBE: a novice mode** (owner, 18-11-2026: maybe, not now): an easier game with fewer bullets. The owner's
      design: a true novice mode plays at the regular speed with fewer bullets, and nothing else changed (to start
      with, to see how it goes):
      - fewer bullets: of the bullets fired each frame (the records at $106CB6, +0 bit 15 in play; the side panel
        counts them), some taken away by a fixed rule (every other one): the patterns thinner, the same shape. To
        check: bullets that make others or drive an effect; the whole game run with it (tests/cheats_check's way)
      - its own version, chosen as the World and Japanese versions are (the main menu's VERSION: WORLD / JAPAN /
        NOVICE, as DonPachi Recomp's VERSION: USA / JAPAN): its own high score table (`ddonpach_novice.hiscore`),
        stage bests and EEPROM file, never mixed with the arcade game's; a "NOVICE" banner over the game's own
        SCORE RANKING screen (drawn over the picture by the port, the picture itself the arcade's), and on the
        side panel while it is played
      - both loops if the same rule holds through loop 2 (the whole-game check says); loop 1 alone otherwise
      - replays: the version in the replay's header, the same rule applied in playback (a fixed rule, no chance:
        in step); achievements and statistics: to decide (kept apart, or not counted)
      - fewer bullets also means less slowdown (the board less busy): one more reason it is its own version
      Kept for later, if the fewer bullets aren't enough: slower bullets (each one's speed halved as it is fired),
      enemies firing less or with less health; enemies taken away is the riskiest (the stages are scripted around
      them). Separately, the arcade's own difficulty in the game options and the training's slower speed (the
      entries above) are assists of their own, not the novice mode
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
- [x] Achievements, statistics: more (DonPachi Recomp's area targets have DoDonPachi equivalents to find: the
      hidden 1-ups, the true last boss). Done 20-11-2026, four more (28): HIDDEN EXTEND (the stage 3 ship's 1UP),
      HIDDEN STAGE (2-7 begun), HIBACHI (its WARNING), TRUE ENDING (Hibachi shot down: the ending). Found: the extends
      are func_044BBE's, by the score ($101950 / $101954 the next one's score, BCD: 6,000,000 then 20,000,000 as
      shown, then $FFFF), and the 1UP item's (func_04D178, func_04E5C0: +1 life, sound $48, its next score left as
      it is), so a life gained with the next extend's score unchanged is the 1UP's. 2-7 is the stage 6 of loop 1
      after 2-6 is cleared (the same values are the pause between the loops after 1-6); Hibachi's WARNING the boss
      music ($502B8E on voice 0, Hachi has its own); after it the loop is 2, the ending. Checked: `tests/
      achievements_check` (the test player, invincible, both loops: 2-7, Hibachi and the ending earned in order, the
      two score extends EXTRA LIFE only; the run again with a life given in 1-3 as the 1UP item gives it: HIDDEN
      EXTEND). Not seen: the real 1UP item taken (none of the test players gets it: all 6 side boxes of the stage 3
      ship opened with the laser, then its tower shot down, no bomb), and the Japanese version's runs
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
- [x] **Bullet hitboxes** (owner, 18-11-2026; DonPachi Recomp's TODO has the same): beside HITBOX and AURA, the
      enemies' bullets as the game tests them. Done 20-11-2026 (GAME OPTIONS > BULLETS, `[game] bullets`, off;
      `hitbox.c`): a bullet is a point, not a box: func_0057EE takes each record's +06/+08 (as the ship's position)
      and tests it inside the ship's box. The records: $106CB6, $40 bytes, 225 tested in the first loop, 245 in the
      second (five a pass, 45 or 49 passes); a record is passed over when its top byte has any of $51 (bits 14, 12, 8
      of +00: one that has hit, or can't); the overlay draws those in play (bit 15) without them. Checked: the points
      against the bullet sprites in the bank shown, 40,000 frames of the test player: the record a frame before the
      bank sits on its sprite's middle (0.14 pixels on average; 2 to 6 pixels at the other delays), the same delay as
      the ship's box, so it is drawn with the hitbox's. The game options' rows 11 pixels apart to fit it
- [x] **Separate volumes for music, sound effects and voices** (owner, 18-11-2026, players asked for it; DonPachi
      Recomp's has the same): SETTINGS > SOUND, VOLUME and MUSIC, EFFECTS and VOICES under it, in steps of 5, an
      example of each played while its volume is changed (owner, 19-11), `[sound] music / effects / voices`. Done
      19-11-2026: Cave68kSDK's groups in the YMZ280B's mix (each voice's level by where its sound starts,
      cave_sound_set_level); DoDonPachi's table from the program's own (66 sounds, pointers at $06D056: the music the
      last 11, #55-#65, all on voice 0), the voices #44-#54 chosen by the owner by ear, the rest effects. Checked
      through both loops: the board the same at any levels, at 100% the sound the same sample for sample, the groups
      adding up to the whole
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
- [x] A release package (tools/make_release.ps1 and .sh, as DonPachi Recomp's): the asset's name must end "-win64.zip"
      (and "-linux-x64.tar.gz" for Linux) and the release's tag be the version ("v0.2.0"), for the updater to find it.
      Done: v0.1.0 released with it (19-11-2026).
