/* tests/training_check [STAGE]: the main menu's TRAINING (ports/sdl2/training.c, built in) run to each place, 1-1 to
   2-6, its start and its boss (or STAGE's, 0-11, alone): whether it gets there, in how many of the board's frames
   and seconds, and a picture of each two seconds after (training_SS_P.ppm). Run where ddonpach.zip is */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "cave/video.h"
#include "../ports/sdl2/training.c"

#include <stdlib.h>
#include <time.h>

static uint32_t frame_pixels[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];

static void save_picture(const char *name)
{
	FILE *const out = fopen(name, "wb");

	video_latch_sprites();
	video_draw(frame_pixels);
	video_rotated(frame_pixels, picture);
	fprintf(out, "P6\n%d %d\n255\n", VIDEO_HEIGHT, VIDEO_WIDTH);
	for (int at = 0; at < VIDEO_WIDTH * VIDEO_HEIGHT; at++)
	{
		fputc(picture[at] >> 16 & 255, out);
		fputc(picture[at] >> 8 & 255, out);
		fputc(picture[at] & 255, out);
	}
	fclose(out);
}

int main(int argc, char **argv)
{
	const char *problem;
	int failed = 0;

	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load("ddonpach.zip")) != NULL || (problem = program_start()) != NULL)
	{
		printf("%s\n", problem);
		return 1;
	}
	for (int stage = 0; stage < TRAINING_STAGES; stage++)
		for (int point = 0; point < TRAINING_POINTS; point++)
		{
			/* (the ships and styles in turn) */
			const training_setup setup = { stage, point, stage % 3, stage % 2 != 0, TRAINING_MAX_POWER, false };
			const uint64_t began = cave_board.frame;
			const clock_t clock_began = clock();

			if (argc > 1 && atoi(argv[1]) != stage)
				continue;
			problem = training_prepare(&setup);
			printf("%-18s %s, %llu frames, %.2f s\n", training_name(&setup), problem ? problem : "reached",
				(unsigned long long)(cave_board.frame - began), (double)(clock() - clock_began) / CLOCKS_PER_SEC);
			if (problem)
			{
				failed++;
				continue;
			}
			for (int frame = 0; frame < 115; frame++)
			{
				cave_board.in0 = cave_board.in1 = 0xFFFFu;
				program_run_frame();
				cave_sound_drop();
			}
			{
				char name[32];

				snprintf(name, sizeof(name), "training_%02d_%d.ppm", stage, point);
				save_picture(name);
			}
		}
	return failed != 0;
}
