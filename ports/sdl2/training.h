/**
 *    Name:        training.h
 *    Description: the main menu's TRAINING: the game started straight at a chosen place (a stage's start or its
 *                 boss, in either loop), with the ship, its style, power and bombs chosen, as a save state to go back
 *                 to
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's training (ports/sdl2/training.h), made again for DoDonPachi. The game is run there unseen and
 *    unheard, as fast as it goes: from the board's reset, a coin and Start, the ship and style chosen on the select
 *    screen; stage 1-1 played with the ship kept from harm ($102CCA held, as tests/slowdown_check's INVINCIBLE), not
 *    firing, to its end, where the stage and loop are set ($101978, $10197A) as it is cleared so that the game's next
 *    stage is the place's (2-1: 1-6's, so that the game goes through the loop's end into the second; 2-7, the hidden
 *    Area 7: 2-6's). A boss: from its WARNING, as the boss's music begins on the sound chip's voice 0 (the owner's
 *    reference, 20-11-2026: before, the scroll's end, up to 24 s into some fights). 2-7's boss Hachi: as it comes in,
 *    after its speech (38.6 s); its true last boss Hibachi: Hachi shot down on the way (the ship at MAX, firing from
 *    below its middle, about 5 minutes of the game's), then from Hibachi's WARNING. There the player's run is made a
 *    fresh one: score 0, 3 lives, the bombs and power chosen, the guard taken off.
 */
#ifndef DDONPACH_TRAINING_H
#define DDONPACH_TRAINING_H

#include <stdbool.h>

/* the places: a stage's start, its boss (from its warning), and in 2-7 (the second loop's hidden Area 7) the true
   last boss, Hibachi, after its boss Hachi (2-7 has no START: its start is a speech) */
enum { TRAINING_START, TRAINING_BOSS, TRAINING_TRUE_LAST, TRAINING_POINTS };

#define TRAINING_STAGES 13          /* 1-1 ... 1-6, 2-1 ... 2-6, 2-7 */
#define TRAINING_AREA_7 12          /* 2-7: Hachi, then Hibachi */
#define TRAINING_MAX_POWER 4        /* the power: 0-3 levels 1-4, 4 MAX */
#define TRAINING_POWER_ON_PACE 5    /* the power a player would have there: training_on_pace */

typedef struct
{
	int stage;                  /* 0-12 (loop * 6 + stage; 12 2-7) */
	int point;                  /* TRAINING_START, TRAINING_BOSS; TRAINING_TRUE_LAST (2-7's) */
	int ship;                   /* 0-2: types A, B, C */
	bool laser;                 /* the style: laser (true) or shot */
	int power;                  /* 0-4: levels 1-4, MAX (both the shot and the laser); TRAINING_POWER_ON_PACE */
	bool max_bombs;             /* the most the ship can carry (6), all full; else 3 */
} training_setup;

/* The game run to the place: NULL (the game is there, between frames: the port saves its state), or what went
   wrong. The game is reset first */
const char *training_prepare(const training_setup *setup);

/* The place a stage has for 'point' (2-7: its boss or the true last boss, no start; the others: no true last boss) */
int training_place(int stage, int point);

/* The place's name for the screen ("STAGE 2-3 BOSS", "STAGE 2-7 HIBACHI") */
const char *training_name(const training_setup *setup);

/* ON PACE: the power (0-4) a player has at the place from the power-ups on the way there, none missed */
int training_on_pace(int stage, int point);

/* Player 1's lives left (the one in play counted): a miss is when they go down (retry on miss) */
int training_lives(void);

#endif
