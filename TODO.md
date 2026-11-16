# TODO

## Translating the program (step 4 of the SDK plan)

- [x] The ROM set read (both versions); the program's chips interleaved, checked by the reset vector.
- [x] The translator's profile: the memory map, the frame wait (0571AC, as DonPachi's).
- [x] DonPachi's call tables carried over by matching code (tools/match_code.py).
- [x] `--check` clean: all 1,650 routines translate (DoDonPachi's own calls added: the records' callbacks, the stage routines, a movem with A7).
- [x] The C builds against Cave68kSDK (all 1,662 routines; tests/build.sh, include/game_board.h).
- [x] It boots: the warning screen, then the attract mode's demo (tests/boot.c; pictures still wrong in places).
- [ ] Names: DonPachi's symbols.txt carried over where the code is the same.
- [ ] The Japanese version (`ddonpachj`).

## The board (in Cave68kSDK)

- [ ] The picture: layer positions and colours, the 8-bit text layer, the sprites checked against MAME.
- [ ] The interrupt timing: when the VBLANK interrupt comes (MAME's per-game setting), checked against DonPachi's.
- [ ] **The arcade's slowdown matched** (owner: very important, for every game on this board). The translated code
      counts the 68000's cycles (the profile's TIMING_MODEL), so a frame whose work runs past the VBLANK waits a
      frame more, as on the board; checked against MAME in the busiest scenes.
- [ ] The YMZ280B sound chip.

## The port

- [ ] Window, controls, settings, save states, replays: DonPachi Recomp's port, carried over.
