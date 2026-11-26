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
#include "loop2.h"

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

/* ---- how near each is (the gadget panels' goals) ---- */

/* (a score as shown, a tenth's last digit put back: "2,340,000") */
static void shown_points(char *out, size_t size, uint32_t kept)
{
	char digits[16], with[24];
	int length, at = 0;
	const unsigned long long shown = (unsigned long long)kept * 10ull;

	length = snprintf(digits, sizeof(digits), "%llu", shown);
	for (int i = 0; i < length; i++)
	{
		if (i && (length - i) % 3 == 0)
			with[at++] = ',';
		with[at++] = digits[i];
	}
	with[at] = '\0';
	snprintf(out, size, "%s", with);
}

static unsigned bits(unsigned value)
{
	unsigned count = 0;

	for (; value; value >>= 1)
		count += value & 1u;
	return count;
}

static float share(double now, double goal)
{
	return goal <= 0.0 ? 1.0f : now >= goal ? 1.0f : (float)(now / goal);
}

bool achievements_goal(int index, achievement_goal *out)
{
	const game_watch_state *const g = &game_watch;
	const int stage = g->stage;
	const bool loop1 = stage >= 0 && stage < 6;
	const loop2_status l2 = loop2_now(0);
	const bool loop2_out = l2.state == LOOP2_OUT_CONTINUED || l2.state == LOOP2_OUT_TWO_PLAYERS;
	static const unsigned hits[3] = { 100u, 500u, 1000u };
	static const uint32_t scores[3] = { 1000000u, 5000000u, 10000000u };    /* (kept: a tenth of shown) */
	static const char *const hit_labels[3] = { "100 HITS", "500 HITS", "1000 HITS" };
	static const char *const score_labels[3] = { "SCORE 10M", "SCORE 50M", "SCORE 100M" };

	if (index < 0 || index >= ACHIEVEMENTS || !g->on || stage < 0)
		return false;
	out->value[0] = '\0';
	out->state = GOAL_GOING;
	out->part = 0.0f;
	switch (index)
	{
	case A_STAGE1:
		if (stage != 0)
			return false;
		out->label = "CLEAR 1-1";
		out->part = 0.5f;
		snprintf(out->value, sizeof(out->value), "THIS STAGE");
		break;
	case A_LOOP1:
		if (!loop1)
			return false;
		out->label = "CLEAR 1-6";
		out->part = ((float)stage + 0.5f) / 6.0f;
		snprintf(out->value, sizeof(out->value), "AT 1-%d", stage + 1);
		break;
	case A_LOOP2:
		if (!loop1)
			return false;
		out->label = "LOOP 2";
		out->state = loop2_out ? GOAL_LOST : l2.state == LOOP2_IN ? GOAL_KEPT : GOAL_GOING;
		out->part = ((float)stage + (l2.state == LOOP2_IN ? 1.0f : 0.3f)) / 6.5f;
		if (loop2_out)
			snprintf(out->value, sizeof(out->value), "NO");
		else if (l2.state == LOOP2_IN)
			snprintf(out->value, sizeof(out->value), "YES: %s", l2.by ? l2.by : "");
		else if (l2.state == LOOP2_ON_TRACK)
			snprintf(out->value, sizeof(out->value), "%u MISS%s LEFT", LOOP2_MISSES - l2.misses,
				LOOP2_MISSES - l2.misses == 1u ? "" : "ES");
		else
			snprintf(out->value, sizeof(out->value), "SHORT");
		break;
	case A_ALL:
		if (loop1 && loop2_out)
			return false;
		out->label = "CLEAR 2-6";
		out->part = ((float)(stage % 12) + 0.5f) / 12.0f;
		snprintf(out->value, sizeof(out->value), "AT %d-%d", stage / 6 + 1, stage % 6 + 1);
		break;
	case A_ONE_CREDIT:
	case A_ONE_CREDIT_ALL:
	{
		const bool all = index == A_ONE_CREDIT_ALL;

		if ((!all && !loop1) || (all && loop1 && loop2_out))
			return false;
		out->label = all ? "1 CREDIT 2-6" : "1 CREDIT 1-6";
		out->state = g->continued_ever ? GOAL_LOST : GOAL_GOING;
		out->part = ((float)stage + 0.5f) / (all ? 12.0f : 6.0f);
		snprintf(out->value, sizeof(out->value), g->continued_ever ? "CONTINUED" : "AT %d-%d", stage / 6 + 1,
			stage % 6 + 1);
		break;
	}
	case A_NO_MISS_STAGE:
		out->label = "NO MISS";
		out->state = g->stage_misses ? GOAL_LOST : GOAL_KEPT;
		out->part = 0.55f;
		snprintf(out->value, sizeof(out->value), "%u", g->stage_misses);
		break;
	case A_NO_BOMB_STAGE:
		out->label = "NO BOMB";
		out->state = g->stage_bombs ? GOAL_LOST : GOAL_KEPT;
		out->part = 0.5f;
		snprintf(out->value, sizeof(out->value), "%u", g->stage_bombs);
		break;
	case A_NO_MISS_LOOP:
		if (!loop1)
			return false;
		out->label = "NO MISS LOOP";
		out->state = g->loop1_misses || g->continued_ever ? GOAL_LOST : GOAL_KEPT;
		out->part = ((float)stage + 0.5f) / 6.0f;
		snprintf(out->value, sizeof(out->value), "%u", g->loop1_misses);
		break;
	case A_BEES_STAGE:
		out->label = "ALL 13 BEES";
		out->state = g->stage_bees >= 13u ? GOAL_KEPT : GOAL_GOING;
		out->part = share(g->stage_bees, 13.0);
		snprintf(out->value, sizeof(out->value), "%u/13", g->stage_bees);
		break;
	case A_BEES_LOOP:
		if (!loop1)
			return false;
		out->label = "ALL BEES LOOP";
		out->state = g->loop1_full_bees < (unsigned)stage ? GOAL_LOST : GOAL_GOING;
		out->part = ((float)g->loop1_full_bees + share(g->stage_bees, 13.0)) / 6.0f;
		snprintf(out->value, sizeof(out->value), "%u/6", g->loop1_full_bees);
		break;
	case A_HIT100:
	case A_HIT500:
	case A_HIT1000:
	{
		const int step = index - A_HIT100;

		if (step > 0 && !dates[index - 1][0])
			return false;    /* (the next one only) */
		out->label = hit_labels[step];
		out->state = g->max_hit >= hits[step] ? GOAL_KEPT : GOAL_GOING;
		out->part = share(g->max_hit, hits[step]);
		snprintf(out->value, sizeof(out->value), "BEST %u", g->max_hit);
		break;
	}
	case A_SCORE10M:
	case A_SCORE50M:
	case A_SCORE100M:
	{
		const int step = index - A_SCORE10M;
		char points[20];

		if (step > 0 && !dates[index - 1][0])
			return false;
		out->label = score_labels[step];
		out->part = share(g->score, scores[step]);
		if (g->score >= scores[step])
			out->state = GOAL_KEPT;
		shown_points(points, sizeof(points), g->score >= scores[step] ? 0u : scores[step] - g->score);
		snprintf(out->value, sizeof(out->value), "%s TO GO", points);
		break;
	}
	case A_FULL_POWER:
		out->label = "FULL POWER";
		out->part = share(g->own_power + g->other_power, 8.0);
		snprintf(out->value, sizeof(out->value), "%u/8", g->own_power + g->other_power);
		break;
	case A_BOMB_SLOTS:
		/* (the bomb slots grow from 3 to 6, a life lost with them full: then all 6 carried) */
		out->label = "6 BOMBS";
		out->part = ((float)(g->bomb_most > 3u ? g->bomb_most - 3u : 0u) + (float)(g->bombs > 3u ? g->bombs - 3u : 0u))
			/ 6.0f;
		snprintf(out->value, sizeof(out->value), "SLOTS %u/6", g->bomb_most > g->bombs ? g->bomb_most : g->bombs);
		break;
	case A_EXTEND:
	{
		char points[20];

		out->label = "EXTRA LIFE";
		out->part = share(g->score, 600000.0);
		shown_points(points, sizeof(points), g->score >= 600000u ? 0u : 600000u - g->score);
		snprintf(out->value, sizeof(out->value), "%s TO GO", points);
		break;
	}
	case A_EVERY_SHIP:
	case A_BOTH_STYLES:
	{
		const bool ships = index == A_EVERY_SHIP;
		const unsigned done = ships ? ships_cleared : styles_cleared;
		const unsigned now = ships ? 1u << g->ship : g->laser ? 2u : 1u;

		if (stage != 0 || (done & now))
			return false;    /* (only in 1-1, with a ship or style not yet cleared with) */
		out->label = ships ? "ALL 3 SHIPS" : "BOTH STYLES";
		out->part = ((float)bits(done) + 0.5f) / (ships ? 3.0f : 2.0f);
		snprintf(out->value, sizeof(out->value), "%u/%u", bits(done), ships ? 3u : 2u);
		break;
	}
	case A_SURVIVOR:
	{
		const unsigned seconds = (unsigned)((double)g->life_frames / 57.55);

		out->label = "SURVIVOR";
		out->part = share(g->life_frames, SURVIVOR_FRAMES);
		out->state = g->life_frames >= SURVIVOR_FRAMES ? GOAL_KEPT : GOAL_GOING;
		snprintf(out->value, sizeof(out->value), "%u:%02u", seconds / 60u, seconds % 60u);
		break;
	}
	case A_HIDDEN_EXTEND:
		if (stage % 6 != 2)
			return false;
		out->label = "HIDDEN 1UP";
		out->part = 0.3f;
		snprintf(out->value, sizeof(out->value), "THIS STAGE");
		break;
	case A_AREA7:
		if (loop1)
			return false;
		out->label = "REACH 2-7";
		out->part = 0.4f + (float)(stage - 6) / 12.0f;
		snprintf(out->value, sizeof(out->value), g->in_area7 ? "HERE" : "AFTER 2-6");
		break;
	case A_HIBACHI:
	case A_HIBACHI_DOWN:
		if (!g->in_area7 || (index == A_HIBACHI_DOWN && !g->hibachi))
			return false;
		out->label = index == A_HIBACHI ? "HIBACHI" : "DOWN HIBACHI";
		out->part = index == A_HIBACHI ? 0.8f : 0.9f;
		snprintf(out->value, sizeof(out->value), "2-7");
		break;
	default:
		return false;    /* (TEAM WORK: two players) */
	}
	return true;
}
