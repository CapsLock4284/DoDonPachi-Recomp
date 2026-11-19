/**
 *    Name:        replay.c
 *    Description: replays: recorded, saved, listed, deleted and played back (replay.h)
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    The file (little endian):
 *      "DDREPLAY", the format (16 bits: 1), the port's version (16 bytes), the date (20 bytes, "2026-11-18 18:40"),
 *      player 1's best score (32), the stage reached (8: 0-11), the ship (8), the style (8: 1 laser), its end (8: 1 a
 *      game over, 2 all clear: 2-6 cleared), the game's first frame (32: its ship select), the frames in all (32)
 *      the run's start: the slowdown (8: 1 on), the game's version (8), the high score table put back (8: 1), a byte
 *      unused, the EEPROM (64 words of 16), the high score table (100 bytes)
 *      the checks: their count (32), then each: the frame after which (32), cave_state_hash() then (64)
 *      the inputs: each frame's IN0 and IN1 (16 each, active low), from the board's reset
 *    The file's name: the date and time it was saved, "2026-11-18_184012.ddreplay"
 */
#include "replay.h"
#include "crash_log.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifndef DDONPACH_VERSION
#define DDONPACH_VERSION "0.0.0"
#endif

#define FORMAT      1u
#define CHECK_EVERY 600u        /* frames between checks (about 10 seconds) */
#define GAME_MODE   0x1017A6u
#define SCORE       0x10161Eu   /* player 1's (BCD long) */
#define STAGE       0x101978u
#define LOOP        0x10197Au
#define SHIP        0x101754u
#define STYLE       0x101756u
#define EXTENSION   ".ddreplay"

static const char replay_magic[8] = { 'D', 'D', 'R', 'E', 'P', 'L', 'A', 'Y' };

typedef struct
{
	uint32_t frame;
	uint64_t hash;
} check;

static char folder[1024];

void replay_folder(const char *path)
{
	snprintf(folder, sizeof(folder), "%s", path);
}

static unsigned word(uint32_t address)
{
	return *cave_work_ram_byte(address) << 8 | *cave_work_ram_byte(address + 1u);
}

static uint32_t bcd_long(uint32_t address)
{
	uint32_t value = 0;

	for (uint32_t at = 0; at < 4u; at++)
	{
		const unsigned bcd = *cave_work_ram_byte(address + at);

		value = value * 100u + (bcd >> 4) * 10u + (bcd & 15u);
	}
	return value;
}

/* ---- recording ---- */

static struct
{
	check *checks;
	uint32_t check_count, check_room;
	/* the game being played (or the last) */
	bool on, played;
	uint32_t begins;            /* its first frame (the crash log's count as its ship select came) */
	uint32_t score;
	int stage, ship;
	bool laser, all_clear;
	/* a game ended, waiting for the answer */
	bool waiting;
	uint32_t ends;
} rec;

void replay_reset(void)
{
	rec.check_count = 0;
	rec.on = rec.played = rec.waiting = false;
}

static void add_check(uint32_t frame)
{
	if (rec.check_count >= rec.check_room)
	{
		const uint32_t room = rec.check_room ? rec.check_room * 2u : 256u;
		check *const more = realloc(rec.checks, room * sizeof(check));

		if (!more)
			return;
		rec.checks = more;
		rec.check_room = room;
	}
	rec.checks[rec.check_count].frame = frame;
	rec.checks[rec.check_count].hash = cave_state_hash();
	rec.check_count++;
}

bool replay_after_frame(void)
{
	crash_log_run run;
	unsigned mode;

	crash_log_view(&run);
	if (!run.replayable || run.count == 0)
		return false;
	if (run.count % CHECK_EVERY == 0)
		add_check(run.count);
	if (program_frame_lagged())
		return false;    /* (the game's memory half way through its frame: looked at next) */
	mode = word(GAME_MODE);
	if (mode == 0xBu && !rec.on)
	{
		/* a game begins: its ship select */
		rec.on = true;
		rec.played = false;
		rec.waiting = false;
		rec.begins = run.count - 1u;    /* (the frame just run: the first of the select) */
		rec.score = 0;
		rec.stage = 0;
		rec.all_clear = false;
	}
	if (!rec.on)
		return false;
	if (mode == 7u || mode == 8u)
	{
		const unsigned stage = word(STAGE), loop = word(LOOP);

		if (!rec.played)
		{
			rec.ship = (int)(word(SHIP) % 3u);
			rec.laser = word(STYLE) == 1u;
		}
		rec.played = true;
		if (bcd_long(SCORE) > rec.score)
			rec.score = bcd_long(SCORE);
		if (stage < 6u && loop < 2u && (int)(loop * 6u + stage) > rec.stage)
			rec.stage = (int)(loop * 6u + stage);
		if (loop == 1u && stage == 6u)
			rec.all_clear = true;    /* (2-6 cleared: the ending) */
		return false;
	}
	if (mode == 0xBu)
		return false;
	/* the game is over (the ranking, the attract mode) */
	rec.on = false;
	if (!rec.played)
		return false;
	rec.waiting = true;
	rec.ends = run.count;
	return true;
}

bool replay_waiting(void)
{
	return rec.waiting;
}

static void put(FILE *file, uint64_t value, int bytes)
{
	for (int at = 0; at < bytes; at++)
		fputc((int)(value >> (8 * at) & 255u), file);
}

static void make_folder(void)
{
	char path[1024];
	size_t length;

	snprintf(path, sizeof(path), "%s", folder);
	length = strlen(path);
	if (length && (path[length - 1] == '/' || path[length - 1] == '\\'))
		path[length - 1] = '\0';
#ifdef _WIN32
	_mkdir(path);
#else
	mkdir(path, 0755);
#endif
}

const char *replay_keep(bool keep)
{
	static char name[64];
	char path[1400], date[20];
	crash_log_run run;
	FILE *file;
	time_t now = time(NULL);
	bool ok;

	if (!rec.waiting)
		return NULL;
	rec.waiting = false;
	if (!keep)
		return NULL;
	crash_log_view(&run);
	if (!run.replayable || run.count < rec.ends)
		return NULL;
	make_folder();
	strftime(name, sizeof(name), "%Y-%m-%d_%H%M%S" EXTENSION, localtime(&now));
	strftime(date, sizeof(date), "%Y-%m-%d %H:%M", localtime(&now));
	snprintf(path, sizeof(path), "%s%s", folder, name);
	for (int more = 2; more < 100 && (file = fopen(path, "rb")) != NULL; more++)
	{
		/* (one saved this second already: "_2" ...) */
		char stem[48];

		fclose(file);
		strftime(stem, sizeof(stem), "%Y-%m-%d_%H%M%S", localtime(&now));
		snprintf(name, sizeof(name), "%s_%d" EXTENSION, stem, more);
		snprintf(path, sizeof(path), "%s%s", folder, name);
	}
	if (!(file = fopen(path, "wb")))
		return NULL;
	fwrite(replay_magic, 1, sizeof(replay_magic), file);
	put(file, FORMAT, 2);
	{
		char version[16] = { 0 }, when[20] = { 0 };

		snprintf(version, sizeof(version), "%s", DDONPACH_VERSION);
		snprintf(when, sizeof(when), "%s", date);
		fwrite(version, 1, sizeof(version), file);
		fwrite(when, 1, sizeof(when), file);
	}
	put(file, rec.score, 4);
	put(file, (uint64_t)rec.stage, 1);
	put(file, (uint64_t)rec.ship, 1);
	put(file, rec.laser ? 1u : 0u, 1);
	put(file, rec.all_clear ? 2u : 1u, 1);
	put(file, rec.begins, 4);
	put(file, rec.ends, 4);
	put(file, run.slowdown ? 1u : 0u, 1);
	put(file, (uint64_t)run.version, 1);
	put(file, run.has_table ? 1u : 0u, 1);
	put(file, 0, 1);
	for (unsigned at = 0; at < CAVE_EEPROM_WORDS; at++)
		put(file, run.eeprom[at], 2);
	fwrite(run.table, 1, 100, file);
	{
		uint32_t checks = 0;

		for (uint32_t at = 0; at < rec.check_count; at++)
			checks += rec.checks[at].frame <= rec.ends;
		put(file, checks, 4);
		for (uint32_t at = 0; at < rec.check_count; at++)
			if (rec.checks[at].frame <= rec.ends)
			{
				put(file, rec.checks[at].frame, 4);
				put(file, rec.checks[at].hash, 8);
			}
	}
	for (uint32_t frame = 0; frame < rec.ends; frame++)
	{
		put(file, run.frames[frame * 2u], 2);
		put(file, run.frames[frame * 2u + 1u], 2);
	}
	ok = !ferror(file);
	if (fclose(file) != 0 || !ok)
	{
		remove(path);
		return NULL;
	}
	return name;
}

/* ---- reading ---- */

static uint64_t get(FILE *file, int bytes, bool *ok)
{
	uint64_t value = 0;

	for (int at = 0; at < bytes; at++)
	{
		const int c = fgetc(file);

		if (c == EOF)
			*ok = false;
		value |= (uint64_t)(c & 255) << (8 * at);
	}
	return value;
}

/* the header to the frames in all; false: not a replay of this port's format */
static bool read_header(FILE *file, replay_info *info, uint32_t *begins, uint32_t *all, int *version)
{
	char header[8], text[20];
	bool ok = true;

	if (fread(header, 1, sizeof(header), file) != sizeof(header) || memcmp(header, replay_magic, sizeof(replay_magic))
			|| get(file, 2, &ok) != FORMAT)
		return false;
	if (fread(text, 1, 16, file) != 16 || fread(text, 1, 20, file) != 20)
		return false;
	text[19] = '\0';
	snprintf(info->date, sizeof(info->date), "%s", text);
	info->score = (uint32_t)get(file, 4, &ok);
	info->stage = (int)get(file, 1, &ok) % 12;
	info->ship = (int)get(file, 1, &ok) % 3;
	info->laser = get(file, 1, &ok) != 0;
	info->all_clear = get(file, 1, &ok) == 2u;
	*begins = (uint32_t)get(file, 4, &ok);
	*all = (uint32_t)get(file, 4, &ok);
	info->frames = *all > *begins ? *all - *begins : 0u;
	get(file, 1, &ok);
	*version = (int)get(file, 1, &ok);
	return ok;
}

static int newest_first(const void *a, const void *b)
{
	return strcmp(((const replay_info *)b)->file, ((const replay_info *)a)->file);
}

static void list_one(const char *file_name, replay_info *out, int *count, int most)
{
	char path[1400];
	FILE *file;
	uint32_t begins, all;
	int version;
	const size_t length = strlen(file_name), extension = strlen(EXTENSION);

	if (*count >= most || length <= extension || strcmp(file_name + length - extension, EXTENSION))
		return;
	snprintf(path, sizeof(path), "%s%s", folder, file_name);
	if (!(file = fopen(path, "rb")))
		return;
	memset(&out[*count], 0, sizeof(out[*count]));
	if (read_header(file, &out[*count], &begins, &all, &version) && version == rom_set.version)
	{
		snprintf(out[*count].file, sizeof(out[*count].file), "%s", file_name);
		(*count)++;
	}
	fclose(file);
}

int replay_list(replay_info *out, int most)
{
	int count = 0;
#ifdef _WIN32
	char pattern[1400];
	WIN32_FIND_DATAA found;
	HANDLE search;

	snprintf(pattern, sizeof(pattern), "%s*" EXTENSION, folder);
	if ((search = FindFirstFileA(pattern, &found)) != INVALID_HANDLE_VALUE)
	{
		do
			list_one(found.cFileName, out, &count, most);
		while (FindNextFileA(search, &found));
		FindClose(search);
	}
#else
	DIR *const dir = opendir(folder[0] ? folder : ".");
	struct dirent *entry;

	if (dir)
	{
		while ((entry = readdir(dir)) != NULL)
			list_one(entry->d_name, out, &count, most);
		closedir(dir);
	}
#endif
	qsort(out, (size_t)count, sizeof(out[0]), newest_first);
	return count;
}

bool replay_delete(const char *file)
{
	char path[1400];

	snprintf(path, sizeof(path), "%s%s", folder, file);
	return remove(path) == 0;
}

/* ---- playback ---- */

static struct
{
	bool on, out_of_step;
	uint16_t *frames;
	uint32_t count, position;
	check *checks;
	uint32_t check_count, next_check;
} play;

const char *replay_play(const char *file_name, replay_start *start)
{
	char path[1400];
	FILE *file;
	replay_info info;
	uint32_t begins, all;
	int version;
	bool ok = true;

	replay_stop();
	snprintf(path, sizeof(path), "%s%s", folder, file_name);
	if (!(file = fopen(path, "rb")))
		return "it could not be opened";
	if (!read_header(file, &info, &begins, &all, &version) || version != rom_set.version)
	{
		fclose(file);
		return "it is not a replay of this version of the game";
	}
	start->game_begins = begins;
	fseek(file, 0, SEEK_SET);
	fseek(file, 8 + 2 + 16 + 20 + 4 + 4 + 4 + 4, SEEK_SET);
	start->slowdown = get(file, 1, &ok) != 0;
	get(file, 1, &ok);
	start->has_table = get(file, 1, &ok) != 0;
	get(file, 1, &ok);
	for (unsigned at = 0; at < CAVE_EEPROM_WORDS; at++)
		start->eeprom[at] = (uint16_t)get(file, 2, &ok);
	if (fread(start->table, 1, sizeof(start->table), file) != sizeof(start->table))
		ok = false;
	play.check_count = (uint32_t)get(file, 4, &ok);
	if (ok && play.check_count && !(play.checks = calloc(play.check_count, sizeof(check))))
		ok = false;
	for (uint32_t at = 0; ok && at < play.check_count; at++)
	{
		play.checks[at].frame = (uint32_t)get(file, 4, &ok);
		play.checks[at].hash = get(file, 8, &ok);
	}
	if (ok && all && !(play.frames = malloc((size_t)all * 2u * sizeof(uint16_t))))
		ok = false;
	for (uint32_t at = 0; ok && at < all * 2u; at++)
		play.frames[at] = (uint16_t)get(file, 2, &ok);
	fclose(file);
	if (!ok)
	{
		replay_stop();
		return "it is damaged (cut short)";
	}
	play.count = all;
	play.position = play.next_check = 0;
	play.out_of_step = false;
	play.on = true;
	return NULL;
}

bool replay_playing(void)
{
	return play.on;
}

bool replay_next(uint16_t *in0, uint16_t *in1)
{
	if (!play.on || play.position >= play.count)
		return false;
	*in0 = play.frames[play.position * 2u];
	*in1 = play.frames[play.position * 2u + 1u];
	play.position++;
	return true;
}

bool replay_after_playback_frame(void)
{
	while (play.next_check < play.check_count && play.checks[play.next_check].frame < play.position)
		play.next_check++;
	if (play.next_check < play.check_count && play.checks[play.next_check].frame == play.position)
	{
		const bool same = play.checks[play.next_check].hash == cave_state_hash();

		play.next_check++;
		if (!same && !play.out_of_step)
		{
			play.out_of_step = true;
			return false;
		}
	}
	return true;
}

uint32_t replay_position(void)
{
	return play.position;
}

uint32_t replay_length(void)
{
	return play.count;
}

void replay_stop(void)
{
	free(play.frames);
	free(play.checks);
	memset(&play, 0, sizeof(play));
}
