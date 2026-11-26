/**
 *    Name:        achievements.c
 *    Description: goals met in the arcade game, kept in ddonpach.achievements (achievements.h)
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    From the game watched (game_watch.h). The file: "; ..." then "[achievements]" and "id = date" for each earned.
 */
#include "achievements.h"
#include "file_save.h"
#include "game_watch.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
	A_STAGE1, A_LOOP1, A_LOOP2, A_ALL, A_ONE_CREDIT, A_ONE_CREDIT_ALL, A_NO_MISS_STAGE, A_NO_MISS_LOOP,
	A_NO_BOMB_STAGE, A_BEES_STAGE, A_BEES_LOOP, A_HIT100, A_HIT500, A_HIT1000, A_SCORE10M, A_SCORE50M,
	A_SCORE100M, A_FULL_POWER, A_BOMB_SLOTS, A_EXTEND, A_TEAM, A_EVERY_SHIP, A_BOTH_STYLES, A_SURVIVOR,
	A_HIDDEN_EXTEND, A_AREA7, A_HIBACHI, A_HIBACHI_DOWN,
	ACHIEVEMENTS
};

static const struct { const char *id, *name, *text; } list[ACHIEVEMENTS] = {
	{ "stage1",         "FIRST STEP",       "CLEAR STAGE 1-1" },
	{ "loop1",          "FIRST LOOP",       "CLEAR STAGE 1-6" },
	{ "loop2",          "SECOND LOOP",      "REACH STAGE 2-1" },
	{ "all_clear",      "ALL CLEAR",        "CLEAR STAGE 2-6" },
	{ "one_credit",     "ONE CREDIT",       "CLEAR 1-6 WITHOUT CONTINUES" },
	{ "one_credit_all", "ONE CREDIT CLEAR", "CLEAR 2-6 WITHOUT CONTINUES" },
	{ "no_miss_stage",  "UNTOUCHABLE",      "CLEAR A STAGE WITHOUT DYING" },
	{ "no_miss_loop",   "PERFECT LOOP",     "THE FIRST LOOP WITHOUT DYING" },
	{ "no_bomb_stage",  "NO BOMBS NEEDED",  "CLEAR A STAGE WITHOUT BOMBS" },
	{ "bees_stage",     "BEE KEEPER",       "ALL 13 BEES IN A STAGE" },
	{ "bees_loop",      "QUEEN BEE",        "ALL BEES IN THE FIRST LOOP" },
	{ "hit100",         "100 HITS",         "A HIT COMBO OF 100" },
	{ "hit500",         "500 HITS",         "A HIT COMBO OF 500" },
	{ "hit1000",        "1000 HITS",        "A HIT COMBO OF 1000" },
	{ "score10m",       "TEN MILLION",      "SCORE 10,000,000" },
	{ "score50m",       "FIFTY MILLION",    "SCORE 50,000,000" },
	{ "score100m",      "HUNDRED MILLION",  "SCORE 100,000,000" },
	{ "full_power",     "FULL POWER",       "SHOT AND LASER BOTH MAX" },
	{ "bomb_slots",     "BOMB HOARDER",     "CARRY 6 BOMBS" },
	{ "extend",         "EXTRA LIFE",       "EARN AN EXTRA LIFE" },
	{ "team",           "TEAM WORK",        "CLEAR A STAGE WITH 2 PLAYERS" },
	{ "every_ship",     "TEST PILOT",       "CLEAR 1-1 WITH TYPES A, B, C" },
	{ "both_styles",    "SHOT AND LASER",   "CLEAR 1-1 IN BOTH STYLES" },
	{ "survivor",       "SURVIVOR",         "5 MINUTES WITHOUT DYING" },
	{ "hidden_extend",  "HIDDEN EXTEND",    "THE STAGE 3 SHIP'S 1UP" },
	{ "area7",          "HIDDEN STAGE",     "REACH STAGE 2-7" },
	{ "hibachi",        "HIBACHI",          "FACE THE TRUE LAST BOSS" },
	{ "hibachi_down",   "TRUE ENDING",      "SHOOT DOWN HIBACHI" },
};

static char dates[ACHIEVEMENTS][16];    /* when each was earned ("" not yet) */
static unsigned ships_cleared, styles_cleared;    /* (bits: the ships and styles 1-1 was cleared with) */
static char achievements_path[1024];
static int shown_queue[ACHIEVEMENTS], queued;

/* ---- the file ---- */

static void save(void)
{
	file_text text = { 0 };

	if (!achievements_path[0])
		return;
	/* (built in memory, written by file_save's helper: an achievement earned in play doesn't wait for the disk) */
	file_text_add(&text, "; DoDonPachi's achievements: when each was earned (arcade games only)\n[achievements]\n");
	for (int index = 0; index < ACHIEVEMENTS; index++)
		if (dates[index][0])
			file_text_add(&text, "%s = %s\n", list[index].id, dates[index]);
	file_text_add(&text, "ships_cleared = %u\n", ships_cleared);
	file_text_add(&text, "styles_cleared = %u\n", styles_cleared);
	file_text_save(&text, achievements_path);
}

void achievements_load(const char *path)
{
	char line[256];
	FILE *file;

	snprintf(achievements_path, sizeof(achievements_path), "%s", path);
	if (!(file = fopen(path, "r")))
		return;
	while (fgets(line, sizeof(line), file))
	{
		char id[64], date[16];

		if (sscanf(line, " %63[a-z0-9_] = %15s", id, date) != 2)
			continue;
		if (!strcmp(id, "ships_cleared"))
			sscanf(date, "%u", &ships_cleared);
		else if (!strcmp(id, "styles_cleared"))
			sscanf(date, "%u", &styles_cleared);
		for (int index = 0; index < ACHIEVEMENTS; index++)
			if (!strcmp(id, list[index].id))
				snprintf(dates[index], sizeof(dates[index]), "%s", date);
	}
	fclose(file);
}

static void earn(int index)
{
	time_t now = time(NULL);
	const struct tm *const local = localtime(&now);

	if (dates[index][0])
		return;
	if (local)
		strftime(dates[index], sizeof(dates[index]), "%Y-%m-%d", local);
	else
		snprintf(dates[index], sizeof(dates[index]), "yes");
	if (queued < ACHIEVEMENTS)
		shown_queue[queued++] = index;
	save();
}

/* ---- the game, watched ---- */

#define SURVIVOR_FRAMES (5u * 60u * 57u)

void achievements_frame(bool counts)
{
	const game_watch_state *const g = &game_watch;

	(void)counts;    /* (game_watch has it, from the game's ship select on) */
	if (!g->on || !g->counts)
		return;
	if (g->cleared >= 0)
	{
		const int stage = g->cleared;

		earn(A_STAGE1);
		if (stage == 0)
		{
			ships_cleared |= 1u << g->ship;
			styles_cleared |= g->laser ? 2u : 1u;
			save();
			if (ships_cleared == 7u)
				earn(A_EVERY_SHIP);
			if (styles_cleared == 3u)
				earn(A_BOTH_STYLES);
		}
		if (g->cleared_misses == 0)
			earn(A_NO_MISS_STAGE);
		if (g->cleared_bombs == 0)
			earn(A_NO_BOMB_STAGE);
		if (g->cleared_bees >= 13u)
			earn(A_BEES_STAGE);
		if (g->cleared_two_players)
			earn(A_TEAM);
		if (stage == 5)
		{
			earn(A_LOOP1);
			if (!g->continued_ever)
				earn(A_ONE_CREDIT);
			if (g->loop1_misses == 0 && !g->continued_ever)
				earn(A_NO_MISS_LOOP);
			if (g->loop1_full_bees >= 6u)
				earn(A_BEES_LOOP);
		}
		if (stage == 11)
		{
			earn(A_ALL);
			if (!g->continued_ever)
				earn(A_ONE_CREDIT_ALL);
		}
	}
	if (!g->playing)
		return;
	if (g->stage >= 6)
		earn(A_LOOP2);
	if (g->extended)
		earn(A_EXTEND);
	if (g->hidden_extend)
		earn(A_HIDDEN_EXTEND);
	if (g->area7_reached)
		earn(A_AREA7);
	if (g->hibachi_met)
		earn(A_HIBACHI);
	if (g->hibachi_defeated)
		earn(A_HIBACHI_DOWN);
	if (g->max_hit >= 100u)
		earn(A_HIT100);
	if (g->max_hit >= 500u)
		earn(A_HIT500);
	if (g->max_hit >= 1000u)
		earn(A_HIT1000);
	/* (the score as the game counts it: a tenth of what it shows) */
	if (g->score >= 1000000u)
		earn(A_SCORE10M);
	if (g->score >= 5000000u)
		earn(A_SCORE50M);
	if (g->score >= 10000000u)
		earn(A_SCORE100M);
	if (g->own_power >= 4u && g->other_power >= 4u)
		earn(A_FULL_POWER);
	if (g->bomb_most >= 6u && g->bombs >= 6u)
		earn(A_BOMB_SLOTS);
	if (g->life_frames >= SURVIVOR_FRAMES)
		earn(A_SURVIVOR);
}

const char *achievements_next_shown(void)
{
	int index;

	if (queued == 0)
		return NULL;
	index = shown_queue[0];
	memmove(shown_queue, shown_queue + 1, sizeof(shown_queue[0]) * (size_t)(queued - 1));
	queued--;
	return list[index].name;
}

int achievements_waiting(void)
{
	return queued;
}

int achievements_count(void)
{
	return ACHIEVEMENTS;
}

int achievements_done(void)
{
	int done = 0;

	for (int index = 0; index < ACHIEVEMENTS; index++)
		done += dates[index][0] != '\0';
	return done;
}

const char *achievements_name(int index)
{
	return index >= 0 && index < ACHIEVEMENTS ? list[index].name : "";
}

const char *achievements_text(int index)
{
	return index >= 0 && index < ACHIEVEMENTS ? list[index].text : "";
}

int achievements_find(const char *id)
{
	for (int index = 0; index < ACHIEVEMENTS; index++)
		if (!strcmp(list[index].id, id))
			return index;
	return -1;
}

const char *achievements_date(int index)
{
	return index >= 0 && index < ACHIEVEMENTS && dates[index][0] ? dates[index] : NULL;
}
