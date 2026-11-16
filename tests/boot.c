/* boot [FRAMES] [ROM]: DoDonPachi's program from the board's reset, headless: FRAMES frames (default 1200), a coin
   and Start in the middle, the picture saved now and then (boot_NNNNN.ppm, as the cabinet shows it), and what the board
   saw (unmapped reads and writes, the frames of slowdown, the work RAM's hash) */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "cave/video.h"

#include <stdio.h>
#include <stdlib.h>

static void save_ppm(const char *path, const uint32_t *pixels, int w, int h)
{
	FILE *const file = fopen(path, "wb");

	if (!file)
		return;
	fprintf(file, "P6 %d %d 255\n", w, h);
	for (int i = 0; i < w * h; i++)
	{
		fputc(pixels[i] >> 16 & 255, file);
		fputc(pixels[i] >> 8 & 255, file);
		fputc(pixels[i] & 255, file);
	}
	fclose(file);
}

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 1200;
	const char *const rom = argc > 2 ? argv[2] : "rom/ddonpach.zip";
	static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
	const char *problem;

	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load(rom)) != NULL || (problem = program_start()) != NULL)
	{
		printf("%s\n", problem);
		return 1;
	}
	for (long f = 0; f < frames; f++)
	{
		/* (active low: a coin at 600, Start at 700) */
		unsigned held = f >= 600 && f < 606 ? 0x0100u : f >= 700 && f < 706 ? 0x0080u : 0u;

		cave_board.in0 = (uint16_t)~held;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		if ((f + 1) % 100 == 0)
		{
			char name[32];

			video_latch_sprites();
			video_draw(frame);
			video_rotated(frame, picture);
			snprintf(name, sizeof(name), "boot_%05ld.ppm", f + 1);
			save_ppm(name, picture, VIDEO_HEIGHT, VIDEO_WIDTH);
			printf("frame %5ld: hash %016llx, unmapped reads %u (last %06X), writes %u (last %06X)\n", f + 1,
				(unsigned long long)cave_state_hash(), cave_board.unmapped_reads, cave_board.last_unmapped_read,
				cave_board.unmapped_writes, cave_board.last_unmapped_write);
		}
	}
	return 0;
}
