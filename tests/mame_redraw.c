/**
 *    Name:        mame_redraw.c
 *    Description: the real MAME's ddonpach pictures, all through the game, against the port's drawing of the same
 *                 moments: the board's video memories MAME kept (tools/mame/ddp_check.lua 0.5.0's video_N.bin) put
 *                 in the port's board and drawn as the port draws, beside MAME's own picture of them (screen_N.bin)
 *    Created:     26-11-2026
 *
 *    license:BSD-3-Clause
 *    Usage: mame_redraw DIR [ddonpach.zip]   (DIR: ddp_check.lua's folder)
 *    MAKE=1: writes DIR from the port itself instead (video_N.bin and screen_N.bin as ddp_check.lua would, every
 *    DDP_VIDEO_EVERY VBLANKs and the one before, to DDP_FRAMES), to check this tool: drawn again, all the same.
 *    Unlike tests/mame_compare, the two games needn't stay together: each picture is drawn from MAME's own memories,
 *    so it compares MAME's drawing with the port's at every stage the run reaches (the sprites, the layers, their
 *    scrolling, the palette, the priorities), not the games' timing. MAME keeps each picture at its frame notifier,
 *    a moment after the VBLANK whose memories it keeps, and draws the screen a moment after that interrupt: so each
 *    point is drawn from VBLANK N's memories and from N-1's (the sprites from one, the rest from either), and the one
 *    nearest MAME's picture N is the one compared (each point's line says which). The sprites are also drawn from
 *    the other of the game's two sprite banks ($800008's low bit): MAME's memories are kept after its interrupt has
 *    begun, and the game turns the bank over there, so MAME's picture of the frame before is the other bank's
 *    (found on the owner's first run: the same at every point then). Writes the first pictures that
 *    differ (MAME_REDRAW_KEEP of them, 5) as redraw_mame_N.ppm, redraw_port_N.ppm and redraw_diff_N.ppm (white where
 *    they differ) where it runs.
 */
#include "cave/board.h"
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "cave/video.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static unsigned player_inputs(uint64_t frame)
{
#include "player.h"
}

/* ---- video_N.bin: the board's video memories, as the 68000 reads them (big endian) ---- */

static const char magic[8] = { 'D', 'D', 'P', 'V', 'I', 'D', '0', '1' };

static const struct
{
	uint32_t address, size;     /* 0: the video registers (MAME's own copy: not read through the 68000) */
} parts[] = {
	{ 0x400000u, 0x10000u },    /* the sprites */
	{ 0x500000u, 0x8000u },     /* layer 0 */
	{ 0x600000u, 0x8000u },     /* layer 1 */
	{ 0x700000u, 0x10000u },    /* layer 2 (8x8: its RAM mirrored over the 64 KB) */
	{ 0x900000u, 6u },          /* the layers' registers */
	{ 0xA00000u, 6u },
	{ 0xB00000u, 6u },
	{ 0xC00000u, 0x10000u },    /* the palette */
	{ 0u, 0x80u },              /* the video registers */
	{ 0x100000u, 0x10000u },    /* the work RAM */
};
#define PARTS ((int)(sizeof(parts) / sizeof(parts[0])))
#define VIDEO_REGS_PART 8
#define WORK_RAM_PART   9

static size_t video_size(void)
{
	size_t size = sizeof(magic);

	for (int part = 0; part < PARTS; part++)
		size += parts[part].size;
	return size;
}

static uint8_t *read_file(const char *path, size_t *size)
{
	FILE *const file = fopen(path, "rb");
	uint8_t *data;
	long length;

	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = malloc(length > 0 ? (size_t)length : 1u);
	*size = fread(data, 1, (size_t)length, file);
	fclose(file);
	return data;
}

static void write_ppm(const char *path, const uint32_t *pixels, int width, int height)
{
	FILE *const file = fopen(path, "wb");

	if (!file)
		return;
	fprintf(file, "P6 %d %d 255\n", width, height);
	for (int i = 0; i < width * height; i++)
	{
		fputc(pixels[i] >> 16 & 255, file);
		fputc(pixels[i] >> 8 & 255, file);
		fputc(pixels[i] & 255, file);
	}
	fclose(file);
}

/* a part of a video_N.bin into the board: through the 68000's writes (the board's own layout of each memory, its
   mirrors), the video registers straight in */
static void put_part(const uint8_t *data, int part)
{
	const uint8_t *at = data + sizeof(magic);

	for (int before = 0; before < part; before++)
		at += parts[before].size;
	if (part == VIDEO_REGS_PART)
	{
		for (uint32_t word = 0; word < parts[part].size / 2u && word < CAVE_VIDEO_REG_WORDS; word++)
			cave_board.video_regs[word] = (uint16_t)(at[2u * word] << 8 | at[2u * word + 1u]);
		return;
	}
	for (uint32_t offset = 0; offset + 1u < parts[part].size; offset += 2u)
		cave_write16(parts[part].address + offset, (uint16_t)(at[offset] << 8 | at[offset + 1u]), 0xFFFFu);
}

/* the picture drawn from these memories: the sprites latched from 'sprites' (from the other of the game's two
   sprite banks when 'other_bank'), the rest from 'rest' */
static void draw_from(const uint8_t *sprites, const uint8_t *rest, bool other_bank, uint32_t *picture)
{
	for (int part = 0; part < PARTS; part++)
		if (part != WORK_RAM_PART)
			put_part(sprites, part);
	if (other_bank)
		cave_board.video_regs[4] ^= 1u;
	video_latch_sprites();
	for (int part = 0; part < PARTS; part++)
		if (part != WORK_RAM_PART)
			put_part(rest, part);
	video_draw(picture);
}

/* MAME's picture N, as 0x00RRGGBB (false: none, or not the port's size) */
static bool mame_picture(const char *dir, unsigned n, uint32_t *picture)
{
	char path[1024];
	size_t size = 0;
	uint8_t *data;

	snprintf(path, sizeof(path), "%s/screen_%u.bin", dir, n);
	if (!(data = read_file(path, &size)))
		return false;
	if (size < (size_t)VIDEO_WIDTH * VIDEO_HEIGHT * 4u)
	{
		free(data);
		return false;
	}
	for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
		picture[i] = (uint32_t)data[4 * i] | (uint32_t)data[4 * i + 1] << 8 | (uint32_t)data[4 * i + 2] << 16;
	free(data);
	return true;
}

static unsigned differing(const uint32_t *a, const uint32_t *b)
{
	unsigned count = 0;

	for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
		count += (a[i] & 0xFFFFFFu) != (b[i] & 0xFFFFFFu);
	return count;
}

static bool start(const char *rom)
{
	const char *problem;

	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load(rom)) != NULL || (problem = program_start()) != NULL)
	{
		printf("%s: %s\n", rom, problem);
		return false;
	}
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	return true;
}

/* ---- MAKE=1: the folder written from the port, as ddp_check.lua writes it ---- */

static void write_video(const char *dir, uint64_t n)
{
	static uint32_t picture[VIDEO_WIDTH * VIDEO_HEIGHT];
	char path[1024];
	FILE *file;

	snprintf(path, sizeof(path), "%s/video_%llu.bin", dir, (unsigned long long)n);
	if (!(file = fopen(path, "wb")))
		return;
	fwrite(magic, 1, sizeof(magic), file);
	for (int part = 0; part < PARTS; part++)
	{
		if (part == VIDEO_REGS_PART)
			for (uint32_t word = 0; word < parts[part].size / 2u; word++)
			{
				/* (the sprite bank turned over, as MAME's are kept: after its interrupt has turned it) */
				const uint16_t value = word >= CAVE_VIDEO_REG_WORDS ? 0u
					: word == 4u ? cave_board.video_regs[word] ^ 1u : cave_board.video_regs[word];

				fputc(value >> 8, file);
				fputc(value & 255, file);
			}
		else
			for (uint32_t offset = 0; offset + 1u < parts[part].size; offset += 2u)
			{
				/* (the 68000's reads, but not the interrupt's cause: none of these are its addresses) */
				const uint16_t value = cave_read16(parts[part].address + offset, 0xFFFFu);

				fputc(value >> 8, file);
				fputc(value & 255, file);
			}
	}
	fclose(file);
	/* its picture: drawn from these memories now */
	video_latch_sprites();
	video_draw(picture);
	snprintf(path, sizeof(path), "%s/screen_%llu.bin", dir, (unsigned long long)n);
	if ((file = fopen(path, "wb")) != NULL)
	{
		for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
		{
			fputc(picture[i] & 255, file);
			fputc(picture[i] >> 8 & 255, file);
			fputc(picture[i] >> 16 & 255, file);
			fputc(255, file);
		}
		fclose(file);
	}
	snprintf(path, sizeof(path), "%s/screen_size.txt", dir);
	if ((file = fopen(path, "w")) != NULL)
	{
		fprintf(file, "%d %d\n", VIDEO_WIDTH, VIDEO_HEIGHT);
		fclose(file);
	}
}

static int make(const char *dir, const char *rom)
{
	const uint64_t frames = getenv("DDP_FRAMES") ? strtoull(getenv("DDP_FRAMES"), NULL, 10) : 30000u;
	const uint64_t every = getenv("DDP_VIDEO_EVERY") ? strtoull(getenv("DDP_VIDEO_EVERY"), NULL, 10) : 2500u;
	unsigned written = 0;

	if (!start(rom))
		return 1;
	for (uint64_t vblank = 1; vblank <= frames; vblank++)
	{
		cave_board.in0 = (uint16_t)~player_inputs(vblank);
		program_run_frame();
		cave_sound_drop();
		if (vblank > 2500u)
			*cave_work_ram_byte(0x102CCAu) = 0x80u;
		if (every && (vblank % every == 0 || vblank % every == every - 1u))
		{
			write_video(dir, vblank);
			written++;
		}
	}
	printf("%u video states written in %s, %llu VBLANKs\n", written, dir, (unsigned long long)frames);
	return 0;
}

/* ---- the comparison ---- */

static int by_number(const void *a, const void *b)
{
	const unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;

	return x < y ? -1 : x > y;
}

int main(int argc, char **argv)
{
	const char *const dir = argc > 1 ? argv[1] : "build/mame_check";
	const char *const rom = argc > 2 ? argv[2] : "rom/ddonpach.zip";
	const unsigned keep = getenv("MAME_REDRAW_KEEP") ? (unsigned)atoi(getenv("MAME_REDRAW_KEEP")) : 5u;
	static uint32_t mame[VIDEO_WIDTH * VIDEO_HEIGHT], port[VIDEO_WIDTH * VIDEO_HEIGHT], best[VIDEO_WIDTH * VIDEO_HEIGHT];
	static uint32_t diff[VIDEO_WIDTH * VIDEO_HEIGHT];
	unsigned *points = NULL, count = 0, same = 0, differ = 0, kept = 0, skipped = 0;
	DIR *folder;
	struct dirent *entry;

	if (getenv("MAKE"))
		return make(dir, rom);
	if (!(folder = opendir(dir)))
	{
		printf("%s: no such folder\n", dir);
		return 1;
	}
	while ((entry = readdir(folder)) != NULL)
	{
		unsigned n;
		char tail[8];

		if (sscanf(entry->d_name, "video_%u.%7s", &n, tail) == 2 && !strcmp(tail, "bin"))
		{
			unsigned *const more = realloc(points, sizeof(unsigned) * (count + 1u));

			if (!more)
				break;
			points = more;
			points[count++] = n;
		}
	}
	closedir(folder);
	if (!count)
	{
		printf("%s: no video_N.bin (ddp_check.lua 0.5.0 writes them)\n", dir);
		return 1;
	}
	qsort(points, count, sizeof(unsigned), by_number);
	if (!start(rom))
		return 1;
	printf("%u video states in %s\n", count, dir);
	for (unsigned at = 0; at < count; at++)
	{
		const unsigned n = points[at];
		char path[1024];
		size_t size_now = 0, size_before = 0;
		uint8_t *now, *before = NULL;
		unsigned least = ~0u;
		const char *how = "";

		/* (each point is VBLANK N with N-1 beside it: N-1 on its own is drawn as N's "before") */
		if (at + 1u < count && points[at + 1u] == n + 1u)
			continue;
		snprintf(path, sizeof(path), "%s/video_%u.bin", dir, n);
		now = read_file(path, &size_now);
		if (!now || size_now != video_size() || memcmp(now, magic, sizeof(magic)))
		{
			printf("  %u: not a video state (%zu bytes)\n", n, size_now);
			free(now);
			skipped++;
			continue;
		}
		if (n > 0)
		{
			snprintf(path, sizeof(path), "%s/video_%u.bin", dir, n - 1u);
			before = read_file(path, &size_before);
			if (before && (size_before != video_size() || memcmp(before, magic, sizeof(magic))))
			{
				free(before);
				before = NULL;
			}
		}
		if (!mame_picture(dir, n, mame))
		{
			printf("  %u: no picture of MAME's (or not %dx%d)\n", n, VIDEO_WIDTH, VIDEO_HEIGHT);
			free(now);
			free(before);
			skipped++;
			continue;
		}
		{
			/* the ways to draw it: the sprites and the rest each from N-1 or N, the sprites from either bank (N-1's
			   memories with the other bank first: as MAME's picture N is drawn) */
			const uint8_t *const sources[2] = { before ? before : now, now };
			static const char *const names[8] = { "N-1's memories, the other sprite bank", "N-1's memories",
				"N-1's sprites, N's rest", "N-1's sprites (the other bank), N's rest", "N's sprites, N-1's rest",
				"N's sprites (the other bank), N-1's rest", "N's memories", "N's memories, the other sprite bank" };
			static const int sprites_of[8] = { 0, 0, 0, 0, 1, 1, 1, 1 }, rest_of[8] = { 0, 0, 1, 1, 0, 0, 1, 1 };
			static const bool other[8] = { true, false, false, true, false, true, false, true };

			for (int way = before ? 0 : 6; way < 8; way++)
			{
				unsigned now_differ;

				draw_from(sources[sprites_of[way]], sources[rest_of[way]], other[way], port);
				now_differ = differing(port, mame);
				if (now_differ < least)
				{
					least = now_differ;
					how = names[way];
					memcpy(best, port, sizeof(best));
				}
			}
		}
		{
			/* the stage, from the work RAM kept with it ($101978 the stage, $10197A the loop) */
			const uint8_t *const ram = now + video_size() - parts[WORK_RAM_PART].size;
			const unsigned stage = (unsigned)(ram[0x1978] << 8 | ram[0x1979]);
			const unsigned loop = (unsigned)(ram[0x197A] << 8 | ram[0x197B]);
			char where[16];

			if (stage < 7u && loop < 2u)
				snprintf(where, sizeof(where), "%u-%u", loop + 1u, stage + 1u);
			else
				snprintf(where, sizeof(where), "-");
			if (least == 0)
			{
				same++;
				printf("  %u (%s): the same (%s)\n", n, where, how);
			}
			else
			{
				differ++;
				printf("  %u (%s): %u pixels differ (nearest: %s)\n", n, where, least, how);
				if (kept < keep)
				{
					kept++;
					for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
						diff[i] = mame[i] != (best[i] & 0xFFFFFFu) ? 0xFFFFFFu : (mame[i] >> 2 & 0x3F3F3Fu);
					snprintf(path, sizeof(path), "redraw_mame_%u.ppm", n);
					write_ppm(path, mame, VIDEO_WIDTH, VIDEO_HEIGHT);
					snprintf(path, sizeof(path), "redraw_port_%u.ppm", n);
					write_ppm(path, best, VIDEO_WIDTH, VIDEO_HEIGHT);
					snprintf(path, sizeof(path), "redraw_diff_%u.ppm", n);
					write_ppm(path, diff, VIDEO_WIDTH, VIDEO_HEIGHT);
				}
			}
		}
		free(now);
		free(before);
	}
	printf("\n%u pictures the same, %u differ%s\n", same, differ, skipped ? " (some not compared: above)" : "");
	free(points);
	return differ ? 1 : 0;
}
