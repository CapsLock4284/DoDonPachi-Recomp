/**
 *    Name:        hiscore.h
 *    Description: the high score table kept in a file (ddonpach.hiscore beside the program), as MAME's high score
 *                 plugin does: the arcade board keeps only its settings (the EEPROM), and its table goes back to
 *                 the defaults each time it is switched on
 *    Version:     0.1.0 (draft)
 *    Created:     17-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's hiscore.c, with DoDonPachi's table. The table, in the game's memory ($1016EA-$10174D, set
 *    up once at boot, in 00043A; a new entry put in by 0430F6):
 *      $1016EA  the five scores (BCD longs, #1 first: 590806 ... 356276 by default)
 *      $1016FE  the five names (3 words each: the font's tiles)
 *      $10171C  $101726  $101730  $10173A  $101744   five words each for the five: the ranking's other columns
 *               (the area, the ship and its style, the max hits; $101744 the continues used)
 *      $101626  the HIGH the game shows: #1's score, until a player's goes over it (044BBE), and $101750 its last
 *               digit (the score's last digit is the continues used: tile $100 + them), put back from #1's entry
 *    The file's table is put back over the defaults as soon as the game has set them up (after its boot, and
 *    after each reset); then the table is saved whenever the game changes it. Nothing else is touched.
 */
#ifndef DDONPACH_HISCORE_H
#define DDONPACH_HISCORE_H

#include <stdbool.h>

/* The file ('path', or NULL: ddonpach.hiscore beside the program), read now: NULL, or what was wrong with it (a
   file that is not there is no problem: the game keeps its defaults until there is one) */
const char *hiscore_open(const char *path);

/* The board was reset: its table is set up again (to put the file's back over) */
void hiscore_reset(void);

/* A save state was gone back to (cave/program.h): the file's table put back over the game's (its scores stay the
   file's), and kept as it is */
void hiscore_state_loaded(void);

/* A game that went back to a save state, or had a cheat on, is over: the file's table put back, and its HIGH (#1's
   score: none of that game's) */
void hiscore_game_not_kept(void);

/* After each of the game's frames. 'enabled': the table is put back and saved ([game] save_high_scores); 'saving':
   changes may go into the file (not in a game that had a cheat on or went back to a save state) */
void hiscore_frame(bool enabled, bool saving);

/* No game is being played (the attract mode, the title): the byte at $1017A5 is $80 then, 0 in a game */
bool hiscore_between_games(void);

#endif
