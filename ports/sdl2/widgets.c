/**
 *    Name:        widgets.c
 *    Description: the side panels (widgets.h)
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    The font is the game's text layer, as the options menu's (menu.c: rom_set.layer[2], 8x8 tiles of 8 bits a pixel,
 *    ASCII, pens 1-5 a shade, 15 the outline), here also twice as large. Every panel is laid out for any width from
 *    WIDGET_W to WIDGET_MAX_W: text against the panel's sides or centred, bars and the graph as wide as it is.
 *    The stages' file: "DDSTGB01", then 12 scores (little-endian longs, as the game counts the score: without its last
 *    digit), area 1-1 ... 1-6, 2-1 ... 2-6; 0 none yet.
 */
#include "widgets.h"
#include "file_save.h"
#include "paths.h"

#include "cave/board.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define BACKGROUND  0x0A0A14u
#define HEADER      0x14142Au
#define FRAME       0x2A2A44u
#define GOLD        0xFFC83Cu
#define WHITE       0xF0F0F0u
#define GREY        0x8C8C9Cu
#define DIM         0x40404Cu
#define ORANGE      0xFF7A1Eu
#define CYAN        0x6CD8FFu
#define GREEN       0x5CE05Cu
#define YELLOW      0xFFE040u
#define RED         0xFF4040u
#define MAGENTA     0xFF5CE0u
#define FILL        0x2E4A82u   /* the graph's area under the score */

/* the high score table's places as the graph marks them: #1 ... #5 */
static const uint32_t rank_colours[5] = { GOLD, MAGENTA, ORANGE, GREEN, CYAN };

/* ---- the game's memory ---- */

#define SCORE       0x10161Eu   /* ($101622 player 2's) */
#define TABLE       0x1016EAu
#define NO_GAME     0x1017A5u   /* bit 7: the attract mode or the title */
#define GAME_MODE   0x1017A6u   /* 0xB the ship select, 7 playing, 8 the continue's count down (4 the ranking after a
                                   game over, and the title: a game can start from there, the ship select next, without
                                   the attract mode between) */
#define SHIP        0x101754u   /* ($101758 player 2's) */
#define STYLE       0x101756u   /* ($10175A) */
#define RECORD      0x102C8Cu   /* (+$40 player 2's: the power and bombs below too) */
#define OWN_POWER   0x102CACu
#define OTHER_POWER 0x102CAEu
#define BOMBS       0x102CB0u
#define STAGE       0x101978u
#define LOOP        0x10197Au
#define BEES_TAKEN  0x103F8Eu  /* the bees taken since the stage began or the last miss (09:4628 adds one; the game
                                   sets it back to 0 at a miss and at the stage's end: the panel counts the rises) */
#define BEES        13          /* in each stage */
#define PLAYER_STEP 0x40u
#define MAX_POWER   4u
#define STAGES      6           /* in a loop */
#define AREAS       12

static unsigned byte_at(uint32_t address)
{
	return *cave_work_ram_byte(address);
}

static unsigned word(uint32_t address)
{
	return byte_at(address) << 8 | byte_at(address + 1u);
}

static uint32_t bcd_long(uint32_t address)
{
	uint32_t value = 0;

	for (uint32_t at = 0; at < 4u; at++)
	{
		const unsigned bcd = byte_at(address + at);

		value = value * 100u + (bcd >> 4) * 10u + (bcd & 15u);
	}
	return value;
}

static bool attract_seen;      /* the attract mode, the title or the ship select seen since the reset: before it, the
                                   game's memory is still being set up (its warning) */

/* a game being played: from the ship select to its last game over (its continues' count downs in it) */
static bool in_game(void)
{
	const unsigned mode = word(GAME_MODE);

	return attract_seen && (mode == 7u || mode == 8u);
}

static uint32_t score_of(int player)
{
	return bcd_long(SCORE + 4u * (unsigned)player);
}

static bool player_in(int player)
{
	return (word(RECORD + PLAYER_STEP * (unsigned)player) & 0x8000u) || score_of(player) != 0;
}

/* the player the panels follow: player 1, or player 2 playing alone */
static int followed(void)
{
	return !player_in(0) && player_in(1) ? 1 : 0;
}

/* the area being played, 0-11 (loop * 6 + stage), or -1 (between the loops, the ending) */
static int area_now(void)
{
	const unsigned stage = word(STAGE), loop = word(LOOP);

	return stage < STAGES && loop < 2u ? (int)(loop * STAGES + stage) : -1;
}

/* ---- the board's speed: its frames, and those of slowdown (the game's frame not done in time) ---- */

#define SECOND_FRAMES 58        /* the board's frames in about a second (57.55 a second) */

static struct
{
	bool lagged[SECOND_FRAMES];
	int at, filled, lagged_now;
	unsigned stage_frames, stage_lagged, game_frames, game_lagged;
} speed;

static void speed_frame(bool lagged, bool counting)
{
	if (speed.filled == SECOND_FRAMES)
		speed.lagged_now -= speed.lagged[speed.at];
	else
		speed.filled++;
	speed.lagged[speed.at] = lagged;
	speed.lagged_now += lagged;
	speed.at = (speed.at + 1) % SECOND_FRAMES;
	if (counting)
	{
		speed.stage_frames++;
		speed.stage_lagged += lagged;
		speed.game_frames++;
		speed.game_lagged += lagged;
	}
}

/* ---- the enemies' bullets on screen, and the last seconds of them with the slowdown ---- */

#define BULLETS_AT    0x106CB6u   /* the bullets' records (func_0057EE tests them against the ship): +0 bit 15 in play */
#define BULLET_SLOTS  245         /* ($40 each, to $10A9F6; loop 1 tests the first 225, loop 2 all) */
#define HISTORY       288         /* the board's frames the graph shows: 5 seconds */

static struct
{
	uint8_t bullets[HISTORY];
	bool lagged[HISTORY];
	int at, filled, now;
} history;

static int bullets_now(void)
{
	int count = 0;

	for (uint32_t slot = 0; slot < BULLET_SLOTS; slot++)
		if (*(const uint16_t *)cave_work_ram_at(BULLETS_AT + slot * 0x40u, 2) & 0x8000u)
			count++;
	return count;
}

/* a board frame: its bullets (a frame of slowdown: the last count, the game's memory being half way through its
   frame) and whether it was slowed */
static void history_frame(bool lagged)
{
	if (!lagged)
		history.now = bullets_now();
	history.bullets[history.at] = (uint8_t)history.now;
	history.lagged[history.at] = lagged;
	history.at = (history.at + 1) % HISTORY;
	if (history.filled < HISTORY)
		history.filled++;
}

/* ---- the stages' best scores (the file) ---- */

static const char stages_magic[8] = { 'D', 'D', 'S', 'T', 'G', 'B', '0', '1' };
static char stages_path[1024];
static uint32_t best[AREAS];

const char *widgets_open(const char *path)
{
	FILE *file;
	uint8_t data[sizeof(stages_magic) + 4 * AREAS];

	memset(best, 0, sizeof(best));
	if (path)
		snprintf(stages_path, sizeof(stages_path), "%s", path);
	else
		snprintf(stages_path, sizeof(stages_path), "%sddonpach.stages", paths_folder());
	if (!(file = fopen(stages_path, "rb")))
		return NULL;
	if (fread(data, 1, sizeof(data), file) != sizeof(data) || memcmp(data, stages_magic, sizeof(stages_magic)))
	{
		fclose(file);
		return "not a DoDonPachi stage scores file: started again (and saved over it)";
	}
	fclose(file);
	for (int area = 0; area < AREAS; area++)
	{
		const uint8_t *const at = data + sizeof(stages_magic) + 4 * area;

		best[area] = (uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24;
	}
	return NULL;
}

static void stages_save(void)
{
	uint8_t data[sizeof(stages_magic) + 4 * AREAS];

	memcpy(data, stages_magic, sizeof(stages_magic));
	for (int area = 0; area < AREAS; area++)
	{
		uint8_t *const at = data + sizeof(stages_magic) + 4 * area;

		at[0] = (uint8_t)best[area];
		at[1] = (uint8_t)(best[area] >> 8);
		at[2] = (uint8_t)(best[area] >> 16);
		at[3] = (uint8_t)(best[area] >> 24);
	}
	file_save(stages_path, data, sizeof(data));    /* (by file_save's helper: the frame doesn't wait for the disk) */
}

/* ---- the run: the stage being played, the graph, the high scores chased ---- */

#define POINTS  512             /* the graph's points kept (two become one when it is full) */
#define MARKS   32

static struct
{
	bool on;                    /* a game seen from its start (the panels show the last one through the attract) */
	int player;
	/* the stage */
	int area;                   /* -1: none */
	uint32_t stage_start;       /* the score as it began */
	bool stage_counts;          /* clean all through, from its start, no continue */
	/* each stage finished in this game: its score, the best before it, whether it counted (and was a new best) */
	bool done[AREAS], counted[AREAS], new_best[AREAS];
	/* the bees taken in each stage (the panel shows the one being played, or the last played) */
	unsigned bees[AREAS], last_bees;
	int bee_area;
	uint32_t made[AREAS], best_before[AREAS];
	/* the graph */
	uint32_t table[5];          /* the high score table as the game began */
	int chasing;                /* the place chased: 4 (#5) ... 0 (#1), -1 all passed */
	uint32_t points[POINTS];
	int count;
	unsigned interval, frames;  /* the game's frames a point, and those since the last */
	int passed_at[5];           /* the point where each place was passed (-1 not yet) */
	int stage_marks[MARKS], mark_count;
	uint32_t last_score_seen;
} run;

static bool was_in_game;

void widgets_game_begun(void)
{
	attract_seen = true;
	was_in_game = false;
}

void widgets_reset(void)
{
	memset(&run, 0, sizeof(run));
	run.area = -1;
	for (int place = 0; place < 5; place++)
		run.passed_at[place] = -1;
	memset(&speed, 0, sizeof(speed));
	attract_seen = was_in_game = false;
}

static void run_begin(void)
{
	memset(&run, 0, sizeof(run));
	run.area = -1;
	speed.stage_frames = speed.stage_lagged = speed.game_frames = speed.game_lagged = 0;
	run.on = true;
	run.player = followed();
	for (int place = 0; place < 5; place++)
	{
		run.table[place] = bcd_long(TABLE + 4u * (unsigned)place);
		run.passed_at[place] = -1;
	}
	run.chasing = 4;
	run.interval = 4;
	run.last_bees = word(BEES_TAKEN);
	run.bee_area = -1;
}

static void graph_point(uint32_t score)
{
	if (run.count == POINTS)
	{
		for (int at = 0; at < POINTS / 2; at++)
			run.points[at] = run.points[2 * at + 1];
		run.count = POINTS / 2;
		run.interval *= 2;
		for (int place = 0; place < 5; place++)
			if (run.passed_at[place] >= 0)
				run.passed_at[place] /= 2;
		for (int mark = 0; mark < run.mark_count; mark++)
			run.stage_marks[mark] /= 2;
	}
	run.points[run.count++] = score;
}

static void stage_begin(int area, uint32_t score, bool clean)
{
	run.area = area;
	run.stage_start = score;
	run.stage_counts = clean;
	speed.stage_frames = speed.stage_lagged = 0;
	if (run.count > 0 && run.mark_count < MARKS)
		run.stage_marks[run.mark_count++] = run.count;
}

static void stage_end(uint32_t score)
{
	const int area = run.area;
	const uint32_t made = score - run.stage_start;

	run.done[area] = true;
	run.made[area] = made;
	run.best_before[area] = best[area];
	run.counted[area] = run.stage_counts;
	run.new_best[area] = run.stage_counts && made > best[area];
	if (run.new_best[area])
	{
		best[area] = made;
		stages_save();
	}
}

/* ---- an achievement earned: a box over the right panel's top ---- */

#define TOAST_FRAMES 230    /* 4 seconds */

static char toast_name[32];
static int toast_frames, toast_more;

void widgets_toast(const char *name, int more)
{
	snprintf(toast_name, sizeof(toast_name), "%s", name);
	toast_frames = TOAST_FRAMES;
	toast_more = more;
}

bool widgets_toast_busy(void)
{
	return toast_frames > 0;
}

void widgets_frame(bool clean)
{
	if (toast_frames > 0)
		toast_frames--;
	const bool lagged = program_frame_lagged();

	if (!lagged && ((byte_at(NO_GAME) & 0x80u) || word(GAME_MODE) == 0xBu))
		attract_seen = true;
	if (!lagged && word(GAME_MODE) == 0xBu)
		was_in_game = false;    /* (the ship select: a new game, its own run, whether the attract mode came first or not) */
	history_frame(lagged);
	if (!in_game())
	{
		/* (the last game's panels stay through the attract mode) */
		was_in_game = false;
		run.area = -1;
		speed_frame(lagged, false);
		return;
	}
	if (!was_in_game)
		run_begin();    /* (a game begins) */
	was_in_game = true;
	speed_frame(lagged, true);
	if (lagged)
		return;    /* (the game's memory is half way through its frame: looked at again next) */
	{
		const int player = run.player = followed();
		const uint32_t score = score_of(player);
		const int area = area_now();

		if (score < run.last_score_seen)
		{
			/* a continue (the score starts again from 0): this stage doesn't count, and is measured from here */
			run.stage_start = score;
			run.stage_counts = false;
		}
		run.last_score_seen = score;
		if (!clean)
			run.stage_counts = false;
		if (area != run.area)
		{
			if (run.area >= 0 && (area == run.area + 1 || area == -1))
				stage_end(score);    /* (finished: on to the next, or between the loops, or the ending) */
			if (area >= 0)
				stage_begin(area, score, clean);
			else
				run.area = -1;
		}
		{
			const unsigned bees = word(BEES_TAKEN);

			if (area >= 0)
			{
				if (bees > run.last_bees && run.bees[area] + (bees - run.last_bees) <= BEES)
					run.bees[area] += bees - run.last_bees;
				run.bee_area = area;
			}
			run.last_bees = bees;
		}
		while (run.chasing >= 0 && score > run.table[run.chasing])
			run.passed_at[run.chasing--] = run.count;
		if (++run.frames >= run.interval)
		{
			run.frames = 0;
			graph_point(score);
		}
	}
}

/* ---- drawing on a panel ---- */

typedef struct
{
	uint32_t *pixels;
	int w;
} panel;

static void dot(panel *p, int x, int y, uint32_t colour)
{
	if (x >= 0 && x < p->w && y >= 0 && y < WIDGET_H)
		p->pixels[y * p->w + x] = colour;
}

static void box(panel *p, int x0, int y0, int x1, int y1, uint32_t colour)
{
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++)
			dot(p, x, y, colour);
}

static void outline(panel *p, int x0, int y0, int x1, int y1, uint32_t colour)
{
	box(p, x0, y0, x1, y0 + 1, colour);
	box(p, x0, y1 - 1, x1, y1, colour);
	box(p, x0, y0, x0 + 1, y1, colour);
	box(p, x1 - 1, y0, x1, y1, colour);
}

static uint32_t shade(uint32_t colour, int percent)
{
	return ((colour >> 16 & 255u) * (uint32_t)percent / 100u) << 16 | ((colour >> 8 & 255u) * (uint32_t)percent / 100u) << 8
		| (colour & 255u) * (uint32_t)percent / 100u;
}

/* a tile's pen at (tx, ty): as menu.c reads the layer */
static unsigned tile_pen(const uint8_t *tile, int tx, int ty)
{
	const uint8_t *const pair = &tile[ty * 8 + (tx >> 1) * 2];
	const unsigned shift = (tx & 1) ? 0u : 4u;

	return (unsigned)((pair[1] >> shift & 15u) << 4 | (pair[0] >> shift & 15u));
}

static void put_char(panel *p, int x, int y, char ch, uint32_t colour, int size)
{
	static const int pens[16] = { 0, 100, 92, 84, 76, 68, 60, 60, 60, 60, 60, 60, 60, 60, 60, -1 };
	const int code = toupper((unsigned char)ch);
	const uint8_t *tile;

	if (code <= 0x20 || code >= 0x60 || !rom_set.layer[2])
		return;
	tile = rom_set.layer[2] + code * 64;
	for (int i = 0; i < 8; i++)          /* (the tiles lie turned: a row is a column) */
		for (int j = 0; j < 8; j++)
		{
			const unsigned pen = tile_pen(tile, 7 - j, i);

			if (pen)
				box(p, x + i * size, y + j * size, x + (i + 1) * size, y + (j + 1) * size,
					pen >= 16 ? shade(colour, 60) : pens[pen] < 0 ? 0x101018u : shade(colour, pens[pen]));
		}
}

static int text_width(const char *text, int size)
{
	return (int)strlen(text) * 8 * size;
}

static void put_text(panel *p, int x, int y, const char *text, uint32_t colour, int size)
{
	for (; *text; text++, x += 8 * size)
		put_char(p, x, y, *text, colour, size);
}

static void put_centre(panel *p, int y, const char *text, uint32_t colour, int size)
{
	put_text(p, (p->w - text_width(text, size)) / 2, y, text, colour, size);
}

static void put_right(panel *p, int y, const char *text, uint32_t colour, int size)
{
	put_text(p, p->w - 8 - text_width(text, size), y, text, colour, size);
}

static void line(panel *p, int x0, int y0, int x1, int y1, uint32_t colour)
{
	const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
	const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
	int error = dx - dy;

	for (;;)
	{
		dot(p, x0, y0, colour);
		if (x0 == x1 && y0 == y1)
			break;
		if (2 * error > -dy)
		{
			error -= dy;
			x0 += sx;
		}
		if (2 * error < dx)
		{
			error += dx;
			y0 += sy;
		}
	}
}

/* a score as the game shows it (its last digit, the continues, left as 0), with commas when they fit in 'room' */
static void score_text(char *out, size_t size, uint32_t value, int room)
{
	char digits[16], grouped[24];
	int length, at = 0;

	snprintf(digits, sizeof(digits), "%lu0", (unsigned long)value);
	if (value == 0)
		snprintf(digits, sizeof(digits), "0");
	length = (int)strlen(digits);
	for (int i = 0; i < length; i++)
	{
		grouped[at++] = digits[i];
		if ((length - 1 - i) % 3 == 0 && i < length - 1)
			grouped[at++] = ',';
	}
	grouped[at] = '\0';
	snprintf(out, size, "%s", text_width(grouped, 1) <= room ? grouped : digits);
}

static void frame_panel(panel *p, const char *title)
{
	box(p, 0, 0, p->w, WIDGET_H, BACKGROUND);
	outline(p, 0, 0, p->w, WIDGET_H, FRAME);
	box(p, 1, 1, p->w - 1, 15, HEADER);
	put_centre(p, 4, title, GOLD, 1);
}

static void section(panel *p, int y, const char *title)
{
	box(p, 8, y + 3, p->w - 8, y + 4, FRAME);
	box(p, (p->w - text_width(title, 1)) / 2 - 4, y, (p->w + text_width(title, 1)) / 2 + 4, y + 8, BACKGROUND);
	put_centre(p, y, title, GREY, 1);
}

/* 'count' cells across the panel, 'filled' of them lit */
static void pips(panel *p, int y, int height, int count, int filled, uint32_t colour, uint32_t last_colour)
{
	const int room = p->w - 16, gap = 3, cell = (room - gap * (count - 1)) / count;
	const int left = 8 + (room - (cell * count + gap * (count - 1))) / 2;

	for (int at = 0; at < count; at++)
	{
		const int x = left + at * (cell + gap);

		if (at < filled)
			box(p, x, y, x + cell, y + height, at == count - 1 ? last_colour : colour);
		else
			outline(p, x, y, x + cell, y + height, DIM);
	}
}

/* ---- the left panel: the player, the board's speed ---- */

static void draw_power(panel *p, int y, const char *name, unsigned level, bool showing)
{
	char text[8];

	put_text(p, 8, y, name, showing ? WHITE : DIM, 1);
	if (level >= MAX_POWER)
		snprintf(text, sizeof(text), "MAX");
	else
		snprintf(text, sizeof(text), "LV %u", level + 1u);
	put_right(p, y, text, !showing ? DIM : level >= MAX_POWER ? GOLD : WHITE, 1);
	pips(p, y + 10, 6, (int)MAX_POWER + 1, showing ? (int)level + 1 : 0, ORANGE, GOLD);
}

static void draw_percent(panel *p, int y, const char *name, unsigned lagged, unsigned frames)
{
	char text[16];

	put_text(p, 8, y, name, GREY, 1);
	if (frames)
		snprintf(text, sizeof(text), "%u%%", (lagged * 100u + frames / 2u) / frames);
	else
		snprintf(text, sizeof(text), "-");
	put_right(p, y, text, WHITE, 1);
}

static int draw_bees(panel *p);
static void draw_input(panel *p, int top);

/* The last 5 seconds (the newest at the right): a bar for the bullets on screen, red where the board slowed down
   (gold where it didn't), against the most there were in that time (40 at least) */
#define HISTORY_HEIGHT 15

static void draw_history(panel *p, int top)
{
	const int columns = p->w - 18;
	int most = 40;

	outline(p, 8, top, p->w - 8, top + HISTORY_HEIGHT, FRAME);
	for (int age = 0; age < history.filled; age++)
	{
		const int value = history.bullets[(history.at - 1 - age + HISTORY) % HISTORY];

		if (value > most)
			most = value;
	}
	most = (most + 19) / 20 * 20;
	for (int column = 0; column < columns; column++)
	{
		/* (the frames this column stands for: the oldest at the left) */
		const int first = column * HISTORY / columns, last = (column + 1) * HISTORY / columns;
		int bullets = 0, height;
		bool lagged = false, seen = false;

		for (int frame = first; frame < last; frame++)
		{
			const int age = HISTORY - 1 - frame;

			if (age >= history.filled)
				continue;
			{
				const int at = (history.at - 1 - age + HISTORY) % HISTORY;

				seen = true;
				if (history.bullets[at] > bullets)
					bullets = history.bullets[at];
				lagged = lagged || history.lagged[at];
			}
		}
		if (!seen)
			continue;
		height = (bullets * (HISTORY_HEIGHT - 2) + most - 1) / most;
		if (lagged && height < 2)
			height = 2;    /* (a slowed frame shows, whatever its bullets) */
		box(p, 9 + column, top + HISTORY_HEIGHT - 1 - height, 10 + column, top + HISTORY_HEIGHT - 1,
			lagged ? RED : shade(GOLD, 70));
	}
}

static void draw_left(panel *p)
{
	static const char *const ships[3] = { "TYPE-A", "TYPE-B", "TYPE-C" };
	static const uint32_t ship_colours[3] = { RED, CYAN, GREEN };
	const bool showing = run.on && in_game();
	const unsigned step = PLAYER_STEP * (unsigned)run.player;
	const unsigned ship = word(SHIP + 4u * (unsigned)run.player) % 3u;
	const bool laser = word(STYLE + 4u * (unsigned)run.player) == 1u;
	const unsigned own = word(OWN_POWER + step), other = word(OTHER_POWER + step);
	const unsigned bombs = byte_at(BOMBS + step), most = byte_at(BOMBS + step + 1u);
	char text[32];

	frame_panel(p, run.player ? "2P" : "1P");
	/* the ship and its style */
	put_text(p, 8, 22, showing ? ships[ship] : "------", showing ? ship_colours[ship] : DIM, 1);
	put_right(p, 22, showing ? (laser ? "LASER" : "SHOT") : "", WHITE, 1);
	/* the power: the style's own weapon first */
	section(p, 40, "POWER");
	draw_power(p, 54, laser ? "LASER" : "SHOT", showing ? own : 0u, showing);
	draw_power(p, 78, laser ? "SHOT" : "LASER", showing ? other : 0u, showing);
	/* the bombs */
	put_text(p, 8, 102, "BOMBS", showing ? WHITE : DIM, 1);
	snprintf(text, sizeof(text), "%u/%u", showing ? bombs : 0u, showing ? most : 0u);
	put_right(p, 102, text, showing ? WHITE : DIM, 1);
	pips(p, 112, 6, most >= 3u && most <= 6u ? (int)most : 3, showing ? (int)bombs : 0, YELLOW, YELLOW);

	/* the board's speed: the game's frames a second now (the board's 57.55, less its frames of slowdown in the last
	   second), and the slowdown now, this stage and this game */
	section(p, 134, "BOARD");
	{
		const double rate = 1000000.0 / (double)CAVE_FRAME_USEC;
		const int frames = speed.filled ? speed.filled : 1;
		const double game_rate = rate * (double)(frames - speed.lagged_now) / (double)frames;
		const unsigned percent = (unsigned)(speed.lagged_now * 100 + frames / 2) / (unsigned)frames;

		snprintf(text, sizeof(text), "%.1f", game_rate);
		put_text(p, 8, 148, text, percent == 0 ? GREEN : percent < 25 ? YELLOW : RED, 2);
		put_text(p, 8 + text_width(text, 2) + 4, 156, "FPS", GREY, 1);
		{
			/* (the board's own rate beside it, where the panel is wide enough) */
			const int used = 8 + text_width(text, 2) + 4 + text_width("FPS", 1);

			snprintf(text, sizeof(text), "OF %.2f", rate);
			if (used + 8 + text_width(text, 1) <= p->w - 8)
				put_right(p, 156, text, DIM, 1);
		}
		put_text(p, 8, 168, "SLOWDOWN", GREY, 1);
		snprintf(text, sizeof(text), "%u%%", percent);
		put_right(p, 168, text, percent == 0 ? WHITE : percent < 25 ? YELLOW : RED, 1);
		put_text(p, 8, 180, "BULLETS", GREY, 1);
		snprintf(text, sizeof(text), "%d", history.now);
		put_right(p, 180, text, WHITE, 1);
		draw_history(p, 191);
		draw_percent(p, 210, "THIS STAGE", showing ? speed.stage_lagged : 0u, showing ? speed.stage_frames : 0u);
		draw_percent(p, 222, "THIS GAME", run.on ? speed.game_lagged : 0u, run.on ? speed.game_frames : 0u);
	}
	draw_input(p, draw_bees(p) + 3);
}

/* ---- the bees: the stage's 13, each greyed until it is taken ---- */

#define BEE_CODE    0x4496u     /* the bee's sprite (a frame of its flight: 32 x 32, at the code's 256 pixels on) */
#define BEE_COLOUR  0x17u       /* its palette (the sprite's attribute as shown, $1730) */
#define BEE_SIZE    32
#define BEE_ICON    16          /* drawn at half size */

static unsigned bee_pen(int x, int y)
{
	const uint32_t pixel = (BEE_CODE << 8) + (uint32_t)(y * BEE_SIZE + x);
	const uint8_t byte = rom_set.sprites[(pixel >> 1) & (rom_set_region_size(CAVE_ROM_SPRITES) - 1u)];

	return (pixel & 1u) ? (byte & 15u) : (byte >> 4);    /* (DoDonPachi's sprites: high nibble first) */
}

/* xGRB 555, as the board's palette has it, to 0x00RRGGBB */
static uint32_t palette_colour(unsigned pen)
{
	const uint16_t value = cave_board.palette[pen & 0x7FFFu];
	const uint32_t g = value >> 10 & 31u, r = value >> 5 & 31u, b = value & 31u;

	return (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

/* a bee at (x, y): 2 x 2 of the sprite's pixels to each of the icon's, their colours averaged, turned as the picture
   is (the board's screen lies on its side); a pixel with fewer than two of the bee's left clear. Taken: its own
   colours; not yet: grey, dim */
static void draw_bee(panel *p, int x, int y, bool taken)
{
	if (!rom_set.sprites)
		return;
	for (int row = 0; row < BEE_ICON; row++)
		for (int column = 0; column < BEE_ICON; column++)
		{
			unsigned r = 0, g = 0, b = 0, count = 0;

			for (int sy = 0; sy < 2; sy++)
				for (int sx = 0; sx < 2; sx++)
				{
					/* (the picture's (column, row) is the sprite's (31 - row, column), at twice the size) */
					const unsigned pen = bee_pen((BEE_ICON - 1 - row) * 2 + sx, column * 2 + sy);

					if (pen)
					{
						/* (its palette, or before the game has set it up (the main menu at the start) gold made
						   here, brighter for the lower pens as the bee's palette goes) */
						const uint32_t colour = cave_board.palette[BEE_COLOUR * 256u + 2u]
							? palette_colour(BEE_COLOUR * 256u + pen)
							: pen == 1u ? 0xFFFFFFu : pen < 12u ? shade(GOLD, 110 - (int)pen * 9)
							: shade(0xC0E000u, 130 - (int)pen * 8);

						r += colour >> 16 & 255u;
						g += colour >> 8 & 255u;
						b += colour & 255u;
						count++;
					}
				}
			if (count < 2)
				continue;
			r /= count;
			g /= count;
			b /= count;
			if (!taken)
			{
				const unsigned grey = (r * 30u + g * 59u + b * 11u) / 100u * 45u / 100u + 18u;

				r = g = b = grey;
			}
			dot(p, x + column, y + row, r << 16 | g << 8 | b);
		}
}

#define BEES_TOP    234

/* (the bees' section; its foot, for the controls under it) */
static int draw_bees(panel *p)
{
	const unsigned taken = run.on && run.bee_area >= 0 ? run.bees[run.bee_area] : 0u;
	/* (as many to a row as fit, 14 pixels apart at least: the bee leaves the sides of its icon clear) */
	const int room = p->w - 16, per_row = room / 14 < BEES ? room / 14 : BEES;
	const int step = room / per_row < BEE_ICON + 2 ? room / per_row : BEE_ICON + 2;
	const int rows = (BEES + per_row - 1) / per_row;
	char text[16];

	section(p, BEES_TOP, "BEES");
	snprintf(text, sizeof(text), "%u/%d", taken, BEES);
	put_right(p, BEES_TOP + 12, text, taken == BEES ? GOLD : run.on ? WHITE : DIM, 1);
	if (run.on && run.bee_area >= 0)
	{
		snprintf(text, sizeof(text), "%d-%d", run.bee_area / STAGES + 1, run.bee_area % STAGES + 1);
		put_text(p, 8, BEES_TOP + 12, text, GREY, 1);
	}
	for (int bee = 0; bee < BEES; bee++)
	{
		const int row = bee / per_row, column = bee % per_row;
		const int in_row = row == rows - 1 ? BEES - row * per_row : per_row;
		const int left = 8 + (room - ((in_row - 1) * step + BEE_ICON)) / 2;    /* (each row centred) */

		draw_bee(p, left + column * step, BEES_TOP + 22 + row * BEE_ICON, (unsigned)bee < taken);
	}
	return BEES_TOP + 22 + rows * BEE_ICON;
}

/* ---- the controls (as DonPachi Recomp's): what the board is given, the stick and the three buttons ---- */

#define INPUT_CELL  8           /* the stick's cells, 8 apart (7 wide: a pixel between them) */

static void draw_input(panel *p, int top)
{
	const bool showing = run.on && in_game();
	const uint16_t held = (uint16_t)~(run.player ? cave_board.in1 : cave_board.in0);
	const uint32_t frame = showing ? GREY : DIM;
	/* the stick: a 3 x 3 of cells, the one it points to lit */
	const int dx = held & CAVE_LEFT ? -1 : held & CAVE_RIGHT ? 1 : 0, dy = held & CAVE_UP ? -1 : held & CAVE_DOWN ? 1 : 0;
	static const struct { uint16_t bit; char name; } buttons[3] = {
		{ CAVE_BUTTON1, 'S' }, { CAVE_BUTTON2, 'B' }, { CAVE_BUTTON3, 'C' }
	};
	/* (the stick and the buttons together centred: 23 + 10 + 3 x 20 + 2 x 4 pixels) */
	const int left = (p->w - 101) / 2;

	box(p, 8, top, p->w - 8, top + 1, FRAME);
	for (int row = -1; row <= 1; row++)
		for (int column = -1; column <= 1; column++)
		{
			const int x = left + (column + 1) * INPUT_CELL, y = top + 4 + (row + 1) * INPUT_CELL;

			if (showing && row == dy && column == dx)
				box(p, x, y, x + INPUT_CELL - 1, y + INPUT_CELL - 1, row == 0 && column == 0 ? GREY : GOLD);
			else
				outline(p, x, y, x + INPUT_CELL - 1, y + INPUT_CELL - 1, row == 0 && column == 0 ? DIM : frame);
		}
	for (int button = 0; button < 3; button++)
	{
		const int x = left + 33 + button * 24, y = top + 9;
		const bool on = showing && (held & buttons[button].bit);

		if (on)
			box(p, x, y, x + 20, y + 13, GOLD);
		else
			outline(p, x, y, x + 20, y + 13, frame);
		put_char(p, x + 6, y + 3, buttons[button].name, on ? 0x10101Cu : frame, 1);
	}
}

/* ---- the right panel: the score chasing the high scores, its graph, this stage ---- */

#define GRAPH_TOP    100         /* (under the next extra life's line) */
#define GRAPH_HEIGHT 78

/* the score of the player's next extend, as the score is kept (game_memory.h: $101950 / $101954, BCD; func_044BBE
   gives the life when the score reaches it), or 0 when there are none left ($FFFF) */
#define NEXT_EXTEND 0x101950u
#define EXTEND_COLOUR 0xC8FFC8u    /* (pale: the places' colours are the graph's other lines) */

static uint32_t next_extend(int player)
{
	const uint32_t at = NEXT_EXTEND + 4u * (uint32_t)player;

	return word(at) == 0xFFFFu ? 0u : bcd_long(at);
}

static void draw_graph(panel *p, uint32_t score)
{
	const int left = 8, width = p->w - 16, bottom = GRAPH_TOP + GRAPH_HEIGHT;
	const int count = run.count;
	uint32_t top = run.chasing >= 0 ? run.table[run.chasing] : score;

	if (!run.on)
	{
		box(p, left, GRAPH_TOP, left + width, bottom, 0x000000u);
		outline(p, left - 1, GRAPH_TOP - 1, left + width + 1, bottom + 1, FRAME);
		return;
	}

	for (int at = 0; at < count; at++)
		if (run.points[at] > top)
			top = run.points[at];
	if (top == 0)
		top = 1;
	box(p, left, GRAPH_TOP, left + width, bottom, 0x000000u);
	outline(p, left - 1, GRAPH_TOP - 1, left + width + 1, bottom + 1, FRAME);
	/* (the points: one a column while they fit, then the columns share them out) */
	{
		int previous_y = -1;

		for (int column = 0; column < width; column++)
		{
			const int at = count <= width ? column : (int)((long)column * count / width);
			int height, y;

			if (at >= count)
				break;
			height = (int)((double)run.points[at] / (double)top * (GRAPH_HEIGHT - 1));
			y = bottom - 1 - height;
			box(p, left + column, y, left + column + 1, bottom, FILL);
			if (previous_y >= 0)
				line(p, left + column - 1, previous_y, left + column, y, WHITE);
			previous_y = y;
		}
	}
	/* the stages' beginnings: faint marks along the bottom */
	for (int mark = 0; mark < run.mark_count; mark++)
	{
		const int column = count <= width ? run.stage_marks[mark] : (int)((long)run.stage_marks[mark] * width / count);

		box(p, left + column, bottom - 4, left + column + 1, bottom, GREY);
	}
	/* the places: the one chased as a dashed line at its score (the graph's top), those passed as their level
	   (dotted) and a line in their colour where they were passed */
	for (int place = 4; place >= 0; place--)
	{
		const uint32_t colour = rank_colours[place];
		const int y = bottom - 1 - (int)((double)run.table[place] / (double)top * (GRAPH_HEIGHT - 1));
		if (run.passed_at[place] >= 0)
		{
			const int column = count <= width ? run.passed_at[place] : (int)((long)run.passed_at[place] * width / count);

			for (int x = left; x < left + width; x += 4)
				dot(p, x, y, shade(colour, 70));
			box(p, left + column, GRAPH_TOP, left + column + 1, bottom, colour);
			/* (its number beside the line, each place on a row of its own: passed together, they don't cover each
			   other) */
			put_char(p, left + column + 2 > left + width - 8 ? left + column - 9 : left + column + 2,
				GRAPH_TOP + 1 + 9 * (4 - place), (char)('1' + place), colour, 1);
		}
		else if (place == run.chasing)
			for (int x = left; x < left + width; x += 6)
				box(p, x, y, x + 3, y + 1, colour);
	}
	/* the next extra life's score, dotted, when it is under the graph's top */
	{
		const uint32_t extend = next_extend(run.player);

		if (extend != 0 && extend > score && extend <= top)
		{
			const int y = bottom - 1 - (int)((double)extend / (double)top * (GRAPH_HEIGHT - 1));

			for (int x = left + 1; x < left + width; x += 3)
				dot(p, x, y, EXTEND_COLOUR);
		}
	}
}

/* a difference against a best: "+N" or "-N" */
static void difference(char *out, size_t size, uint32_t made, uint32_t against, int room)
{
	char value[32];

	score_text(value, sizeof(value), made > against ? made - against : against - made, room - 8);
	snprintf(out, size, "%c%s", made > against ? '+' : '-', value);
}

/* the stages of the loop being played, one under the other: each finished one's score and how it went against the
   best for that stage before it (a new best in gold), the one being played as it goes (against its best so far),
   those to come dim */
#define STAGES_TOP  184
#define STAGE_ROWS  19

static void draw_stages(panel *p, uint32_t score)
{
	const int room = p->w - 16;
	const int loop = run.on && run.area >= 0 ? run.area / STAGES : run.on && run.done[STAGES] ? 1 : 0;

	section(p, STAGES_TOP, "STAGES");
	for (int stage = 0; stage < STAGES; stage++)
	{
		const int area = loop * STAGES + stage, y = STAGES_TOP + 12 + stage * STAGE_ROWS;
		const bool live = run.on && area == run.area && in_game();
		char label[8], value[32], text[40];

		snprintf(label, sizeof(label), "%d-%d", loop + 1, stage + 1);
		if (live)
		{
			/* the stage being played: its score so far, against its best */
			const uint32_t made = score - run.stage_start;

			put_text(p, 8, y, label, YELLOW, 1);
			score_text(value, sizeof(value), made, room - 32);
			put_right(p, y, value, WHITE, 1);
			if (!run.stage_counts)
				put_right(p, y + 9, "UNRANKED", DIM, 1);
			else if (best[area])
			{
				difference(text, sizeof(text), made, best[area], room);
				put_right(p, y + 9, text, made > best[area] ? GOLD : GREY, 1);
			}
		}
		else if (run.on && run.done[area])
		{
			put_text(p, 8, y, label, GREY, 1);
			score_text(value, sizeof(value), run.made[area], room - 32);
			put_right(p, y, value, run.new_best[area] ? GOLD : WHITE, 1);
			if (!run.counted[area])
				put_right(p, y + 9, "UNRANKED", DIM, 1);
			else if (!run.best_before[area])
				put_right(p, y + 9, "NEW BEST", GOLD, 1);
			else
			{
				difference(text, sizeof(text), run.made[area], run.best_before[area], room);
				put_right(p, y + 9, text, run.new_best[area] ? GOLD : RED, 1);
			}
		}
		else
		{
			put_text(p, 8, y, label, DIM, 1);
			if (best[area])
			{
				/* (to come: its best, to beat) */
				score_text(value, sizeof(value), best[area], room - 32);
				put_right(p, y, value, DIM, 1);
			}
		}
	}
}

static void draw_right(panel *p)
{
	const bool showing = run.on;
	const uint32_t score = showing ? score_of(run.player) : 0u;
	const int room = p->w - 16;
	char text[40], value[32];

	frame_panel(p, "SCORE");
	/* the stage */
	if (showing && run.area >= 0)
		snprintf(text, sizeof(text), "STAGE %d-%d", run.area / STAGES + 1, run.area % STAGES + 1);
	else
		snprintf(text, sizeof(text), "STAGE -");
	put_text(p, 8, 22, text, showing ? WHITE : DIM, 1);
	/* the score, and the next place chased: how much more it takes */
	score_text(value, sizeof(value), score, room);
	put_right(p, 36, value, showing ? WHITE : DIM, 1);
	if (!showing)
		put_text(p, 8, 50, "NO GAME", DIM, 1);
	else if (run.chasing >= 0)
	{
		const uint32_t colour = rank_colours[run.chasing];

		snprintf(text, sizeof(text), "NEXT #%d", run.chasing + 1);
		put_text(p, 8, 50, text, colour, 1);
		score_text(value, sizeof(value), run.table[run.chasing], room);
		if (text_width(value, 1) + text_width(text, 1) + 8 <= room)
			put_right(p, 50, value, colour, 1);    /* (beside it when there is room, below it when not) */
		else
			put_right(p, 60, value, colour, 1);
		put_text(p, 8, 72, "TO GO", GREY, 1);
		score_text(value, sizeof(value), run.table[run.chasing] - score, room - 48);
		put_right(p, 72, value, WHITE, 1);
	}
	else
	{
		put_centre(p, 60, "PAST #1!", GOLD, 1);
	}
	/* the next extra life: how much more score it takes (the score's extends; none left after the last) */
	if (showing)
	{
		const uint32_t extend = next_extend(run.player);

		const char *label = "1UP IN";

		if (extend)
		{
			score_text(value, sizeof(value), extend > score ? extend - score : 0u, room - text_width("1UP", 1) - 8);
			if (text_width(label, 1) + 8 + text_width(value, 1) > room)
				label = "1UP";    /* (a narrow panel: the shorter name, the amount kept whole) */
		}
		put_text(p, 8, 86, label, extend ? EXTEND_COLOUR : DIM, 1);
		put_right(p, 86, extend ? value : "NO MORE", extend ? WHITE : DIM, 1);
	}
	draw_graph(p, score);

	draw_stages(p, score);
}

static void draw_toast(panel *p)
{
	const int top = 18, bottom = 102, w = p->w;
	/* the frame flashes for its first half second */
	const uint32_t rim = TOAST_FRAMES - toast_frames < 30 && (TOAST_FRAMES - toast_frames) / 5 % 2 ? WHITE : GOLD;
	char first[32], second[32] = "";

	box(p, 4, top, w - 4, bottom, HEADER);
	outline(p, 4, top, w - 4, bottom, rim);
	outline(p, 6, top + 2, w - 6, bottom - 2, FRAME);
	{
		/* a little gold trophy: the cup, its handles, the stem and the base */
		const int c = w / 2, y = top + 6;

		box(p, c - 5, y, c + 6, y + 6, GOLD);
		box(p, c - 4, y + 6, c + 5, y + 7, GOLD);
		box(p, c - 3, y + 7, c + 4, y + 8, GOLD);
		box(p, c - 8, y + 1, c - 5, y + 2, GOLD);
		box(p, c - 8, y + 1, c - 7, y + 4, GOLD);
		box(p, c - 8, y + 3, c - 5, y + 4, GOLD);
		box(p, c + 6, y + 1, c + 9, y + 2, GOLD);
		box(p, c + 8, y + 1, c + 9, y + 4, GOLD);
		box(p, c + 6, y + 3, c + 9, y + 4, GOLD);
		box(p, c - 3, y + 1, c - 1, y + 5, 0xFFF0A0u);
		box(p, c, y + 8, c + 1, y + 11, GOLD);
		box(p, c - 3, y + 11, c + 4, y + 13, GOLD);
	}
	put_centre(p, top + 25, "ACHIEVEMENT", GOLD, 1);
	put_centre(p, top + 35, "UNLOCKED", GOLD, 1);
	/* the name: on two lines when it is wider than the panel (at its last space that fits) */
	snprintf(first, sizeof(first), "%s", toast_name);
	if (text_width(first, 1) > w - 16)
	{
		char *space = NULL;

		for (char *at = first; *at && text_width(first, 1) - text_width(at, 1) <= w - 16; at++)
			if (*at == ' ')
				space = at;
		if (space)
		{
			snprintf(second, sizeof(second), "%s", space + 1);
			*space = '\0';
		}
	}
	put_centre(p, top + (second[0] ? 49 : 54), first, WHITE, 1);
	if (second[0])
		put_centre(p, top + 59, second, WHITE, 1);
	if (toast_more > 0)
	{
		char more[16];

		snprintf(more, sizeof(more), "+%d MORE", toast_more);
		put_centre(p, top + 72, more, GREY, 1);
	}
}

/* ---- the FPS meter, on the right panel over the score's graph (the stages' list left as it is) ---- */

static widgets_meter meter;

void widgets_set_meter(const widgets_meter *now)
{
	meter = *now;
}

static void draw_meter(panel *p)
{
	const int top = GRAPH_TOP - 3, bottom = GRAPH_TOP + GRAPH_HEIGHT + 3, w = p->w;
	char text[24];
	const struct { const char *name; double value; const char *format; uint32_t colour; } rows[6] = {
		{ "GAME", meter.game, "%.1f", meter.game < meter.board - 0.5 ? YELLOW : GREEN },
		{ "BOARD", meter.board, "%.1f", meter.board < 57.0 ? RED : WHITE },
		{ "WINDOW", meter.windows, "%.1f", WHITE },
		{ "LATE", (double)meter.late, "%.0f", meter.late ? RED : WHITE },
		{ "WORK MS", meter.work, "%.1f", WHITE },
		{ "MOST MS", meter.work_most, "%.1f", meter.work_most > 17.4 ? YELLOW : WHITE },
	};

	box(p, 4, top, w - 4, bottom, HEADER);
	outline(p, 4, top, w - 4, bottom, FRAME);
	put_centre(p, top + 4, "FPS METER", GOLD, 1);
	for (int row = 0; row < 6; row++)
	{
		const int y = top + 16 + row * 10;

		put_text(p, 10, y, rows[row].name, GREY, 1);
		snprintf(text, sizeof(text), rows[row].format, rows[row].value);
		put_right(p, y, text, rows[row].colour, 1);
	}
}

/* ---- the menu's help, on the right panel while a page is up ---- */

/* 'text' in lines broken at its spaces to the panel's width, from 'y': the y under the last */
static int put_wrapped(panel *p, int y, const char *text, uint32_t colour)
{
	const int most = (p->w - 12) / 8;
	char line[40];

	while (*text && y < WIDGET_H - 12)
	{
		int length = (int)strlen(text), take = length;

		if (length > most)
		{
			/* (the last space that fits; a word too long for a line is cut) */
			take = most;
			while (take > 0 && text[take] != ' ')
				take--;
			if (take == 0)
				take = most;
		}
		snprintf(line, sizeof(line), "%.*s", take, text);
		put_text(p, 6, y, line, colour, 1);
		y += 11;
		text += take;
		while (*text == ' ')
			text++;
	}
	return y;
}

void widgets_draw_help(uint32_t *pixels, int width, const char *title, const char *text)
{
	panel p;
	int y;

	p.w = width < WIDGET_W ? WIDGET_W : width > WIDGET_MAX_W ? WIDGET_MAX_W : width;
	p.pixels = pixels;
	frame_panel(&p, "HELP");
	y = put_wrapped(&p, 26, title, GOLD);
	box(&p, 6, y + 2, p.w - 6, y + 3, FRAME);
	put_wrapped(&p, y + 10, text, WHITE);
}

void widgets_draw(uint32_t *left, uint32_t *right, int width)
{
	panel p;

	if (width < WIDGET_W)
		width = WIDGET_W;
	if (width > WIDGET_MAX_W)
		width = WIDGET_MAX_W;
	p.w = width;
	p.pixels = left;
	draw_left(&p);
	p.pixels = right;
	draw_right(&p);
	if (meter.on)
		draw_meter(&p);
	if (toast_frames > 0)
		draw_toast(&p);
}
