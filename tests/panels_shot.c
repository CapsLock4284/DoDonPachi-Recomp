/* tests/panels_shot VBLANKS EVERY [WIDTHS...]: the side panels (ports/sdl2/widgets.c, built in) beside the picture,
   as the screen shows them, played by tests/player.h (or another: -DPLAYER='"players/type_A_shot.h"') and kept from
   harm from VBLANK 2,500 as slowdown_check's INVINCIBLE (MORTAL=1: not); every EVERY VBLANKs a picture
   panels_VBLANK_WIDTH.ppm for each panel width given (120 156 251 by default: 16:10, 16:9 and 21:9 screens). The
   stages' best scores in panels_test.stages (started empty). Run where ddonpach.zip is, as slowdown_check */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "cave/video.h"
#include "../ports/sdl2/widgets.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *paths_folder(void)
{
	return "";
}

#ifndef PLAYER
#define PLAYER "player.h"
#endif

static unsigned player_inputs(uint64_t frame)
{
#include PLAYER
}

static uint32_t frame_pixels[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
static uint32_t left[WIDGET_MAX_W * WIDGET_H], right[WIDGET_MAX_W * WIDGET_H];

static void save(const char *name, int pw)
{
	const int w = VIDEO_HEIGHT + 2 * (pw + WIDGET_GAP), h = WIDGET_H;
	FILE *const out = fopen(name, "wb");

	fprintf(out, "P6\n%d %d\n255\n", w, h);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
		{
			uint32_t c = 0;

			if (x < pw)
				c = left[y * pw + x];
			else if (x >= pw + WIDGET_GAP && x < pw + WIDGET_GAP + VIDEO_HEIGHT)
				c = picture[y * VIDEO_HEIGHT + x - pw - WIDGET_GAP];
			else if (x >= w - pw)
				c = right[y * pw + x - (w - pw)];
			fputc(c >> 16 & 255, out);
			fputc(c >> 8 & 255, out);
			fputc(c & 255, out);
		}
	fclose(out);
}

int main(int argc, char **argv)
{
	const unsigned long long vblanks = argc > 1 ? strtoull(argv[1], NULL, 10) : 20000;
	const unsigned long long every = argc > 2 ? strtoull(argv[2], NULL, 10) : 5000;
	int widths[8] = { 120, 156, 251 }, width_count = 3;
	const char *problem;

	if (argc > 3)
		for (width_count = 0; width_count < 8 && 3 + width_count < argc; width_count++)
			widths[width_count] = atoi(argv[3 + width_count]);
	remove("panels_test.stages");
	widgets_open("panels_test.stages");
	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load("ddonpach.zip")) != NULL || (problem = program_start()) != NULL)
	{
		printf("%s\n", problem);
		return 1;
	}
	widgets_reset();
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	while (cave_board.frame < vblanks)
	{
		program_run_frame();
		cave_sound_drop();
		widgets_frame(true);
		cave_board.in0 = (uint16_t)~player_inputs(cave_board.frame);
		if (!getenv("MORTAL") && cave_board.frame > 2500u)
			*cave_work_ram_byte(0x102CCAu) = 0x80u;
		if (cave_board.frame % every == 0)
		{
			video_latch_sprites();
			video_draw(frame_pixels);
			video_rotated(frame_pixels, picture);
			for (int at = 0; at < width_count; at++)
			{
				char name[64];

				widgets_draw(left, right, widths[at]);
				snprintf(name, sizeof(name), "panels_%07llu_%d.ppm", (unsigned long long)cave_board.frame, widths[at]);
				save(name, widths[at] < WIDGET_W ? WIDGET_W : widths[at] > WIDGET_MAX_W ? WIDGET_MAX_W : widths[at]);
			}
		}
	}
	return 0;
}
