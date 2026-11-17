# TODO

## Translating the program (step 4 of the SDK plan)

- [x] The ROM set read (both versions); the program's chips interleaved, checked by the reset vector.
- [x] The translator's profile: the memory map, the frame wait (0571AC, as DonPachi's).
- [x] DonPachi's call tables carried over by matching code (tools/match_code.py).
- [x] `--check` clean: all 1,650 routines translate (DoDonPachi's own calls added: the records' callbacks, the stage routines, a movem with A7).
- [x] The C builds against Cave68kSDK (all 1,662 routines; tests/build.sh, include/game_board.h).
- [x] It boots: the warning screen, the title screen and the attract mode's demo, drawn right (tests/boot.c).
- [ ] Names: DonPachi's symbols.txt carried over where the code is the same.
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
- [ ] The quicker picture (DonPachi Recomp's default, a frame sooner): needs the scroll and palette the coming
      interrupt sets, from the game's memory. For now the picture is drawn as the board shows it (arcade timing).
- [ ] The main menu, free play, high scores, training, replays, achievements, statistics, side panels, hitbox:
      each reads DonPachi's memory; DoDonPachi's addresses to find.
- [ ] A release package (tools/make_release.ps1, as DonPachi Recomp's).
