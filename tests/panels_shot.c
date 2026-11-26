/* tests/panels_shot VBLANKS EVERY [WIDTHS...]: the side panels (ports/sdl2/widgets.c, built in) beside the picture,
   as the screen shows them, played by tests/player.h (or another: -DPLAYER='"players/type_A_shot.h"') and kept from
   harm from VBLANK 2,500 as slowdown_check's INVINCIBLE (MORTAL=1: not); every EVERY VBLANKs a picture
   panels_VBLANK_WIDTH.ppm for each panel width given (120 156 251 by default: 16:10, 16:9 and 21:9 screens). The
   stages' best scores in panels_test.stages (started empty; KEEP_STAGES=1: kept from the last run). METER=1: the
   FPS meter on the right panel too, as F9 shows it with the panels on. GADGETS=K: the gadget panels (gadgets.h) instead,
   at K pixels a unit, the picture beside them as large (gadgets_VBLANK_WIDTH.ppm); HELP=1 with it: the right panel as
   the menu's help. Run where ddonpach.zip is, as slowdown_check */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "cave/video.h"
#define FILE_SAVE_SYNC    /* (the port's files written at once: no SDL here) */
#include "../ports/sdl2/file_save.c"
#include "../ports/sdl2/widgets.c"
#include "../ports/sdl2/game_watch.h"
#include "../ports/sdl2/achievements.h"
#include "../ports/sdl2/gadgets.h"    /* (built with it: tests/panels_shot.extra) */

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

/* the gadget panels at k beside the picture, as large */
static void save_gadgets(const char *name, int pw, int k)
{
	static uint32_t gl[WIDGET_MAX_W * 6 * WIDGET_H * 6], gr[WIDGET_MAX_W * 6 * WIDGET_H * 6];
	const int panel = pw * k, gap = WIDGET_GAP * k, w = VIDEO_HEIGHT * k + 2 * (panel + gap), h = WIDGET_H * k;
	FILE *const out = fopen(name, "wb");

	gadgets_draw(gl, gr, pw, k, NULL);
	if (getenv("HELP"))
		gadgets_draw_help(gr, pw, k, "SIDE PANELS", "CLASSIC: THE PANELS AS THEY WERE. GADGETS: THE BLUE GADGETS, SHARP AT "
			"THE SCREEN'S OWN RESOLUTION. OFF: NONE.");
	fprintf(out, "P6\n%d %d\n255\n", w, h);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
		{
			uint32_t c = 0x02060Fu;

			if (x < panel)
				c = gl[y * panel + x];
			else if (x >= panel + gap && x < panel + gap + VIDEO_HEIGHT * k)
				c = picture[(y / k) * VIDEO_HEIGHT + (x - panel - gap) / k];
			else if (x >= w - panel)
				c = gr[y * panel + x - (w - panel)];
			fputc(c >> 16 & 255, out);
			fputc(c >> 8 & 255, out);
			fputc(c & 255, out);
		}
	fclose(out);
}

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
	if (!getenv("KEEP_STAGES"))
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
		game_watch_frame(true);
		achievements_frame(true);
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

				if (getenv("METER"))
				{
					/* (the FPS meter on the right panel's foot, as F9 shows it with the panels: figures for the
					   picture) */
					const widgets_meter shown = { true, 56.6, 57.5, 57.5, 0, 3.2, 6.8 };

					widgets_set_meter(&shown);
				}
				if (getenv("GADGETS"))
				{
					snprintf(name, sizeof(name), "gadgets_%07llu_%d.ppm", (unsigned long long)cave_board.frame, widths[at]);
					save_gadgets(name, widths[at] < WIDGET_W ? WIDGET_W : widths[at] > WIDGET_MAX_W ? WIDGET_MAX_W
						: widths[at], atoi(getenv("GADGETS")));
					continue;
				}
				widgets_draw(left, right, widths[at]);
				snprintf(name, sizeof(name), "panels_%07llu_%d.ppm", (unsigned long long)cave_board.frame, widths[at]);
				save(name, widths[at] < WIDGET_W ? WIDGET_W : widths[at] > WIDGET_MAX_W ? WIDGET_MAX_W : widths[at]);
			}
		}
	}
	return 0;
}
