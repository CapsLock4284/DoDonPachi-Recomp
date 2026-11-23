/**
 *    Name:        loop2_check.c
 *    Description: the second loop's qualification as the side panels read it (ports/sdl2/loop2.h) against the game's
 *                 own choice at 1-6's end: the test player (tests/player.h) with P1 INVINCIBLE from the power on to
 *                 1-6 (a save state there), then 1-6 played again for each case, the game's memory held at the case's
 *                 values through 1-6; the answer loop2_qualifies gave on 1-6's last frame, and whether the second
 *                 loop's 2-1 began. Then the second loop with 9 misses, no score and no hits through 2-6: 2-7 still
 *                 comes. And the hits' table in the World program (040038) as loop2.h has it
 *    Created:     22-11-2026
 *
 *    license:BSD-3-Clause
 *    Usage: loop2_check [ddonpach.zip]   PASS or FAIL each. About 2 minutes
 */
#include "cave/boards.h"
#include "cave/cheats.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "m68000/ops.h"

#include "../ports/sdl2/own_cheats.c"
#include "../ports/sdl2/loop2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned player_inputs(uint64_t frame)
{
#include "player.h"
}

static int failures;

static void verdict(const char *what, bool pass)
{
	printf("%-72s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

static void frame(void)
{
	program_run_frame();
	cave_sound_drop();
	cheats_apply();
	cave_board.in0 = (uint16_t)~player_inputs(cave_board.frame);
}

static void set_word(uint32_t address, unsigned value)
{
	*cave_work_ram_byte(address) = (uint8_t)(value >> 8);
	*cave_work_ram_byte(address + 1u) = (uint8_t)value;
}

/* a case: the values held through the stage (-1: the game's own) */
typedef struct
{
	const char *name;
	int misses, hits, score_high, bee_stages, credit;
	bool not_alone, expected;
} test_case;

static void hold(const test_case *c)
{
	if (c->misses >= 0)
		set_word(0x101948u, (unsigned)c->misses);
	if (c->hits >= 0)
		set_word(0x101842u, (unsigned)c->hits);
	if (c->score_high >= 0)
		set_word(0x10161Eu, (unsigned)c->score_high);
	if (c->bee_stages >= 0)
		set_word(0x1017C2u, (unsigned)c->bee_stages);
	if (c->credit >= 0)
		set_word(0x10174Eu, (unsigned)c->credit);
	if (c->not_alone)
		*cave_work_ram_byte(0x1017A9u) &= (uint8_t)~LOOP2_ALONE;
}

/* 1-6 from the state, the case held: loop2_qualifies on its last frame, and the game's choice */
static void run_case(const program_state *at_1_6, const test_case *c)
{
	bool predicted = false, second_loop = false;
	char what[96];

	program_state_load(at_1_6);
	for (int frames = 0; frames < 60000; frames++)
	{
		if (gm_word(GM_LOOP) == 0u)
		{
			hold(c);
			predicted = loop2_qualifies(0);
		}
		frame();
		if (gm_word(GM_LOOP) == 1u && gm_word(GM_STAGE) == 0u && gm_word(GM_MODE) == 7u)
		{
			second_loop = true;
			break;
		}
		if (gm_word(GM_LOOP) == 0u && gm_word(GM_STAGE) == 0u && frames > 1000)
			break;    /* (the game over: the test player's next game) */
	}
	snprintf(what, sizeof(what), "%s: %s (panel: %s)", c->name, second_loop ? "2-1" : "no second loop",
		predicted ? "yes" : "no");
	verdict(what, predicted == second_loop && second_loop == c->expected);
}

int main(int argc, char **argv)
{
	const char *const rom = argc > 1 ? argv[1] : "ddonpach.zip";
	static const test_case cases[] = {
		/* name                                   misses hits score bees credit 2P expected */
		{ "2 misses, nothing else",                   2,   0, 0x0100, 0, -1, false, true },
		{ "3 misses, nothing else",                   3,   0, 0x0100, 0, -1, false, false },
		{ "3 misses, MAX HIT 623 (type A)",           3, 623, 0x0100, 0, -1, false, false },
		{ "3 misses, MAX HIT 624 (type A)",           3, 624, 0x0100, 0, -1, false, true },
		{ "3 misses, score 49,99x,xxx",               3,   0, 0x0499, 0, -1, false, false },
		{ "3 misses, score 50,00x,xxx",               3,   0, 0x0500, 0, -1, false, true },
		{ "3 misses, 3 stages of all bees",           3,   0, 0x0100, 3, -1, false, false },
		{ "3 misses, 4 stages of all bees",           3,   0, 0x0100, 4, -1, false, true },
		{ "no misses, a continue",                    0,   0, 0x0100, 0, 0x0101, false, false },
		{ "no misses, the other player joined",       0,   0, 0x0100, 0, -1, true, false },
	};
	const char *problem;
	program_state *at_1_6 = NULL;

	own_cheats_add();
	cheats_load("no-cheat-file.xml");
	for (int cheat = 0; cheat < cheats_count(); cheat++)
		if (!strcmp(cheats_name(cheat), "P1 INVINCIBLE"))
			cheats_set(cheat, true);
	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load(rom)) != NULL || (problem = program_start()) != NULL)
	{
		printf("%s: %s\n", rom, problem);
		return 1;
	}
	for (int i = 0; i < 3; i++)
	{
		char what[64];

		snprintf(what, sizeof(what), "MAX HIT needed, ship %c: %u in the program", 'A' + i, loop2_hits_needed[i]);
		verdict(what, mem_read16(0x40038u + 2u * (unsigned)i) == loop2_hits_needed[i]);
	}
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	while (cave_board.frame < 400000u && !(gm_word(GM_LOOP) == 0u && gm_word(GM_STAGE) == 5u
			&& gm_word(GM_MODE) == 7u))
		frame();
	if (!(at_1_6 = program_state_save()))
	{
		printf("1-6 not reached\n");
		return 1;
	}
	for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
		run_case(at_1_6, &cases[c]);

	/* the second loop: 2-6 with 9 misses, no score and no hits; then 2-7 */
	{
		bool reached = false;

		program_state_load(at_1_6);
		while (cave_board.frame < 400000u && !(gm_word(GM_LOOP) == 1u && gm_word(GM_STAGE) == 6u
				&& gm_word(LOOP2_HIDDEN_STAGE) == 1u))
		{
			if (gm_word(GM_LOOP) == 1u && gm_word(GM_STAGE) == 5u)
			{
				const test_case worst = { "", 9, 0, 0x0001, 0, -1, false, false };

				hold(&worst);
			}
			frame();
		}
		reached = gm_word(GM_LOOP) == 1u && gm_word(GM_STAGE) == 6u;
		verdict("2-6 with 9 misses, no score, no hits: 2-7 (Hibachi's) next", reached);
	}
	program_state_free(at_1_6);
	printf(failures ? "%d FAILED\n" : "all PASS\n", failures);
	return failures != 0;
}
