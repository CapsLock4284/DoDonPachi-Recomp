/* tests/achievements_check [ddonpach.zip]: the achievements of the second loop's end and the hidden 1UP
   (ports/sdl2/achievements.c, on game_watch.c) checked on whole games of the test player (tests/player.h) from the
   power on, P1 INVINCIBLE on (the own cheat, as cheats_check), the game watched as an arcade game's:
     the run as it is: both loops, 2-7, Hibachi and the ending. HIDDEN STAGE as 2-7 begins (not at the pause between
       the loops, the same stage values after 1-6), HIBACHI at its WARNING, TRUE ENDING as the ending comes; the two
       score extends (6,000,000 and 20,000,000) are EXTRA LIFE, not HIDDEN EXTEND
     the run again with a life given in 1-3 as the 1UP item gives it (the lives one up, the next extend's score left
       as it is): HIDDEN EXTEND then
   PASS or FAIL each. Writes achievements_check.tmp where it runs (deleted after). About 90 s */
#include "cave/boards.h"
#include "cave/cheats.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"

#include "../ports/sdl2/own_cheats.c"
#include "../ports/sdl2/game_watch.c"
#include "../ports/sdl2/achievements.c"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define FILE_NAME "achievements_check.tmp"
#define VBLANKS   300000u

static unsigned player_inputs(uint64_t frame)
{
#include "player.h"
}

static int failures;

static void verdict(const char *what, bool pass)
{
	printf("%-66s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

static uint64_t earned_at[ACHIEVEMENTS];

/* A whole run from the power on; 'give_life': a life given as the 1UP item does, once, in 1-3 */
static void run(const char *rom, bool give_life)
{
	bool given = false;

	memset(earned_at, 0, sizeof(earned_at));
	memset(dates, 0, sizeof(dates));
	ships_cleared = styles_cleared = 0;
	queued = 0;
	remove(FILE_NAME);
	achievements_load(FILE_NAME);
	cave_board_describe(&cave_board_ddonpach);
	if (rom_set_load(rom) != NULL || program_start() != NULL)
	{
		printf("%s: not started\n", rom);
		exit(1);
	}
	game_watch_reset();
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	while (cave_board.frame < VBLANKS)
	{
		const char *name;

		program_run_frame();
		cave_sound_drop();
		cheats_apply();
		if (give_life && !given && gm_word(GM_MODE) == 7u && gm_area() == 2 && gm_word(0x10198Eu) == 200u
				&& !program_frame_lagged())
		{
			*cave_work_ram_byte(GM_LIVES) += 1u;    /* (the 1UP item's: func_04D178) */
			given = true;
		}
		game_watch_frame(true);
		achievements_frame(true);
		while ((name = achievements_next_shown()) != NULL)
			for (int index = 0; index < ACHIEVEMENTS; index++)
				if (!strcmp(list[index].name, name))
				{
					earned_at[index] = cave_board.frame;
					printf("  %-18s at VBLANK %7" PRIu64 ": loop %u, stage %u\n", name, cave_board.frame,
						gm_word(GM_LOOP) + 1u, gm_word(GM_STAGE) + 1u);
				}
		cave_board.in0 = (uint16_t)~player_inputs(cave_board.frame);
	}
}

/* The run in a child (each from the power on): its earned_at back through a pipe */
static void run_apart(const char *rom, bool give_life)
{
	int ends[2];
	pid_t child;

	fflush(stdout);
	if (pipe(ends))
		exit(1);
	if ((child = fork()) == 0)
	{
		close(ends[0]);
		run(rom, give_life);
		fflush(stdout);
		if (write(ends[1], earned_at, sizeof(earned_at)) != (ssize_t)sizeof(earned_at))
			_exit(2);
		_exit(0);
	}
	close(ends[1]);
	memset(earned_at, 0, sizeof(earned_at));
	if (read(ends[0], earned_at, sizeof(earned_at)) != (ssize_t)sizeof(earned_at))
		printf("  (the run gave no result)\n");
	close(ends[0]);
	waitpid(child, NULL, 0);
}

int main(int argc, char **argv)
{
	const char *const rom = argc > 1 ? argv[1] : "ddonpach.zip";

	own_cheats_add();
	cheats_load("no-cheat-file.xml");
	for (int cheat = 0; cheat < cheats_count(); cheat++)
		if (!strcmp(cheats_name(cheat), "P1 INVINCIBLE"))
			cheats_set(cheat, true);

	printf("the test player's run:\n");
	run_apart(rom, false);
	verdict("HIDDEN STAGE as 2-7 begins (2-6 cleared; not between the loops)", earned_at[A_AREA7] != 0
		&& earned_at[A_ALL] != 0 && earned_at[A_AREA7] >= earned_at[A_ALL] && earned_at[A_AREA7] > earned_at[A_LOOP2] && earned_at[A_LOOP2] != 0);
	verdict("HIBACHI after it, in 2-7", earned_at[A_HIBACHI] > earned_at[A_AREA7] + 2220u);
	verdict("TRUE ENDING after it", earned_at[A_HIBACHI_DOWN] > earned_at[A_HIBACHI]);
	verdict("the score's extends: EXTRA LIFE, not HIDDEN EXTEND", earned_at[A_EXTEND] != 0
		&& earned_at[A_HIDDEN_EXTEND] == 0);

	printf("the run again, a life given in 1-3 as the 1UP item gives it:\n");
	run_apart(rom, true);
	verdict("HIDDEN EXTEND in 1-3", earned_at[A_HIDDEN_EXTEND] != 0 && earned_at[A_HIDDEN_EXTEND] < earned_at[A_LOOP1]);
	remove(FILE_NAME);
	return failures != 0;
}
