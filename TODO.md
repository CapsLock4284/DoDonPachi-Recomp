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
- [ ] **An interrupt over the stack's temporary bytes, as the board has it** (asked the owner): 057A70 (a number to
      decimal digits) builds its digits below the stack pointer (movea.l A7, A0; move.b Dn, -(A0) x3, clr.b -(A0))
      and reads them back as a long; an interrupt in between (the YMZ280B's) pushes its PC and status register there
      on the 68000, and the digits read back are those bytes: a glitch the arcade and MAME show, the port not (the C
      keeps the bytes in its own variables). Rare: once in about 2 million frames of the checks (type B laser's run,
      game frame 47,518: 672 cycles more on the board, the game going its own way after). To match it: the runtime
      counting the interrupts taken and the status register as they came, the C of this routine writing the frame's
      bytes into its slots when one came between its writes and its read
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
- [ ] The side panels: DoDonPachi's own (owner: the game shows more on its screen than DonPachi did, so the panels
      will differ); later.
- [ ] **Widget ideas for the side panels** (owner, 18-11-2026):
  - The power level, 1 to MAX (the game's steps between, the halves the owner expects to find)
  - The stage's score against the best ever for that stage (a tracker per stage, kept with the high scores)
  - The board's FPS with the percentage of slowdown (the FPS meter's GAME and BOARD: 100 - GAME / BOARD)
- [x] The options menu (Escape / View): DonPachi Recomp's, in DoDonPachi's own font (its text layer's 8x8 tiles,
      8 bits a pixel, ASCII as they are): options, game options (slowdown, C button auto-fire, fast forward speed,
      the test mode, cheats), graphics, controls (remapping, both players, the hotkeys). Silent for now (DonPachi
      Recomp's menu sounds are that game's samples).
- [ ] The menu's sounds, from DoDonPachi's own samples (the YMZ280B's).
- [ ] Look-ahead for input latency (owner; not a priority): predictive emulation, as rollback in fighting games
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
- [ ] The main menu, training, replays, achievements, statistics, side panels, hitbox:
      each reads DonPachi's memory; DoDonPachi's addresses to find.
- [ ] **The aura overlay, not a priority** (owner, 18-11-2026: "would be cool to see"): DoDonPachi's ship has an aura, a small space around it where enemies take extra damage from its shots. An overlay as DonPachi Recomp's HITBOX shows the ship's hitbox, drawing the aura's range around the ship (an option, off by default; never changing the game). To find: where the game tests an enemy against the aura (its range, from the ship's position in its record at $102C8C, and whether it differs for shot and laser), in the code that adds an enemy's damage
- [ ] **Update from inside the program** (owner, 18-11-2026; DonPachi Recomp's TODO has the same): the options menu checks for a newer release and installs it, with no download or extracting by hand: GitHub's releases API for the latest tag (WinHTTP on Windows, libcurl on Linux, in the background, when the player asks), the release's zip downloaded and checked (size, SHA-256), unpacked beside the program, the exe swapped on the next start (a running .exe cannot overwrite itself). The player's files stay (settings, EEPROM, high scores, ddonpach.dat). The repository is private: an update from it needs the releases somewhere public (a public releases-only repository, or the repository made public), never a token in the program. After the release package
- [ ] A release package (tools/make_release.ps1, as DonPachi Recomp's).
