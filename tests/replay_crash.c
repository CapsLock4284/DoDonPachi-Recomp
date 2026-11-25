/* replay_crash FILE.inp [--rom PATH]: a run's inputs, as the window port writes them when it stops or crashes
   (ports/sdl2/crash_log.h: ddonpach_crash_DATE_TIME.inp), played again from the board's reset: the same EEPROM, the
   same slowdown setting, the same high score table put back after the boot (ports/sdl2/hiscore.c), each frame's
   inputs. The game is the same from them, so it stops where the run stopped (the runtime says where, as there);
   if it gets to the end, it says so and where the game was. ENDSHOT=FILE: the last picture (PPM) */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "cave/video.h"

#define FILE_SAVE_SYNC    /* (the port's files written at once: no SDL here) */
#include "../ports/sdl2/file_save.c"
#include "../ports/sdl2/hiscore.c"    /* (the port's, as it puts the table back) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *paths_folder(void)    /* (hiscore.c's default file: not used, the table is given) */
{
	return "";
}

static uint32_t get32(const unsigned char *bytes)
{
	return bytes[0] | bytes[1] << 8 | bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

int main(int argc, char **argv)
{
	const char *rom = "ddonpach.zip", *name = NULL;
	FILE *file;
	unsigned char header[16], eeprom[128], table[HISCORE_TABLE_SIZE];
	unsigned char *frames;
	uint32_t count;

	for (int at = 1; at < argc; at++)
		if (!strcmp(argv[at], "--rom") && at + 1 < argc)
			rom = argv[++at];
		else
			name = argv[at];
	if (!name || !(file = fopen(name, "rb")))
	{
		fprintf(stderr, "replay_crash FILE.inp [--rom PATH]\n");
		return 1;
	}
	if (fread(header, 1, 16, file) != 16 || memcmp(header, "DDPINP0", 7) || (header[7] != '1' && header[7] != '2') || fread(eeprom, 1, 128, file) != 128
			|| fread(table, 1, sizeof(table), file) != sizeof(table))
	{
		fprintf(stderr, "%s: not a run's inputs\n", name);
		return 1;
	}
	count = get32(header + 8);
	if (!(frames = malloc((size_t)count * 4u + 1u)) || fread(frames, 4, count, file) != count)
	{
		fprintf(stderr, "%s: %lu frames, fewer there\n", name, (unsigned long)count);
		return 1;
	}
	fclose(file);
	printf("%s: %lu frames (%.1f minutes), slowdown %s, version %d, high score table %s\n", name, (unsigned long)count,
		count / 57.55 / 60.0, header[12] ? "on" : "off", header[13], header[14] ? "put back" : "the game's own");
	cave_board_describe(&cave_board_ddonpach);
	program_set_vblank_lead(header[15] != 0);    /* (0 in a DDPINP01 file: from before the VBLANK came early) */
	if (rom_set_load(rom) || program_start())
		return 1;
	for (int word = 0; word < CAVE_EEPROM_WORDS; word++)
		cave_board.eeprom.words[word] = (uint16_t)(eeprom[word * 2] << 8 | eeprom[word * 2 + 1]);
	program_set_slowdown(header[12] != 0);
	hiscore_use_table(header[14] ? table : NULL);
	hiscore_reset();
	for (uint32_t frame = 0; frame < count; frame++)
	{
		cave_board.in0 = (uint16_t)(frames[frame * 4] | frames[frame * 4 + 1] << 8);
		cave_board.in1 = (uint16_t)(frames[frame * 4 + 2] | frames[frame * 4 + 3] << 8);
		program_run_frame();
		cave_sound_drop();
		video_latch_sprites();
		hiscore_frame(header[14] != 0, false);
	}
	printf("played to the end: board frame %llu, no stop\n", (unsigned long long)cave_board.frame);
	if (getenv("ENDSHOT"))
	{
		static uint32_t board[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
		FILE *out = fopen(getenv("ENDSHOT"), "wb");

		video_draw(board);
		video_rotated(board, picture);
		if (out)
		{
			fprintf(out, "P6 240 320 255\n");
			for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
			{
				fputc(picture[i] >> 16 & 255, out);
				fputc(picture[i] >> 8 & 255, out);
				fputc(picture[i] & 255, out);
			}
			fclose(out);
		}
	}
	return 0;
}
