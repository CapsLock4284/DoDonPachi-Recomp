/**
 *    Name:        loop2.h
 *    Description: the second loop's qualification, as the game decides it at 1-6's end, read from its memory for the
 *                 side panels (widgets.c): whether the player still gets the second loop, and how
 *    Version:     0.1.0 (draft)
 *    Created:     22-11-2026
 *
 *    license:BSD-3-Clause
 *    The game's own rule (World 040A00 on, Japanese the same at its own places; the ending's code after 1-6, where the
 *    loop is 1): a player gets the second loop when, as 1-6 ends, they fly ($102C8C / $102CCC +0 bit 15), played the
 *    game alone ($1017A8 bit 6: cleared for good when the other player joins), on their first credit ($10174E /
 *    $101752 = $0100: its low byte counts the continues) and with any one of
 *      $101948 / $10194A  the misses (a life lost; the continue starts it again) 2 or fewer
 *      $101842 / $101844  MAX HIT, the best hit combo, at least 624 (type A), 768 (B) or 816 (C): the table at 040038
 *                         (Japanese 03FCC0) by the ship's record +$1C ($102CA8 / $102CE8, 0-2). Not 270, 300 and 330
 *                         as some guides have it
 *      $10161E / $101622  the score (BCD, without its last digit) at least 50,000,000 ($05000000 as kept)
 *      $1017C2            the stages with all 13 bees (the 13th taken with no miss since the stage began: 08:16053),
 *                         4 or more
 *    Checked (tests/loop2_check): the game's choice against this one with each at its edge. The second loop has no
 *    continue (the count down ends the game) and nothing more to ask: 2-6 cleared, 2-7 and its true last boss Hibachi
 *    follow (the misses, the score and the hits there change nothing: checked the same way)
 */
#ifndef DDONPACH_LOOP2_H
#define DDONPACH_LOOP2_H

#include "game_memory.h"

#define LOOP2_MISSES      2u
#define LOOP2_SCORE       0x05000000u   /* (the score as kept: 50,000,000 shown) */
#define LOOP2_BEE_STAGES  4u
#define LOOP2_ALONE       0x40u         /* ($1017A8's) */
#define LOOP2_FIRST_CREDIT 0x0100u
#define LOOP2_HIDDEN_STAGE 0x1017C4u    /* 1 from 2-6's end: 2-7 (game_00 4880) */

static const unsigned loop2_hits_needed[3] = { 624u, 768u, 816u };

typedef enum
{
	LOOP2_OUT_CONTINUED,    /* a continue: no second loop */
	LOOP2_OUT_TWO_PLAYERS,  /* the other player joined */
	LOOP2_IN,               /* one of the four that can't be lost again (hits, score, bee stages) */
	LOOP2_ON_TRACK,         /* none of those yet; the misses still 2 or fewer */
	LOOP2_SHORT             /* none of the four now: the hits, the score or the bee stages can still do it */
} loop2_state;

typedef struct
{
	loop2_state state;
	unsigned misses, hits, hits_needed, bee_stages;
	uint32_t score;         /* (as kept: BCD, without the last digit) */
	const char *by;         /* LOOP2_IN: which ("HITS", "SCORE", "BEES") */
} loop2_status;

static inline loop2_status loop2_now(int player)
{
	const uint32_t step = 2u * (uint32_t)player;
	loop2_status s;
	const unsigned ship = gm_word(gm_record(player) + 0x1Cu);

	s.misses = gm_word(0x101948u + step);
	s.hits = gm_word(0x101842u + step);
	s.hits_needed = loop2_hits_needed[ship < 3u ? ship : 0u];
	s.score = (uint32_t)gm_word(0x10161Eu + 2u * step) << 16 | gm_word(0x10161Eu + 2u * step + 2u);
	s.bee_stages = gm_word(0x1017C2u);
	s.by = "";
	if (gm_word(0x10174Eu + 4u * (uint32_t)player) != LOOP2_FIRST_CREDIT)
		s.state = LOOP2_OUT_CONTINUED;
	else if (!(gm_byte(0x1017A9u) & LOOP2_ALONE))
		s.state = LOOP2_OUT_TWO_PLAYERS;
	else if (s.hits >= s.hits_needed || s.score >= LOOP2_SCORE || s.bee_stages >= LOOP2_BEE_STAGES)
	{
		s.state = LOOP2_IN;
		s.by = s.score >= LOOP2_SCORE ? "SCORE" : s.hits >= s.hits_needed ? "HITS" : "BEES";
	}
	else
		s.state = s.misses <= LOOP2_MISSES ? LOOP2_ON_TRACK : LOOP2_SHORT;
	return s;
}

/* the game's own answer, were 1-6 to end now */
static inline bool loop2_qualifies(int player)
{
	const loop2_state state = loop2_now(player).state;

	return gm_flying(player) && (state == LOOP2_IN || state == LOOP2_ON_TRACK);
}

#endif
