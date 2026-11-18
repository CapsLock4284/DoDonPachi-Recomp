/* latency_check [VBLANK [left|shot]]: the input latency, in frames, of the board and of the window port's two
   INPUT TIMING settings. From the power on (tests/player.h, player 1 kept from harm as INVINCIBLE=1 does), two
   runs, the second holding the key from VBLANK on (no direction or shot in either for the 60 VBLANKs before), each
   run in a process of its own (a look-ahead's state must not reach the other run). The first picture that differs:
     the board (MAME the same): the picture after each VBLANK, drawn as its vblank interrupt returns (the sprites
       latched at VBLANK, the interrupt's scroll and palette), from the VBLANK whose interrupt first reads the key
       (the game reads IN0 in level_1_interrupt);
     the port: main.c's loop (read_keys, program_run_frame, the sprites latched, then the look-ahead's picture for
       FAST or the real frame's for ARCADE, shown), from the window frame that read the key.
   Measured 18-11-2026: the board 2 frames (the game moves the ship and fires a frame after it reads the key, and
   the board shows a frame's work after its next VBLANK), ARCADE 2 (the same), FAST 1 (the look-ahead: one less).
   Defaults: VBLANKs 9001, 12007, 20011, both keys. ddonpach.zip in the current folder */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "cave/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned player_inputs(uint64_t frame)
{
#include "player.h"
}

#define SPAN 300    /* (the pictures compared, from 100 before the press) */

enum { BOARD, FAST, ARCADE };
static const char *const mode_names[] = { "the board", "FAST", "ARCADE" };

static uint32_t pixels[VIDEO_WIDTH * VIDEO_HEIGHT];
static uint64_t drawn;
static int wanted;

static void after_vblank(void)
{
	uint64_t hash = 1469598103934665603ull;

	if (!wanted)
		return;
	video_draw(pixels);
	for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
	{
		hash ^= pixels[i];
		hash *= 1099511628211ull;
	}
	drawn = hash;
}

/* One run: the pictures (hashed) from 'first' on, as 'mode' shows them, into 'out' */
static void run(int mode, long press, unsigned key, int pressing, uint64_t *out)
{
	const long first = press - 100;

	cave_board_describe(&cave_board_ddonpach);
	if (rom_set_load("ddonpach.zip") || program_start())
		exit(1);
	program_set_slowdown(true);
	program_after_vblank = after_vblank;
	cave_board.in0 = cave_board.in1 = 0xFFFF;
	for (long it = 0; it < first + SPAN; it++)
	{
		/* main.c's read_keys: set as program_run_frame has returned at VBLANK it - 1 ('it' == cave_board.frame), read
		   by that VBLANK's interrupt as the next program_run_frame starts */
		unsigned in = player_inputs((uint64_t)it);

		if (it >= press - 60)
			in &= ~(CAVE_LEFT | CAVE_RIGHT | CAVE_UP | CAVE_DOWN | CAVE_BUTTON1 | CAVE_BUTTON2 | CAVE_BUTTON3);
		if (pressing && it >= press)
			in |= key;
		cave_board.in0 = (uint16_t)~in;
		if (cave_board.frame > 2500u)
			*cave_work_ram_byte(0x102CCAu) = 0x80u;
		wanted = mode != FAST;    /* (the board's and ARCADE's: the picture this frame's interrupt draws) */
		drawn = 0;
		program_run_frame();
		cave_sound_drop();
		video_latch_sprites();
		if (mode == FAST)
		{
			program_state *const state = program_state_save();

			wanted = 1;
			cave_sound_quiet = true;
			program_run_frame();
			cave_sound_quiet = false;
			wanted = 0;
			program_state_load(state);
			program_state_free(state);
		}
		/* the board: the picture drawn in this frame is the one after VBLANK it - 1, whose interrupt read the inputs
		   set at 'it - 1': indexed by that VBLANK */
		if (it >= first)
			out[it - first] = drawn;
	}
}

static long first_difference(int mode, long press, unsigned key)
{
	uint64_t pictures[2][SPAN];

	for (int pressing = 0; pressing < 2; pressing++)
	{
		int pipe_ends[2];
		pid_t child;

		if (pipe(pipe_ends))
			exit(1);
		child = fork();
		if (child == 0)
		{
			static uint64_t out[SPAN];

			close(pipe_ends[0]);
			run(mode, press, key, pressing, out);
			if (write(pipe_ends[1], out, sizeof(out)) != (ssize_t)sizeof(out))
				_exit(1);
			_exit(0);
		}
		close(pipe_ends[1]);
		if (read(pipe_ends[0], pictures[pressing], sizeof(pictures[pressing])) != (ssize_t)sizeof(pictures[pressing]))
			exit(1);
		close(pipe_ends[0]);
		waitpid(child, NULL, 0);
	}
	for (int i = 0; i < SPAN; i++)
		if (pictures[0][i] != pictures[1][i])
			return press - 100 + i;
	return -1;
}

int main(int argc, char **argv)
{
	static const long presses[] = { 9001, 12007, 20011 };
	const int count = argc > 1 ? 1 : 3;

	for (int p = 0; p < count; p++)
	{
		const long press = argc > 1 ? atol(argv[1]) : presses[p];

		for (int k = 0; k < 2; k++)
		{
			const char *const key_name = argc > 2 ? argv[2] : k ? "shot" : "left";
			const unsigned key = !strcmp(key_name, "shot") ? CAVE_BUTTON1 : CAVE_LEFT;

			for (int mode = BOARD; mode <= ARCADE; mode++)
			{
				const long shown = first_difference(mode, press, key);

				/* the board: from the VBLANK whose interrupt reads it (press - 1) to the VBLANK after which it
				   shows (the picture indexed by the frame 'it' that drew it: VBLANK it - 1); the port: window
				   frames, from the one whose read_keys took it */
				if (shown < 0)
					printf("%-9s %-5s pressed at %ld: no difference\n", mode_names[mode], key_name, press);
				else
					printf("%-9s %-5s pressed at %ld: %ld frame(s)\n", mode_names[mode], key_name, press,
						mode == BOARD ? (shown - 1) - (press - 1) : shown - press);
			}
			if (argc > 2)
				break;
		}
	}
	return 0;
}
