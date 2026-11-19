/**
 *    Name:        game_watch.c
 *    Description: the arcade game watched from its memory (game_watch.h)
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "game_watch.h"
#include "game_memory.h"

#include "cave/program.h"

#include <string.h>

game_watch_state game_watch;

static struct
{
	unsigned mode, lives, bombs, bomb_most, own_power, bees;
	bool flying;
} last;

void game_watch_reset(void)
{
	memset(&game_watch, 0, sizeof(game_watch));
	memset(&last, 0, sizeof(last));
	game_watch.stage = game_watch.cleared = -1;
}

static void stage_begin(int stage)
{
	game_watch.stage = stage;
	game_watch.stage_misses = game_watch.stage_bombs = game_watch.stage_bees = 0;
	game_watch.stage_two_players = false;
}

void game_watch_frame(bool counts)
{
	game_watch_state *const g = &game_watch;
	unsigned mode;
	int area;

	g->begun = g->ended = g->died = g->extended = g->bomb_used = g->continued = g->powered_up = false;
	g->cleared = -1;
	g->bees_taken = 0;
	if (program_frame_lagged())
		return;    /* (the game's memory half way through its frame: looked at next) */
	mode = gm_word(GM_MODE);
	area = gm_area();
	if (mode == 0xBu && last.mode != 0xBu)
	{
		/* a game begins: its ship select */
		memset(g, 0, sizeof(*g));
		g->on = g->begun = true;
		g->counts = true;
		g->stage = g->cleared = -1;
		last.lives = 0;
	}
	if (!counts)
		g->counts = false;
	if (g->on && mode != 0xBu && mode != 7u && mode != 8u)
	{
		g->on = false;
		g->ended = last.mode == 7u || last.mode == 8u;
	}
	if (!g->on)
	{
		last.mode = mode;
		return;
	}
	if (last.mode == 8u && mode == 7u)
		g->continued = g->continued_ever = true;
	g->playing = mode == 7u;

	/* the stage: cleared as it moves on from it during play (to the next, or between the loops, the ending) */
	if (area != g->stage)
	{
		if (g->stage >= 0 && last.mode == 7u && (area == g->stage + 1 || area == -1))
		{
			g->cleared = g->stage;
			g->cleared_misses = g->stage_misses;
			g->cleared_bombs = g->stage_bombs;
			g->cleared_bees = g->stage_bees;
			g->cleared_two_players = g->stage_two_players;
			if (g->stage < 6 && g->stage_bees >= 13u)
				g->loop1_full_bees++;
		}
		if (area >= 0)
			stage_begin(area);
		else
			g->stage = -1;
	}
	if (mode == 7u)
	{
		const uint32_t record = gm_record(0);
		const unsigned lives = gm_byte(GM_LIVES), bombs = gm_byte(record + GM_BOMBS);
		const unsigned bomb_most = gm_byte(record + GM_BOMBS + 1u), own = gm_word(record + GM_OWN_POWER);
		const unsigned bees = gm_word(GM_BEES);
		const bool flying = gm_flying(0);

		if (last.mode == 7u)
		{
			if (lives < last.lives)
			{
				g->died = true;
				g->stage_misses++;
				if (g->stage >= 0 && g->stage < 6)
					g->loop1_misses++;
				g->life_frames = 0;
			}
			else if (lives == last.lives + 1u && flying && last.flying)
				g->extended = true;    /* (one more life while flying: an extend; a continue comes from the count) */
			/* a bomb: the stock one down, its most the same (a miss raises the most and fills the stock) */
			if (bombs + 1u == last.bombs && bomb_most == last.bomb_most && lives == last.lives)
			{
				g->bomb_used = true;
				g->stage_bombs++;
			}
			if (own > last.own_power && lives == last.lives)
				g->powered_up = true;
			if (bees > last.bees)
			{
				g->bees_taken = bees - last.bees;
				g->stage_bees += g->bees_taken;
			}
		}
		last.lives = lives;
		last.bombs = bombs;
		last.bomb_most = bomb_most;
		last.own_power = own;
		last.bees = bees;
		last.flying = flying;
		g->play_frames++;
		g->life_frames++;
		if (gm_flying(1) && flying)
			g->stage_two_players = true;
		g->ship = (int)(gm_word(GM_SHIP_1) % 3u);
		g->laser = gm_word(GM_STYLE_1) == 1u;
		g->score = gm_bcd(GM_SCORE_1);
		g->max_hit = gm_word(GM_MAX_HIT_1);
		g->own_power = own;
		g->other_power = gm_word(record + GM_OTHER_POWER);
		g->bombs = bombs;
		g->bomb_most = bomb_most;
	}
	else
		last.bees = gm_word(GM_BEES);
	last.mode = mode;
}
