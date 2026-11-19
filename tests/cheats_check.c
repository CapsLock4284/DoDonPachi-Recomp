/**
 *    Name:        cheats_check.c
 *    Description: the port's own cheats (ports/sdl2/own_cheats.c) checked: the test player (tests/player.h) from the
 *                 power on with P1 INVINCIBLE on, as a player would have it from the CHEATS page (applied after every
 *                 frame, cave/cheats.h), for VBLANKS of the board's frames: each stage as it begins (the score
 *                 then), and the lives lost in the stages (the ending's count of the lives left, into the score, is
 *                 not one). Checked 18-11-2026: both loops, the true last boss and the ending, no life lost
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    Usage: cheats_check [VBLANKS [ddonpach.zip]]   (300,000: both loops for the test player, about 87 minutes of
 *    play). MORTAL=1: the cheat off (the lives the player loses without it, to compare)
 */
#include "cave/boards.h"
#include "cave/cheats.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"

#include "../ports/sdl2/own_cheats.c"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned player_inputs(uint64_t frame)
{
#include "player.h"
}

int main(int argc, char **argv)
{
	const uint64_t vblanks = argc > 1 ? strtoull(argv[1], NULL, 10) : 300000;
	const char *const rom = argc > 2 ? argv[2] : "ddonpach.zip";
	const char *problem;
	unsigned lives_before = 0, lost = 0, games = 0, furthest = 0;
	unsigned mode_before = 0;

	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load(rom)) != NULL || (problem = program_start()) != NULL)
	{
		printf("%s\n", problem);
		return 1;
	}
	own_cheats_add();
	cheats_load("no-cheat-file.xml");    /* (none: the own cheats alone) */
	for (int cheat = 0; cheat < cheats_count(); cheat++)
		if (!strcmp(cheats_name(cheat), "P1 INVINCIBLE") && !getenv("MORTAL"))
			cheats_set(cheat, true);
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	while (cave_board.frame < vblanks)
	{
		unsigned mode, lives, place;

		program_run_frame();
		cave_sound_drop();
		cheats_apply();
		cave_board.in0 = (uint16_t)~player_inputs(cave_board.frame);
		mode = *(uint16_t *)cave_work_ram_at(GM_MODE, 2);
		lives = *cave_work_ram_byte(GM_LIVES);
		if (mode == 0xBu && mode_before != 0xBu)
			games++;
		/* (a life gone in a stage: not at the ending, which counts the lives left into the score, 10,000,000 each) */
		if (mode == 7u && mode_before == 7u && lives < lives_before && *(uint16_t *)cave_work_ram_at(GM_STAGE, 2) < 6u)
		{
			lost++;
			printf("a life gone at VBLANK %" PRIu64 ": loop %u, stage %u (score %08X0)\n", cave_board.frame,
				*(uint16_t *)cave_work_ram_at(GM_LOOP, 2) + 1u, *(uint16_t *)cave_work_ram_at(GM_STAGE, 2) + 1u,
				(unsigned)(*(uint16_t *)cave_work_ram_at(GM_SCORE_1, 2) << 16
					| *(uint16_t *)cave_work_ram_at(GM_SCORE_1 + 2u, 2)));
		}
		/* (the stage, 0-5, then 6 between the loops and at the ending; the loop 0 or 1, then 2 at the ending) */
		place = *(uint16_t *)cave_work_ram_at(GM_LOOP, 2) * 8u + *(uint16_t *)cave_work_ram_at(GM_STAGE, 2);
		if (mode == 7u && place != furthest)
		{
			furthest = place;
			printf("loop %u, stage %u at VBLANK %" PRIu64 " (score %08X0)\n", furthest / 8u + 1u, furthest % 8u + 1u,
				cave_board.frame, (unsigned)(*(uint16_t *)cave_work_ram_at(GM_SCORE_1, 2) << 16
					| *(uint16_t *)cave_work_ram_at(GM_SCORE_1 + 2u, 2)));
			fflush(stdout);
		}
		lives_before = lives;
		mode_before = mode;
	}
	printf("vblanks %" PRIu64 ": games begun %u, lives lost %u, last place: loop %u, stage %u\n", vblanks, games,
		lost, furthest / 8u + 1u, furthest % 8u + 1u);
	return lost != 0;
}
