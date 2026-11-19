/**
 *    Name:        game_memory.h
 *    Description: DoDonPachi's memory as the achievements and the statistics read it (achievements.c, stats.c): the
 *                 places, and small readers of them
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    Found by watching whole games of the test players (TODO.md; widgets.h has the side panels' share):
 *      $1017A6  the game's mode: $B the ship select, 7 playing, 8 the continue's count down (4 the ranking after a
 *               game over, and the title; 0, 1, 2, 5, $C, $D the attract mode)
 *      $101978  the stage of the loop, 0-5 (6: between the loops, and the ending)    $10197A  the loop, 0/1
 *      $101965  player 1's lives, the one in play counted (a byte)
 *      $102C8C / $102CCC  the ships' records: +0 bit 15 in play; +$20 / +$22 the power of the style's own weapon and
 *               of the other (0-4, 4 MAX); +$24 the bombs (high byte) and the most it can carry (low byte, 3-6)
 *      $101754 / $101758  the ship (0-2: types A, B, C)    $101756 / $10175A  the style (0 shot, 1 laser)
 *      $10161E / $101622  the scores (BCD, without the last digit the game shows: the continues)
 *      $101842  player 1's best hit combo of the game (the table's MAX HIT)
 *      $103F8E  the bees taken since the stage began or the last miss (its rises are the bees taken)
 */
#ifndef DDONPACH_GAME_MEMORY_H
#define DDONPACH_GAME_MEMORY_H

#include "cave/machine.h"

#include <stdbool.h>
#include <stdint.h>

#define GM_MODE         0x1017A6u
#define GM_STAGE        0x101978u
#define GM_LOOP         0x10197Au
#define GM_LIVES        0x101965u
#define GM_RECORD_1     0x102C8Cu
#define GM_RECORD_STEP  0x40u
#define GM_OWN_POWER    0x20u       /* (in the record: $102CAC) */
#define GM_OTHER_POWER  0x22u
#define GM_BOMBS        0x24u       /* ($102CB0: the bombs, then the most) */
#define GM_SHIP_1       0x101754u
#define GM_STYLE_1      0x101756u
#define GM_SCORE_1      0x10161Eu
#define GM_MAX_HIT_1    0x101842u
#define GM_BEES         0x103F8Eu

static inline unsigned gm_byte(uint32_t address)
{
	return *cave_work_ram_byte(address);
}

static inline unsigned gm_word(uint32_t address)
{
	return gm_byte(address) << 8 | gm_byte(address + 1u);
}

static inline uint32_t gm_bcd(uint32_t address)
{
	uint32_t value = 0;

	for (uint32_t at = 0; at < 4u; at++)
	{
		const unsigned bcd = gm_byte(address + at);

		value = value * 100u + (bcd >> 4) * 10u + (bcd & 15u);
	}
	return value;
}

static inline uint32_t gm_record(int player)
{
	return GM_RECORD_1 + GM_RECORD_STEP * (uint32_t)player;
}

static inline bool gm_flying(int player)
{
	return (gm_word(gm_record(player)) & 0x8000u) != 0;
}

/* the stage being played, 0-11 (loop * 6 + stage), or -1 (between the loops, the ending) */
static inline int gm_area(void)
{
	const unsigned stage = gm_word(GM_STAGE), loop = gm_word(GM_LOOP);

	return stage < 6u && loop < 2u ? (int)(loop * 6u + stage) : -1;
}

#endif
