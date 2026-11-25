/**
 *    Name:        mame_compare.c
 *    Description: the port against the real MAME's ddonpach, from what tools/mame/ddp_check.lua wrote there: the same
 *                 player (tests/player.h's inputs, as MAME was given them, VBLANK by VBLANK) and player 1 kept from harm
 *                 the same way, then each VBLANK compared: the slowdown (whether the game came to its wait for a frame
 *                 since the last VBLANK), the ships' records and the scores (a sum, the signature), and at every
 *                 VBLANK MAME kept them, the whole work RAM and the picture
 *    Created:     25-11-2026
 *
 *    license:BSD-3-Clause
 *    Usage: mame_compare DIR [ddonpach.zip]   (DIR: ddp_check.lua's folder: log.txt, ram_N.bin, screen_N.bin)
 *    MAKE=1: writes DIR from the port itself instead, as ddp_check.lua would (DDP_FRAMES / DDP_EVERY as it has them),
 *    to check this tool: compared with itself, everything the same (SHIFT=N: MAME's count N ahead of the port's, as
 *    MAME's may be: lined up all the same).
 *    MAME's VBLANKs are counted from its first frame, the port's from the power on: the two are lined up by the first
 *    VBLANK the game comes to its wait at (after the board's self-test, which reads no inputs). Writes the first pictures that
 *    differ as mame_N.ppm, port_N.ppm and diff_N.ppm (white where they differ) where it runs. Prints what it finds
 */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "cave/video.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define READY      0x10120Cu    /* bit 0: the frame made ready for the interrupt (wait_for_frame) */
#define RAM_SIZE   0x10000u
#define MOST       2000000u     /* VBLANKs read from the log at most */

static unsigned player_inputs(uint64_t frame)
{
#include "player.h"
}

static uint16_t *inputs;
static uint8_t *mame_arrived;
static uint32_t *mame_signature;
static unsigned mame_vblanks;

static bool arrived;

static void at_wait(void)
{
	if (!arrived && (*cave_work_ram_byte(READY + 1u) & 1u))
		arrived = true;
}

static unsigned word(uint32_t address)
{
	return (unsigned)*cave_work_ram_byte(address) << 8 | *cave_work_ram_byte(address + 1u);
}

/* ddp_check.lua's signature: both ships' records ($102C8C-$102D0B) and the scores, summed as words */
static uint32_t signature(void)
{
	uint32_t sum = 0;

	for (uint32_t address = 0x102C8Cu; address <= 0x102D0Au; address += 2u)
		sum += word(address);
	return sum + word(0x10161Eu) + word(0x101620u) + word(0x101622u) + word(0x101624u);
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
	program_at_wait = at_wait;
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	return true;
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

/* ---- MAKE=1: the folder ddp_check.lua would write, from the port ---- */

static int make(const char *dir, const char *rom)
{
	const unsigned frames = getenv("DDP_FRAMES") ? (unsigned)atoi(getenv("DDP_FRAMES")) : 300000u;
	const unsigned every = getenv("DDP_EVERY") ? (unsigned)atoi(getenv("DDP_EVERY")) : 5000u;
	const uint64_t shift = getenv("SHIFT") ? strtoull(getenv("SHIFT"), NULL, 10) : 0u;    /* (MAME's count ahead) */
	static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT];
	char path[1024];
	FILE *log;

	if (!start(rom))
		return 1;
	snprintf(path, sizeof(path), "%s/log.txt", dir);
	if (!(log = fopen(path, "w")))
		return 1;
	fputs("; mame_compare MAKE=1: the port, written as ddp_check.lua writes MAME's\n", log);
	while (cave_board.frame < frames)
	{
		unsigned held;

		program_run_frame();
		cave_sound_drop();
		held = player_inputs(cave_board.frame + shift);
		cave_board.in0 = (uint16_t)~held;
		if (cave_board.frame + shift > 2500u)
			*cave_work_ram_byte(0x102CCAu) = 0x80u;
		fprintf(log, "%" PRIu64 " %04x %d %08x\n", cave_board.frame + shift, held, arrived ? 1 : 0, signature());
		arrived = false;
		if ((cave_board.frame + shift) % every == 0)
		{
			FILE *file;

			snprintf(path, sizeof(path), "%s/ram_%" PRIu64 ".bin", dir, cave_board.frame + shift);
			if ((file = fopen(path, "wb")) != NULL)
			{
				for (uint32_t at = 0; at < RAM_SIZE; at++)
					fputc(*cave_work_ram_byte(0x100000u + at), file);
				fclose(file);
			}
			video_latch_sprites();
			video_draw(frame);
			snprintf(path, sizeof(path), "%s/screen_%" PRIu64 ".bin", dir, cave_board.frame + shift);
			if ((file = fopen(path, "wb")) != NULL)
			{
				for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
				{
					const uint32_t pixel = frame[i] | 0xFF000000u;

					fputc(pixel & 255, file);
					fputc(pixel >> 8 & 255, file);
					fputc(pixel >> 16 & 255, file);
					fputc(pixel >> 24, file);
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
	}
	fclose(log);
	printf("written: %s (%u VBLANKs)\n", dir, frames);
	return 0;
}

/* ---- the comparison ---- */

static bool read_log(const char *dir)
{
	char path[1024], line[256];
	FILE *log;

	snprintf(path, sizeof(path), "%s/log.txt", dir);
	if (!(log = fopen(path, "r")))
	{
		printf("no %s\n", path);
		return false;
	}
	inputs = calloc(MOST + 1u, sizeof(*inputs));
	mame_arrived = calloc(MOST + 1u, 1);
	mame_signature = calloc(MOST + 1u, sizeof(*mame_signature));
	while (fgets(line, sizeof(line), log))
	{
		unsigned long vblank;
		unsigned held, came;
		uint32_t sum;

		if (line[0] == ';' || sscanf(line, "%lu %x %u %x", &vblank, &held, &came, &sum) != 4 || vblank > MOST)
			continue;
		inputs[vblank] = (uint16_t)held;
		mame_arrived[vblank] = (uint8_t)came;
		mame_signature[vblank] = sum;
		if (vblank > mame_vblanks)
			mame_vblanks = (unsigned)vblank;
	}
	fclose(log);
	return mame_vblanks > 0;
}

/* MAME's picture at VBLANK 'vblank' against the port's now and a VBLANK before: the pixels that differ */
static void compare_picture(const char *dir, unsigned vblank, const uint32_t *now, const uint32_t *before,
	unsigned *same, unsigned *differ)
{
	static uint32_t mame[VIDEO_WIDTH * VIDEO_HEIGHT], diff[VIDEO_WIDTH * VIDEO_HEIGHT];
	static bool written;
	char path[1024];
	size_t size = 0;
	uint8_t *data;
	int width = 0, height = 0;
	FILE *file;
	unsigned differ_now = 0, differ_before = 0;

	snprintf(path, sizeof(path), "%s/screen_size.txt", dir);
	if ((file = fopen(path, "r")) != NULL)
	{
		if (fscanf(file, "%d %d", &width, &height) != 2)
			width = height = 0;
		fclose(file);
	}
	snprintf(path, sizeof(path), "%s/screen_%u.bin", dir, vblank);
	if (!(data = read_file(path, &size)))
		return;
	if (width != VIDEO_WIDTH || height != VIDEO_HEIGHT || size < (size_t)width * (size_t)height * 4u)
	{
		printf("  picture %u: MAME's is %dx%d (%zu bytes), the port's %dx%d: not compared\n", vblank, width, height,
			size, VIDEO_WIDTH, VIDEO_HEIGHT);
		free(data);
		return;
	}
	for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
	{
		mame[i] = (uint32_t)data[4 * i] | (uint32_t)data[4 * i + 1] << 8 | (uint32_t)data[4 * i + 2] << 16;
		differ_now += mame[i] != (now[i] & 0xFFFFFFu);
		differ_before += mame[i] != (before[i] & 0xFFFFFFu);
		diff[i] = mame[i] != (now[i] & 0xFFFFFFu) ? 0xFFFFFFu : (mame[i] >> 2 & 0x3F3F3Fu);
	}
	free(data);
	if (differ_now == 0)
		(*same)++;
	else
	{
		(*differ)++;
		printf("  picture %u: %u pixels differ (the port's a VBLANK before: %u)\n", vblank, differ_now, differ_before);
		if (!written)
		{
			written = true;
			snprintf(path, sizeof(path), "mame_%u.ppm", vblank);
			write_ppm(path, mame, VIDEO_WIDTH, VIDEO_HEIGHT);
			snprintf(path, sizeof(path), "port_%u.ppm", vblank);
			write_ppm(path, now, VIDEO_WIDTH, VIDEO_HEIGHT);
			snprintf(path, sizeof(path), "diff_%u.ppm", vblank);
			write_ppm(path, diff, VIDEO_WIDTH, VIDEO_HEIGHT);
		}
	}
}

static void compare_ram(const char *dir, unsigned vblank, unsigned *same, unsigned *differ)
{
	char path[1024];
	size_t size = 0;
	uint8_t *const data = (snprintf(path, sizeof(path), "%s/ram_%u.bin", dir, vblank), read_file(path, &size));
	unsigned count = 0;

	if (!data)
		return;
	if (size != RAM_SIZE)
	{
		printf("  RAM %u: %zu bytes, not 64 KB\n", vblank, size);
		free(data);
		return;
	}
	for (uint32_t at = 0; at < RAM_SIZE; at++)
		if (data[at] != *cave_work_ram_byte(0x100000u + at))
		{
			if (count < 8)
				printf("  RAM %u: $%06X MAME %02X, the port %02X\n", vblank, 0x100000u + at, data[at],
					*cave_work_ram_byte(0x100000u + at));
			count++;
		}
	free(data);
	if (count == 0)
		(*same)++;
	else
	{
		(*differ)++;
		printf("  RAM %u: %u bytes differ\n", vblank, count);
	}
}

/* the VBLANKs MAME kept the RAM and the picture at (ram_N.bin in the folder) */
static uint8_t *kept;

static void find_kept(const char *dir)
{
	DIR *const folder = opendir(dir);
	struct dirent *entry;

	kept = calloc(MOST + 2u, 1);
	if (!folder)
		return;
	while ((entry = readdir(folder)) != NULL)
	{
		unsigned long vblank;
		char tail[8];

		if (sscanf(entry->d_name, "ram_%lu.%7s", &vblank, tail) == 2 && !strcmp(tail, "bin") && vblank <= MOST)
			kept[vblank] = 1;
	}
	closedir(folder);
}

typedef struct
{
	unsigned compared, lag_mame, lag_port, lag_differ, first_lag_differ, sig_differ, first_sig_differ;
	unsigned ram_same, ram_differ, pictures_same, pictures_differ;
} result;

/* the port from the power on, MAME's inputs at MAME's VBLANKs ('offset': MAME's count less the port's), each read
   'delay' VBLANKs later than the log has them (the script's own timing: see main); up to MAME's VBLANK 'last', or
   to the first that differs ('stop'); 'full': the RAM and the pictures too */
static result run(const char *dir, const char *rom, long offset, long delay, long last, bool stop, bool full)
{
	static uint32_t now[VIDEO_WIDTH * VIDEO_HEIGHT], before[VIDEO_WIDTH * VIDEO_HEIGHT];
	result r = { 0 };

	if (!start(rom))
		exit(1);
	arrived = false;
	for (;;)
	{
		long v;

		program_run_frame();
		cave_sound_drop();
		v = (long)cave_board.frame + offset;
		if (v > last)
			break;
		if (v >= 1)
		{
			const bool lag_m = !mame_arrived[v], lag_p = !arrived;

			/* (as ddp_check.lua: the inputs for the next VBLANK and player 1's time without harm, then the rest) */
			cave_board.in0 = (uint16_t)~(v - delay >= 1 ? inputs[v - delay] : 0u);
			if (v > 2500)
				*cave_work_ram_byte(0x102CCAu) = 0x80u;
			r.compared++;
			r.lag_mame += v > 1 && lag_m;
			r.lag_port += v > 1 && lag_p;
			if (v > 1 && lag_m != lag_p && !r.lag_differ++)
				r.first_lag_differ = (unsigned)v;
			if (signature() != mame_signature[v] && !r.sig_differ++)
				r.first_sig_differ = (unsigned)v;
			if (stop && (r.lag_differ || r.sig_differ))
				break;
			if (full && (kept[v] || (v + 1 <= (long)MOST && kept[v + 1])))
			{
				/* (the picture drawn only where MAME kept one, and a VBLANK before it) */
				if (kept[v + 1])
				{
					video_latch_sprites();
					video_draw(before);
				}
				if (kept[v])
				{
					video_latch_sprites();
					video_draw(now);
					compare_ram(dir, (unsigned)v, &r.ram_same, &r.ram_differ);
					compare_picture(dir, (unsigned)v, now, before, &r.pictures_same, &r.pictures_differ);
				}
			}
		}
		arrived = false;
	}
	return r;
}

int main(int argc, char **argv)
{
	const char *const dir = argc > 1 ? argv[1] : "build/mame_check";
	const char *const rom = argc > 2 ? argv[2] : "ddonpach.zip";
	unsigned port_first = 0, mame_first = 0;
	long offset, delay = 0;
	unsigned best = 0;
	result r;

	if (getenv("MAKE"))
		return make(dir, rom);
	if (!read_log(dir))
		return 1;
	find_kept(dir);
	for (unsigned v = 1; v <= mame_vblanks && !mame_first; v++)
		if (mame_arrived[v])
			mame_first = v;
	if (!start(rom))
		return 1;
	/* the port's first wait, nothing pressed: the board's self-test before it, its interrupts off (the inputs, from
	   VBLANK 100, not read yet) */
	while (cave_board.frame < 1000u && !port_first)
	{
		program_run_frame();
		cave_sound_drop();
		if (arrived)
			port_first = (unsigned)cave_board.frame;
		arrived = false;
	}
	if (!mame_first || !port_first)
	{
		printf("the game's first wait not found (MAME's %u, the port's %u)\n", mame_first, port_first);
		return 1;
	}
	offset = (long)mame_first - (long)port_first;
	printf("MAME's log: %u VBLANKs. The first wait: MAME's VBLANK %u, the port's %u (MAME's = the port's %+ld)\n",
		mame_vblanks, mame_first, port_first, offset);

	/* when MAME's game read the inputs the log gives: ddp_check.lua 0.1.0 set them a VBLANK late (MAME's frame
	   notifier comes after the VBLANK's interrupt has read them); 0.2.0 on time. The delay that keeps the two the same
	   longest, of 0, 1 and 2 (the ship answers the stick from about VBLANK 2,000) */
	for (long d = 0; d <= 2; d++)
	{
		const result tried = run(dir, rom, offset, d, mame_vblanks < 30000u ? (long)mame_vblanks : 30000, true, false);
		const unsigned until = tried.lag_differ || tried.sig_differ ? (tried.first_sig_differ && (!tried.first_lag_differ
			|| tried.first_sig_differ < tried.first_lag_differ) ? tried.first_sig_differ : tried.first_lag_differ)
			: 0xFFFFFFFFu;

		if (until > best)
		{
			best = until;
			delay = d;
		}
	}
	printf("MAME's game read the log's inputs %ld VBLANK%s later than the port's would\n", delay, delay == 1 ? "" : "s");

	r = run(dir, rom, offset, delay, (long)mame_vblanks, false, true);
	printf("compared %u VBLANKs\n", r.compared);
	if (r.lag_differ || r.sig_differ)
	{
		const unsigned parted = r.first_sig_differ && (!r.first_lag_differ || r.first_sig_differ < r.first_lag_differ)
			? r.first_sig_differ : r.first_lag_differ;

		printf("the same until MAME's VBLANK %u: there %s (after it the two games go apart: what follows differs "
			"from that alone)\n", parted, parted == r.first_lag_differ
			? (mame_arrived[parted] ? "the port slowed down and MAME didn't" : "MAME slowed down and the port didn't")
			: "the ships or the scores differ");
	}
	printf("slowdown: MAME %u VBLANKs lagged, the port %u; %u differ", r.lag_mame, r.lag_port, r.lag_differ);
	if (r.lag_differ)
		printf(" (the first at MAME's VBLANK %u)", r.first_lag_differ);
	printf("\nthe ships and scores: %u VBLANKs differ", r.sig_differ);
	if (r.sig_differ)
		printf(" (the first at MAME's VBLANK %u)", r.first_sig_differ);
	printf("\nwork RAM: %u the same, %u differ\npictures: %u the same, %u differ\n", r.ram_same, r.ram_differ,
		r.pictures_same, r.pictures_differ);
	printf("%s\n", r.lag_differ || r.sig_differ || r.ram_differ || r.pictures_differ ? "DIFFERENT" : "the same");
	return 0;
}
