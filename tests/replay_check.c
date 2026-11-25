/* tests/replay_check [VBLANKS]: the window port's replays (ports/sdl2/replay.c, on crash_log.c's recording, built
   in) checked: a run of tests/players/type_A_shot.h, its ship not kept from harm, recorded from the power on as the
   port records it (its games over one after another, each started from the ranking or the attract mode); each
   game's replay saved (replays_check/), then played back from a reset of the running board, as the port plays one,
   and the board's checks compared all the way, and the end's state. Run where ddonpach.zip is */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"

#include "../ports/sdl2/crash_log.c"
#include "../ports/sdl2/replay.c"

#include <stdlib.h>

static unsigned player_inputs(uint64_t frame)
{
#include "players/type_A_shot.h"
}

int main(int argc, char **argv)
{
	const uint64_t vblanks = argc > 1 ? strtoull(argv[1], NULL, 10) : 40000;
	const char *problem;
	char saved[8][64];
	uint64_t end_hash[8];
	int games = 0, failed = 0;

	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load("ddonpach.zip")) != NULL || (problem = program_start()) != NULL)
	{
		printf("%s\n", problem);
		return 1;
	}
	replay_folder("replays_check/");
	crash_log_reset(true, 0, NULL);
	replay_reset();
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	while (cave_board.frame < vblanks && games < 8)
	{
		cave_board.in0 = (uint16_t)~player_inputs(cave_board.frame);
		crash_log_frame();
		program_run_frame();
		cave_sound_drop();
		if (replay_after_frame())
		{
			const char *const name = replay_keep(true);

			if (!name)
			{
				printf("game %d: not saved\n", games + 1);
				return 1;
			}
			snprintf(saved[games], sizeof(saved[games]), "%s", name);
			end_hash[games] = cave_state_hash();
			printf("game %d over at board frame %llu: %s\n", games + 1, (unsigned long long)cave_board.frame, name);
			games++;
		}
	}
	for (int game = 0; game < games; game++)
	{
		replay_start start;
		uint16_t in0, in1;
		uint32_t frames = 0;
		bool in_step = true;

		if ((problem = replay_play(saved[game], &start)) != NULL)
		{
			printf("replay %d: %s\n", game + 1, problem);
			failed++;
			continue;
		}
		program_reset();
		memcpy(cave_board.eeprom.words, start.eeprom, sizeof(start.eeprom));
		program_set_slowdown(start.slowdown);
		program_set_vblank_lead(start.vblank_lead);
		while (replay_next(&in0, &in1))
		{
			cave_board.in0 = in0;
			cave_board.in1 = in1;
			program_run_frame();
			cave_sound_drop();
			frames++;
			if (!replay_after_playback_frame())
				in_step = false;
		}
		printf("replay %d (%s): %u frames from the reset, its game from frame %u: %s, the end %s\n", game + 1,
			saved[game], frames, start.game_begins, in_step ? "in step all the way" : "OUT OF STEP",
			cave_state_hash() == end_hash[game] ? "the same" : "DIFFERENT");
		failed += !in_step || cave_state_hash() != end_hash[game];
		replay_stop();
	}
	return failed != 0;
}
