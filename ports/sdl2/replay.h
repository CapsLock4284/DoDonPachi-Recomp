/**
 *    Name:        replay.h
 *    Description: replays: every arcade game recorded as it is played, saved as a file (replays/ beside the program)
 *                 when the player says yes at its end, listed and played back from the main menu's REPLAYS
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's replays (ports/sdl2/replay.h), made again for DoDonPachi on the crash log's recording
 *    (crash_log.h): the game is the same from the board's reset with the same EEPROM, slowdown setting, high score
 *    table put back after the boot, and the same inputs each frame, and the crash log keeps all of those since the
 *    reset. A replay is that run up to its game's end, with the frame its game began (its ship select: playback runs
 *    the frames before it at once, unseen) and a check of the board every 10 seconds (cave_state_hash) to tell a
 *    replay gone out of step. A run with a cheat, a loaded save state or training in it is not recorded.
 *    The game's mode ($1017A6): $B the ship select, 7 playing, 8 the continue's count down; a game is from its ship
 *    select to the mode's next other value (the ranking, the attract mode).
 */
#ifndef DDONPACH_REPLAY_H
#define DDONPACH_REPLAY_H

#include <stdbool.h>
#include <stdint.h>

/* The folder replays are saved in and read from (made when the first is saved; ends in its separator) */
void replay_folder(const char *path);

/* ---- recording ---- */

/* The board was reset (the crash log's recording starts again with it) */
void replay_reset(void);

/* After each frame the port records (not a replay's): the board's checks, a game begun or ended. True: a game just
   ended, its replay waiting for the player's yes or no (replay_keep) */
bool replay_after_frame(void);

/* A game's replay waits for the answer; then saved (true: the file's name, or NULL if it could not be) or let go */
bool replay_waiting(void);
const char *replay_keep(bool keep);

/* ---- the list ---- */

typedef struct
{
	char file[256];             /* its name in the folder */
	char date[20];              /* "2026-11-18 18:40" */
	uint32_t score;             /* player 1's best (as the game counts it: its last digit, the continues, left off) */
	int stage;                  /* the furthest stage reached, 0-11 (1-1 ... 2-6) */
	int ship;                   /* 0-2: types A, B, C */
	bool laser;                 /* the style */
	bool all_clear;             /* 2-6 cleared */
	uint32_t frames;            /* the game's frames, from its ship select */
} replay_info;

/* The replays in the folder, newest first, at most 'most': how many */
int replay_list(replay_info *out, int most);

bool replay_delete(const char *file);

/* ---- playback ---- */

/* The replay loaded: what its run starts with (the port resets the board, sets the EEPROM and the rest, and
   replay_next gives the frames from the reset on). NULL, or what is wrong with it */
typedef struct
{
	uint16_t eeprom[64];
	bool slowdown, has_table;
	bool vblank_lead;           /* program_set_vblank_lead for it: false in a replay from before 25-11-2026 (format 1) */
	uint8_t table[100];
	uint32_t game_begins;       /* its game's first frame: the frames before it are run unseen */
} replay_start;

const char *replay_play(const char *file, replay_start *start);

bool replay_playing(void);

/* Before each frame of playback: its inputs (IN0, IN1, active low). False: the replay is over */
bool replay_next(uint16_t *in0, uint16_t *in1);

/* After each frame of playback: its check (false: out of step with the recording, the first time it is seen) */
bool replay_after_playback_frame(void);

/* The frames played so far, and in all */
uint32_t replay_position(void);
uint32_t replay_length(void);

void replay_stop(void);

#endif
