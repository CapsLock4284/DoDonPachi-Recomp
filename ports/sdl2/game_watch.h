/**
 *    Name:        game_watch.h
 *    Description: the arcade game watched from its memory (game_memory.h) after each frame: what happened in that
 *                 frame (a stage cleared, a miss, a bomb, a bee, ...) and the game so far, for the achievements and
 *                 the statistics (achievements.h, stats.h)
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    A game: from its ship select to the mode's next other value than playing or the continue's count down. Player 1
 *    is watched (player 2 for joining in: a stage cleared with both). A frame of slowdown is skipped (the game's
 *    memory is half way through its frame then): its events come with the next.
 */
#ifndef DDONPACH_GAME_WATCH_H
#define DDONPACH_GAME_WATCH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	/* this frame's events */
	bool begun;                 /* a game's ship select */
	bool ended;                 /* the game over (its last count down run out) */
	int cleared;                /* the stage cleared (0-11), or -1; and its misses, bombs, bees, both players in it */
	unsigned cleared_misses, cleared_bombs, cleared_bees;
	bool cleared_two_players;
	bool died, extended, bomb_used, continued, powered_up;
	bool hidden_extend;         /* the extend was the hidden 1UP item's (not the score's) */
	bool area7_reached, hibachi_met, hibachi_defeated;    /* 2-7 begun, its true last boss come, shot down */
	unsigned bees_taken;
	/* the game so far */
	bool on;                    /* a game being played, from its ship select */
	bool counts;                /* not training, a replay, a cheat or a save state, all through */
	bool playing;               /* the mode is play (not the select, nor a count down) */
	int stage;                  /* the stage being played (0-11), or -1 */
	unsigned stage_misses, stage_bombs, stage_bees;    /* in it */
	bool stage_two_players;     /* both played in it */
	unsigned loop1_misses, loop1_full_bees;    /* the first loop's misses, and its stages with all 13 bees */
	bool continued_ever;
	int ship;                   /* 0-2 */
	bool laser;
	uint32_t score;             /* player 1's (as the game counts it) */
	unsigned max_hit;           /* player 1's best combo */
	unsigned own_power, other_power, bombs, bomb_most;
	bool cleared_2_6;           /* 2-6 cleared in this game: the stage 6 of loop 1 is 2-7 from then */
	bool in_area7, hibachi;     /* in 2-7 now, and Hibachi come in it */
	uint32_t play_frames, life_frames;    /* frames of play, and since the last miss */
} game_watch_state;

extern game_watch_state game_watch;

/* After each frame of the game: 'counts' false while the game may not count */
void game_watch_frame(bool counts);

/* The board was reset: no game */
void game_watch_reset(void);

#endif
