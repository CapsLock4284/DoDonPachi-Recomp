/**
 *    Name:        hiscore.c
 *    Description: the high score table kept in a file (hiscore.h)
 *    Version:     0.1.0 (draft)
 *    Created:     17-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's, with DoDonPachi's table (hiscore.h). The file: "DDHISC01", then the table's 100 bytes as the
 *    game has them.
 */
#include "hiscore.h"
#include "paths.h"

#include "cave/machine.h"
#include "cave/program.h"

#include <SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TABLE       0x1016EAu
#define TABLE_SIZE  0x64u           /* to $10174D */
#define HUD_HIGH    0x101626u       /* the HIGH the game shows: a BCD long */
#define HUD_DIGIT   0x101750u       /* its last digit: the continues its holder used, a tile ($100 + them; set with
                                       it, in 044BBE) */
#define CONTINUES   0x101744u       /* the table's: #1's continues (a word) */
#define NO_GAME     0x1017A5u       /* $80 in the attract mode and on the title, 0 in a game */

static const char magic[8] = { 'D', 'D', 'H', 'I', 'S', 'C', '0', '1' };

/* the default scores, BCD: what the game's boot (00043A) writes */
static const uint32_t default_scores[5] = { 0x590806u, 0x554433u, 0x532708u, 0x444015u, 0x356276u };

static char path[1024];
static uint8_t file_data[TABLE_SIZE];   /* the file's table, as the game's bytes */
static bool have_file;
static uint8_t saved[TABLE_SIZE];       /* the table as last saved (or put back) */
static bool pending = true;             /* waiting for the game to set its table up */
static bool restored;                   /* the file's table was put back this boot: the table may be saved */

static void read_table(uint8_t *out)
{
	for (uint32_t at = 0; at < TABLE_SIZE; at++)
		out[at] = *cave_work_ram_byte(TABLE + at);
}

static uint32_t long_at(uint32_t address)
{
	uint32_t value = 0;

	for (uint32_t at = 0; at < 4u; at++)
		value = value << 8 | *cave_work_ram_byte(address + at);
	return value;
}

static bool defaults_set(void)
{
	for (int entry = 0; entry < 5; entry++)
		if (long_at(TABLE + 4u * (uint32_t)entry) != default_scores[entry])
			return false;
	return long_at(HUD_HIGH) == default_scores[0];
}

const char *hiscore_open(const char *file_path)
{
	FILE *file;
	uint8_t header[sizeof(magic)];

	if (file_path)
		snprintf(path, sizeof(path), "%s", file_path);
	else
		snprintf(path, sizeof(path), "%sddonpach.hiscore", paths_folder());
	have_file = false;
	if (!(file = fopen(path, "rb")))
		return NULL;
	if (fread(header, 1, sizeof(header), file) != sizeof(header) || memcmp(header, magic, sizeof(magic))
			|| fread(file_data, 1, TABLE_SIZE, file) != TABLE_SIZE)
	{
		fclose(file);
		return "not a DoDonPachi high score file: the game's defaults are used (and saved over it)";
	}
	fclose(file);
	have_file = true;
	return NULL;
}

void hiscore_reset(void)
{
	pending = true;
	restored = false;
}

bool hiscore_between_games(void)
{
	return (*cave_work_ram_byte(NO_GAME) & 0x80u) != 0;
}

static void save(const uint8_t *table)
{
	FILE *file;

	memcpy(file_data, table, TABLE_SIZE);
	have_file = true;
	memcpy(saved, table, TABLE_SIZE);
	if (!(file = fopen(path, "wb")))
	{
		fprintf(stderr, "%s: could not be written\n", path);
		return;
	}
	if (fwrite(magic, 1, sizeof(magic), file) != sizeof(magic) || fwrite(file_data, 1, TABLE_SIZE, file) != TABLE_SIZE)
		fprintf(stderr, "%s: could not be written\n", path);
	fclose(file);
}

/* #1's score as the game's HIGH, with its last digit (#1's continues) */
static void put_high_back(void)
{
	const uint32_t continues = CONTINUES - TABLE;

	for (uint32_t at = 0; at < 4u; at++)
		*cave_work_ram_byte(HUD_HIGH + at) = file_data[at];
	*cave_work_ram_byte(HUD_DIGIT) = 0x01u;
	*cave_work_ram_byte(HUD_DIGIT + 1u) = file_data[continues + 1u];
}

static void put_table_back(bool high_too)
{
	uint32_t high = 0;

	if (!restored || !have_file)
		return;
	for (uint32_t at = 0; at < TABLE_SIZE; at++)
		*cave_work_ram_byte(TABLE + at) = file_data[at];
	/* the HIGH: the file's #1, or (a game going on) the player's score already over it */
	for (uint32_t at = 0; at < 4u; at++)
		high = high << 8 | file_data[at];
	if (high_too || long_at(HUD_HIGH) < high)
		put_high_back();
	read_table(saved);
}

void hiscore_state_loaded(void)
{
	put_table_back(false);
}

void hiscore_game_not_kept(void)
{
	put_table_back(true);
}

void hiscore_frame(bool enabled, bool saving)
{
	uint8_t table[TABLE_SIZE];

	if (pending)
	{
		/* (the boot clears the memory, then the game writes its defaults, once) */
		if (!defaults_set())
			return;
		pending = false;
		if (!enabled)
			return;    /* (this boot's table is the game's own: it is not saved) */
		if (have_file)
		{
			for (uint32_t at = 0; at < TABLE_SIZE; at++)
				*cave_work_ram_byte(TABLE + at) = file_data[at];
			put_high_back();
		}
		read_table(saved);
		restored = true;
		return;
	}
	if (!enabled || !restored || !saving || program_frame_lagged())
		return;    /* (a frame of slowdown: the table may be half written) */
	read_table(table);
	if (memcmp(table, saved, TABLE_SIZE))
		save(table);
}
