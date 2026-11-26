/**
 *    Name:        playtest.h
 *    Description: --playtest: the game played through by itself, as fast as the PC runs it, player 1 kept from
 *                 harm, to see a build go all the way (both loops, 2-7, Hibachi, the ending) after a change
 *    Version:     0.1.0 (draft)
 *    Created:     25-11-2026
 *
 *    license:BSD-3-Clause
 *    ddonpach --playtest [all | A-shot | A-laser | B-shot | B-laser | C-shot | C-laser]   (all: the default, each in
 *    turn). The board from its power on with the ROM set's own EEPROM (the arcade's settings: the player's EEPROM,
 *    high scores, achievements, statistics, replays and settings neither read nor written), the arcade's slowdown on,
 *    no sound. A coin and Start, the ship and style chosen on the select screen (training.c's presses), then the test
 *    players' way of playing kept near the middle (tests/players/: button C's auto-fire, a short sweep, the laser held
 *    now and then, a bomb every 900 frames), and at a boss (from its WARNING, the boss's music) and Hibachi the laser
 *    held, the ship still; player 1's time without harm held ($102CCA at $80, as tests/slowdown_check's INVINCIBLE=1)
 *    and its weapons at MAX power (the bosses down within their time). Each stage's start is noted with the game's
 *    time; a run passes when the ending has come (the loop at 2, after 2-7's Hibachi, its WARNING seen) and the game
 *    has gone back to its title. It fails when the ship was lost (the continue's count), the game ended before the
 *    ending, the ending came without Hibachi, the ship chosen was not the one asked for, or a stage went on for 35
 *    minutes of the game's time (2-7, Hachi and Hibachi, takes this player up to about 25). The report:
 *    ddonpach_playtest.txt beside the program, the window's title as it goes, and a message at the end (the exit
 *    code: 0 all passed, 1 not). A crash ends it as any crash does (ddonpach.log), the report then saying where the
 *    game was. Checked 25-11-2026: all six to the ending, 1:00-1:18 of the game's time each, about 40 times the
 *    arcade's speed on Claude's workspace
 */
#ifndef DDONPACH_PLAYTEST_H
#define DDONPACH_PLAYTEST_H

#include <stdbool.h>

enum { PLAYTEST_GOING, PLAYTEST_PASSED, PLAYTEST_FAILED };

/* The runs asked for ("all", or a ship and style: "B-laser"; case and the dash optional): false if not one */
bool playtest_choose(const char *which);

/* Its start: the report's file ('path') and the game's version (0 World, 1 Japan), for the report */
void playtest_begin(const char *path, int version);

/* Player 1's inputs for the next frame (the board's bits, 1 = held: the port puts them in IN0 inverted) */
unsigned playtest_inputs(void);

/* After each frame: the guard held, the progress noted. PLAYTEST_GOING, or the end: PLAYTEST_PASSED (every run) or
   PLAYTEST_FAILED (the first that failed, and the report written) */
int playtest_frame(void);

/* What it is doing, for the window's title: "B-laser (4 of 6)  2-3  41:10" */
const char *playtest_status(void);

/* The end: the report written (with the time it took, in seconds), and its text, to show */
const char *playtest_finish(double seconds, bool stopped_by_player);

/* Its last line, after playtest_finish: "ALL PASSED: 1:49:54 of the game's time in 144 seconds ..." */
const char *playtest_summary(void);

#endif
