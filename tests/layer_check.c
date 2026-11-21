/* tests/layer_check [VBLANKS [ddonpach.zip]]: the picture in planes (Cave68kSDK's video_draw_planes, video_compose:
   GRAPHICS > BY LAYER scales each on its own) put together at the game's size against video_draw's picture, every
   third frame of the test player's run from the power on (tests/player.h; 60,000 by default: 1-1 to 1-3). PASS when
   every one is the same, pixel for pixel; and the time each way a frame */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "cave/video.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned player_inputs(uint64_t frame)
{
#include "player.h"
}

static double now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
	const uint64_t vblanks = argc > 1 ? strtoull(argv[1], NULL, 10) : 60000;
	const char *const rom = argc > 2 ? argv[2] : "ddonpach.zip";
	static uint32_t drawn[VIDEO_WIDTH * VIDEO_HEIGHT], composed[VIDEO_WIDTH * VIDEO_HEIGHT];
	static uint32_t store[VIDEO_PLANES][VIDEO_WIDTH * VIDEO_HEIGHT];
	uint32_t *planes[VIDEO_PLANES];
	unsigned long checked = 0, differ = 0;
	double draw_ms = 0, planes_ms = 0;
	const char *problem;

	for (int plane = 0; plane < VIDEO_PLANES; plane++)
		planes[plane] = store[plane];
	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load(rom)) != NULL || (problem = program_start()) != NULL)
	{
		printf("%s\n", problem);
		return 1;
	}
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	while (cave_board.frame < vblanks)
	{
		program_run_frame();
		cave_sound_drop();
		video_latch_sprites();
		cave_board.in0 = (uint16_t)~player_inputs(cave_board.frame);
		if (cave_board.frame % 3u)
			continue;
		{
			const double t0 = now_ms();
			double t1, t2;
			uint32_t background;
			int count;

			video_draw(drawn);
			t1 = now_ms();
			count = video_draw_planes(planes, &background);
			video_compose((const uint32_t *const *)planes, count, background, VIDEO_WIDTH * VIDEO_HEIGHT, composed);
			t2 = now_ms();
			draw_ms += t1 - t0;
			planes_ms += t2 - t1;
		}
		checked++;
		if (memcmp(drawn, composed, sizeof(drawn)) && differ++ < 5)
			printf("VBLANK %" PRIu64 ": the planes put together differ\n", cave_board.frame);
	}
	printf("%lu frames: video_draw %.2f ms, the planes and put together %.2f ms a frame\n", checked,
		draw_ms / (double)checked, planes_ms / (double)checked);
	printf("%-60s %s\n", "the planes put together are video_draw's picture", differ ? "FAIL" : "PASS");
	return differ != 0;
}
