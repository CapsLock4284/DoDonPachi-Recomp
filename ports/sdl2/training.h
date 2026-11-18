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
 *    stage is the place's (2-1: 1-6's, so that the game goes through the loop's end into the second). A boss: on to
 *    where the stage's scroll ends and its boss comes ($101996, the next scroll stop, $FFFF there). There the
 *    player's run is made a fresh one: score 0, 3 lives, the bombs and power chosen, the guard taken off.
 */
#ifndef DDONPACH_TRAINING_H
#define DDONPACH_TRAINING_H

#include <stdbool.h>

enum { TRAINING_START, TRAINING_BOSS, TRAINING_POINTS };

#define TRAINING_STAGES 12          /* 1-1 ... 1-6, 2-1 ... 2-6 */
#define TRAINING_MAX_POWER 4        /* the power: 0-3 levels 1-4, 4 MAX */

typedef struct
{
	int stage;                  /* 0-11 (loop * 6 + stage) */
	int point;                  /* TRAINING_START, TRAINING_BOSS */
	int ship;                   /* 0-2: types A, B, C */
	bool laser;                 /* the style: laser (true) or shot */
	int power;                  /* 0-4: levels 1-4, MAX (both the shot and the laser) */
	bool max_bombs;             /* the most the ship can carry (6), all full; else 3 */
} training_setup;

/* The game run to the place: NULL (the game is there, between frames: the port saves its state), or what went
   wrong. The game is reset first */
const char *training_prepare(const training_setup *setup);

/* The place's name for the screen ("STAGE 2-3 BOSS") */
const char *training_name(const training_setup *setup);

/* Player 1's lives left (the one in play counted): a miss is when they go down (retry on miss) */
int training_lives(void);

#endif
