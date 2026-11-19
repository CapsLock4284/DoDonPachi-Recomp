/**
 *    Name:        stats.c
 *    Description: lifetime statistics, kept in ddonpach.stats (stats.h)
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    The file: "; ...", "[stats]", then "name = value" for each total and record, "stageN = played cleared deaths
 *    best_bees" for each stage (1-12: 1-1 ... 2-6), "ship_X = games best_score play_frames" for each ship.
 */
#include "stats.h"
#include "game_watch.h"

#include <stdio.h>
#include <string.h>

#define STAGES      12
#define SHIPS       3
#define SAVE_EVERY  (57u * 60u)    /* frames of play between saves (a minute), besides the stages' and games' ends */
#define FRAME_USEC  17376u         /* (the board's frame: 57.55 a second) */

enum {
	T_GAMES, T_CONTINUES, T_PLAY_FRAMES, T_STAGES_CLEARED, T_LOOPS, T_ALL_CLEARS, T_LOOP_1CC, T_ALL_1CC,
	T_DEATHS, T_EXTENDS, T_BOMBS_USED, T_BEES, T_POWER_UPS,
	R_BEST_SCORE, R_BEST_HIT, R_LONGEST_LIFE, R_FURTHEST,
	COUNTERS
};

static const char *const counter_ids[COUNTERS] = {
	"games", "continues", "play_frames", "stages_cleared", "loops_cleared", "all_clears", "one_credit_loops",
	"one_credit_clears", "deaths", "extra_lives", "bombs_used", "bees", "power_ups",
	"best_score", "best_hit_combo", "longest_life_frames", "furthest_stage"
};

static struct
{
	unsigned long counter[COUNTERS];
	unsigned long stage_played[STAGES], stage_cleared[STAGES], stage_deaths[STAGES], stage_best_bees[STAGES];
	unsigned long ship_games[SHIPS], ship_best[SHIPS], ship_frames[SHIPS];
} kept;

static char stats_path[1024];
static unsigned since_save;
static int last_stage = -1;

static void at_least(unsigned long *kept_value, unsigned long value)
{
	if (value > *kept_value)
		*kept_value = value;
}

/* ---- the file ---- */

void stats_save(void)
{
	FILE *const file = stats_path[0] ? fopen(stats_path, "w") : NULL;

	if (!file)
		return;
	fputs("; DoDonPachi's lifetime statistics (arcade games only: not training, replays, cheats or save states)\n"
		"[stats]\n", file);
	for (int index = 0; index < COUNTERS; index++)
		fprintf(file, "%s = %lu\n", counter_ids[index], kept.counter[index]);
	for (int stage = 0; stage < STAGES; stage++)
		fprintf(file, "stage%d = %lu %lu %lu %lu\n", stage + 1, kept.stage_played[stage], kept.stage_cleared[stage],
			kept.stage_deaths[stage], kept.stage_best_bees[stage]);
	for (int ship = 0; ship < SHIPS; ship++)
		fprintf(file, "ship_%c = %lu %lu %lu\n", 'a' + ship, kept.ship_games[ship], kept.ship_best[ship],
			kept.ship_frames[ship]);
	fclose(file);
}

void stats_load(const char *path)
{
	char line[256];
	FILE *file;

	snprintf(stats_path, sizeof(stats_path), "%s", path);
	if (!(file = fopen(path, "r")))
		return;
	while (fgets(line, sizeof(line), file))
	{
		char id[64];
		unsigned long a, b, c, d;
		int stage;
		char ship;

		if (sscanf(line, " stage%d = %lu %lu %lu %lu", &stage, &a, &b, &c, &d) == 5 && stage >= 1 && stage <= STAGES)
		{
			kept.stage_played[stage - 1] = a;
			kept.stage_cleared[stage - 1] = b;
			kept.stage_deaths[stage - 1] = c;
			kept.stage_best_bees[stage - 1] = d;
		}
		else if (sscanf(line, " ship_%c = %lu %lu %lu", &ship, &a, &b, &c) == 4 && ship >= 'a' && ship < 'a' + SHIPS)
		{
			kept.ship_games[ship - 'a'] = a;
			kept.ship_best[ship - 'a'] = b;
			kept.ship_frames[ship - 'a'] = c;
		}
		else if (sscanf(line, " %63[a-z0-9_] = %lu", id, &a) == 2)
			for (int index = 0; index < COUNTERS; index++)
				if (!strcmp(id, counter_ids[index]))
					kept.counter[index] = a;
	}
	fclose(file);
}

/* ---- the game, watched ---- */

void stats_frame(void)
{
	const game_watch_state *const g = &game_watch;

	if (g->ended && g->counts)
		stats_save();
	if (!g->on || !g->counts)
	{
		last_stage = -1;
		return;
	}
	if (g->begun)
		kept.counter[T_GAMES]++;
	if (g->continued)
		kept.counter[T_CONTINUES]++;
	if (g->stage >= 0 && g->stage != last_stage)
	{
		kept.stage_played[g->stage]++;
		at_least(&kept.counter[R_FURTHEST], (unsigned long)g->stage + 1u);
	}
	last_stage = g->stage;
	if (g->cleared >= 0)
	{
		const int stage = g->cleared;

		kept.counter[T_STAGES_CLEARED]++;
		kept.stage_cleared[stage]++;
		at_least(&kept.stage_best_bees[stage], g->cleared_bees);
		at_least(&kept.counter[R_FURTHEST], (unsigned long)stage + 2u);
		if (stage == 5 || stage == 11)
			kept.counter[T_LOOPS]++;
		if (stage == 5 && !g->continued_ever)
			kept.counter[T_LOOP_1CC]++;
		if (stage == 11)
		{
			kept.counter[T_ALL_CLEARS]++;
			if (!g->continued_ever)
				kept.counter[T_ALL_1CC]++;
		}
		stats_save();
	}
	if (!g->playing)
		return;
	kept.counter[T_PLAY_FRAMES]++;
	if (g->begun || g->play_frames == 1u)
		kept.ship_games[g->ship]++;
	kept.ship_frames[g->ship]++;
	at_least(&kept.ship_best[g->ship], g->score);
	at_least(&kept.counter[R_BEST_SCORE], g->score);
	at_least(&kept.counter[R_BEST_HIT], g->max_hit);
	at_least(&kept.counter[R_LONGEST_LIFE], g->life_frames);
	if (g->died)
	{
		kept.counter[T_DEATHS]++;
		if (g->stage >= 0)
			kept.stage_deaths[g->stage]++;
	}
	kept.counter[T_EXTENDS] += g->extended;
	kept.counter[T_BOMBS_USED] += g->bomb_used;
	kept.counter[T_BEES] += g->bees_taken;
	kept.counter[T_POWER_UPS] += g->powered_up;
	if (++since_save >= SAVE_EVERY)
	{
		since_save = 0;
		stats_save();
	}
}

/* ---- the page ---- */

#define LINES_MOST 80

static char labels[LINES_MOST][32], values[LINES_MOST][32];
static bool headings[LINES_MOST];
static int line_count;

static void heading(const char *text)
{
	if (line_count >= LINES_MOST)
		return;
	snprintf(labels[line_count], sizeof(labels[0]), "%s", text);
	headings[line_count++] = true;
}

static void add(const char *label, const char *format, unsigned long value)
{
	if (line_count >= LINES_MOST)
		return;
	snprintf(labels[line_count], sizeof(labels[0]), "%s", label);
	snprintf(values[line_count], sizeof(values[0]), format, value);
	headings[line_count++] = false;
}

static void add_text(const char *label, const char *text)
{
	if (line_count >= LINES_MOST)
		return;
	snprintf(labels[line_count], sizeof(labels[0]), "%s", label);
	snprintf(values[line_count], sizeof(values[0]), "%s", text);
	headings[line_count++] = false;
}

static void add_time(const char *label, unsigned long frames)
{
	const unsigned long seconds = (unsigned long)((unsigned long long)frames * FRAME_USEC / 1000000u);
	char text[32];

	if (line_count >= LINES_MOST)
		return;
	if (seconds >= 3600u)
		snprintf(text, sizeof(text), "%luH %02luM", seconds / 3600u, seconds / 60u % 60u);
	else
		snprintf(text, sizeof(text), "%luM %02luS", seconds / 60u, seconds % 60u);
	snprintf(labels[line_count], sizeof(labels[0]), "%s", label);
	snprintf(values[line_count], sizeof(values[0]), "%s", text);
	headings[line_count++] = false;
}

static void build(void)
{
	line_count = 0;
	heading("TOTALS");
	add("GAMES", "%lu", kept.counter[T_GAMES]);
	add("CONTINUES", "%lu", kept.counter[T_CONTINUES]);
	add_time("PLAY TIME", kept.counter[T_PLAY_FRAMES]);
	add("STAGES CLEARED", "%lu", kept.counter[T_STAGES_CLEARED]);
	add("LOOPS CLEARED", "%lu", kept.counter[T_LOOPS]);
	add("ALL CLEARS", "%lu", kept.counter[T_ALL_CLEARS]);
	add("1CC LOOPS", "%lu", kept.counter[T_LOOP_1CC]);
	add("1CC ALL CLEARS", "%lu", kept.counter[T_ALL_1CC]);
	add("DEATHS", "%lu", kept.counter[T_DEATHS]);
	add("EXTRA LIVES", "%lu", kept.counter[T_EXTENDS]);
	add("BOMBS USED", "%lu", kept.counter[T_BOMBS_USED]);
	add("BEES TAKEN", "%lu", kept.counter[T_BEES]);
	add("POWER-UPS", "%lu", kept.counter[T_POWER_UPS]);
	heading("RECORDS");
	add("BEST SCORE", "%lu0", kept.counter[R_BEST_SCORE]);
	add("BEST HIT COMBO", "%lu", kept.counter[R_BEST_HIT]);
	add_time("LONGEST LIFE", kept.counter[R_LONGEST_LIFE]);
	if (kept.counter[R_FURTHEST] > (unsigned long)STAGES)
		add_text("FURTHEST", "ALL CLEAR");
	else if (kept.counter[R_FURTHEST])
	{
		char text[16];

		snprintf(text, sizeof(text), "%lu-%lu", (kept.counter[R_FURTHEST] - 1u) / 6u + 1u,
			(kept.counter[R_FURTHEST] - 1u) % 6u + 1u);
		add_text("FURTHEST", text);
	}
	heading("STAGES: CLEAR, DEATH, BEES");
	for (int stage = 0; stage < STAGES; stage++)
		if (kept.stage_played[stage])
		{
			char label[8], text[32];

			snprintf(label, sizeof(label), "%d-%d", stage / 6 + 1, stage % 6 + 1);
			snprintf(text, sizeof(text), "%lu/%lu  %lu  %lu", kept.stage_cleared[stage] % 100000u,
				kept.stage_played[stage] % 100000u, kept.stage_deaths[stage] % 100000u, kept.stage_best_bees[stage] % 100u);
			add_text(label, text);
		}
	heading("SHIP: GAMES, BEST SCORE");
	for (int ship = 0; ship < SHIPS; ship++)
		if (kept.ship_games[ship])
		{
			char label[32];

			snprintf(label, sizeof(label), "TYPE-%c %lu", 'A' + ship, kept.ship_games[ship] % 100000u);
			add(label, "%lu0", kept.ship_best[ship]);
		}
}

int stats_lines(void)
{
	build();
	return line_count;
}

const char *stats_label(int line)
{
	return line >= 0 && line < line_count ? labels[line] : "";
}

const char *stats_value(int line)
{
	return line >= 0 && line < line_count && !headings[line] ? values[line] : NULL;
}
