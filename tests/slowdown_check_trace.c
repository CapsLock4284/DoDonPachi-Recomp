/* slowdown_check VBLANKS: the port (its timing: the arcade's slowdown) from the power on, played by tests/player.h
   (the same inputs as tools/cref/cref_timed --play, by the VBLANK alone), for VBLANKS of the board's frames. Writes a
   line per frame of the game as it comes to its wait, "game_frame vblank hash" (slowdown_waits.txt), as cref_timed
   --waits writes the original's on MAME's 68000, by the same rule; and counts the VBLANKs lagged (no frame of the game
   since the last one: slowdown). Compare with tests/slowdown_compare.py:
     cref_timed --rom ddonpach.zip --frames 18000 --play --waits mame_waits.txt
     slowdown_check 18000
     python3 tests/slowdown_compare.py mame_waits.txt slowdown_waits.txt
   DUMPFRAME=N: the board's memories at game frame N's wait in port_dump.bin (cref_timed's: mame_dump.bin) */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define READY 0x10120Cu    /* bit 0: the frame made ready for the interrupt (wait_for_frame) */

static unsigned player_inputs(uint64_t frame)
{
#include "player.h"
}

static FILE *waits;

/* the VBLANKs come by the start of the instruction the program is at (every 278,016 cycles from the power on), as
   cref_timed counts them: the runtime has a VBLANK that comes part way into an instruction come at the instruction's
   end, so its count (cave_board.frame) is one short there until then */
extern uint64_t m68k_time;

static uint64_t vblanks_by_now(void)
{
	return m68k_time / ((uint64_t)CAVE_CPU_CLOCK * CAVE_FRAME_USEC / 1000000u);
}
static uint64_t game_frames;
static bool arrived;

#include "game/support.h"    /* (game_trace_file: TRACE_FRAME=N, a traced build: the blocks of game frame N's work) */

static void at_wait(void)
{
	/* (a wait the program has made ready, once: cref_timed's rule) */
	if (arrived || !(*(uint16_t *)cave_work_ram_at(READY, 2) & 1u))
		return;
	arrived = true;
	game_frames++;
	if (getenv("TRACE_FRAME"))
	{
		const uint64_t traced = strtoull(getenv("TRACE_FRAME"), NULL, 10);
		if (game_frames == traced - 1u)
			game_trace_file = fopen("port_trace.txt", "w");
		else if (game_frames == traced && game_trace_file)
		{
			fclose(game_trace_file);
			game_trace_file = NULL;
		}
	}
	if (getenv("DUMPFRAME") && game_frames == strtoull(getenv("DUMPFRAME"), NULL, 10))
		cave_dump("port_dump.bin");
	if (waits)
		fprintf(waits, "%" PRIu64 " %" PRIu64 " %016" PRIx64 "\n", game_frames, vblanks_by_now(), cave_state_hash());
}

int main(int argc, char **argv)
{
	const uint64_t vblanks = argc > 1 ? strtoull(argv[1], NULL, 10) : 18000;
	const char *const rom = argc > 2 ? argv[2] : "ddonpach.zip";
	const char *problem;
	uint64_t lagged = 0;

	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load(rom)) != NULL || (problem = program_start()) != NULL)
	{
		printf("%s\n", problem);
		return 1;
	}
	waits = fopen("slowdown_waits.txt", "w");
	program_at_wait = at_wait;
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	while (cave_board.frame < vblanks)
	{
		program_run_frame();
		cave_sound_drop();
		if (!arrived && cave_board.frame > 1u)
			lagged++;
		arrived = false;
		cave_board.in0 = (uint16_t)~player_inputs(cave_board.frame);
	}
	if (waits)
		fclose(waits);
	printf("vblanks %" PRIu64 ", game frames %" PRIu64 ", lagged %" PRIu64 "\n", vblanks, game_frames, lagged);
	return 0;
}
