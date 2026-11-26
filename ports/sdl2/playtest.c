/**
 *    Name:        playtest.c
 *    Description: --playtest: the game played through by itself, player 1 kept from harm (playtest.h)
 *    Version:     0.1.0 (draft)
 *    Created:     25-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "playtest.h"
#include "game_memory.h"

#include "cave/machine.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GUARD           0x102CCAu   /* player 1's time without harm (a byte: the game counts it down) */
#define GUARD_HELD      0x80u
#define OWN_POWER       0x102CACu   /* player 1's weapons' power, 0-4 (words: game_memory.h's record +$20, +$22) */
#define OTHER_POWER     0x102CAEu
#define MAX_POWER       4u
#define PLACE_STEADY    30L         /* a place counted once the stage and loop have held it this long */
#define FRAMES_A_SECOND 57.55
#define STUCK_FRAMES    (35L * 60L * 58L)    /* a stage this long (35 minutes of the game's): stuck (2-7, Hachi and
                                                Hibachi as this player fights them: up to about 25) */
#define ENDING_MOST     (10L * 60L * 58L)    /* the ending's way back to the title at most */
#define RUNS            6

/* the board's input bits (active high here) */
#define IN_UP     0x01u
#define IN_DOWN   0x02u
#define IN_LEFT   0x04u
#define IN_RIGHT  0x08u
#define IN_A      0x10u
#define IN_B      0x20u
#define IN_C      0x40u
#define IN_START  0x80u
#define IN_COIN   0x100u

static const char *const run_names[RUNS] = { "A-shot", "A-laser", "B-shot", "B-laser", "C-shot", "C-laser" };

enum { WAITING, SELECTING, PLAYING, ENDING };

static int first_run, last_run, run;
static int phase;
static long frame, phase_frames, select_frames, play_frames, since_progress, run_began;
static int place_loop = -1, place_stage = -1, seen_loop = -1, seen_stage = -1;
static long seen_for;
static bool failed, finished, report_written;
static char report[16384], status[96], file_path[1100], summary[256];
static int game_version;

static void note(const char *format, ...)
{
	const size_t used = strlen(report);
	va_list list;

	if (used + 1u >= sizeof(report))
		return;
	va_start(list, format);
	vsnprintf(report + used, sizeof(report) - used, format, list);
	va_end(list);
}

static const char *game_time(long frames)
{
	static char text[2][32];
	static int which;
	const long seconds = (long)(frames / FRAMES_A_SECOND);

	which ^= 1;
	snprintf(text[which], sizeof(text[which]), "%ld:%02ld:%02ld", seconds / 3600, seconds / 60 % 60, seconds % 60);
	return text[which];
}

static bool second_loop_played;    /* (2-1 seen: stage 6 of loop 1 is 2-7 from then on, the pause between the loops
                                       before: game_memory.h) */

/* the place in the game: "1-3", "2-7", "between the loops", "the ending" */
static const char *place_name(int loop, int stage)
{
	static char text[32];

	if (loop >= 2)
		return "the ending";
	if (stage == 6)
		return loop == 1 && second_loop_played ? "2-7" : "between the loops";
	snprintf(text, sizeof(text), "%d-%d", loop + 1, stage + 1);
	return text;
}

bool playtest_choose(const char *which)
{
	char ship = 0;
	bool laser;

	if (!which || !strcmp(which, "all") || !strcmp(which, "ALL"))
	{
		first_run = 0;
		last_run = RUNS - 1;
		return true;
	}
	ship = (char)toupper((unsigned char)which[0]);
	if (ship < 'A' || ship > 'C')
		return false;
	which += 1 + (which[1] == '-' || which[1] == ' ' || which[1] == '_');
	if (!strcmp(which, "shot") || !strcmp(which, "SHOT"))
		laser = false;
	else if (!strcmp(which, "laser") || !strcmp(which, "LASER"))
		laser = true;
	else
		return false;
	first_run = last_run = (ship - 'A') * 2 + (laser ? 1 : 0);
	return true;
}

static void write_report(void)
{
	FILE *const file = file_path[0] ? fopen(file_path, "w") : NULL;

	if (file)
	{
		fputs(report, file);
		fclose(file);
	}
	report_written = true;
}

/* (a crash or the 68000 stopping ends the program: the report says where it was) */
static void at_exit(void)
{
	if (finished || report_written)
		return;
	note("\nTHE PROGRAM STOPPED during %s, in %s at %s of the game's time: see ddonpach.log\n", run_names[run],
		place_loop >= 0 ? place_name(place_loop, place_stage) : "its start", game_time(frame - run_began));
	write_report();
	fputs(report, stderr);
}

void playtest_begin(const char *path, int version)
{
	snprintf(file_path, sizeof(file_path), "%s", path ? path : "");
	game_version = version;
	run = first_run;
	phase = WAITING;
	frame = phase_frames = select_frames = play_frames = since_progress = run_began = 0;
	report[0] = '\0';
	note("DoDonPachi Recomp playtest: %s version, %s\n", version ? "the Japanese" : "the World",
		first_run == last_run ? run_names[first_run] : "every ship and style");
	note("(player 1 kept from harm, at MAX power; the ROM set's own EEPROM; the arcade's slowdown on; nothing saved)\n\n");
	atexit(at_exit);
}

/* the ship select, as training.c's: the ship (down presses), A, the style (right: laser), A */
static unsigned select_inputs(void)
{
	const int ship = run / 2;
	const bool laser = run % 2 != 0;
	const long at = select_frames % 600;    /* (again from the top if the select is still up 600 frames on) */

	if ((at >= 100 && at < 104 && ship >= 1) || (at >= 160 && at < 164 && ship >= 2))
		return IN_DOWN;
	if ((at >= 250 && at < 254) || (at >= 350 && at < 354))
		return IN_A;
	if (at >= 300 && at < 304 && laser)
		return IN_RIGHT;
	return 0;
}

/* the way it plays (the test players', tests/players/, kept near the middle, where the bosses are: they go down
   within their time, and 2-7's Hachi, which Hibachi needs shot down, too): button C's auto-fire, a short sweep left
   and right, up and down as much, the shot held a while (the laser), a bomb every 900 frames */
static bool hibachi_seen;          /* 2-7's true last boss came (its WARNING: the boss's music) */

/* the boss's music begun on the sound chip's voice 0: a boss's WARNING (in 2-7, Hibachi's: Hachi has its own) */
static bool gm_boss_music(void)
{
	const cave_ymz_voice *const music = &cave_board.sound.ymz280b.voice[0];

	return music->playing && music->start == GM_BOSS_MUSIC;
}


static unsigned play_inputs(void)
{
	const long f = play_frames;
	unsigned held = IN_C;

	if (hibachi_seen || gm_boss_music())
	{
		/* a boss (from its WARNING, the boss's music) and Hibachi, which has no time out and has to be shot down:
		   the laser held, the ship still (Hibachi down in about 4 minutes of the game's this way, every ship and
		   style; moving and bombing, 20) */
		return IN_A;
	}

	held |= (f / 25) % 2 ? IN_LEFT : IN_RIGHT;
	if ((f / 70) % 4 == 0)
		held |= IN_UP;
	else if ((f / 70) % 4 == 2)
		held |= IN_DOWN;
	if ((f / 130) % 5 == 4)
		held = (held & ~IN_C) | IN_A;
	if (f % 900 == 450)
		held |= IN_B;
	return held;
}

static void set_word(uint32_t address, unsigned value)
{
	*cave_work_ram_byte(address) = (uint8_t)(value >> 8);
	*cave_work_ram_byte(address + 1u) = (uint8_t)value;
}

unsigned playtest_inputs(void)
{
	const unsigned mode = gm_word(GM_MODE);

	if (finished)
		return 0;
	if (mode == 0xBu)
		return select_inputs();
	if (mode == 7u || mode == 8u)
		return phase == PLAYING ? play_inputs() : 0u;
	if (phase == WAITING)
	{
		/* the title or the attract mode: a coin, then Start */
		const long beat = phase_frames % 120;

		if (phase_frames > 120 && beat < 6)
			return IN_COIN;
		if (phase_frames > 120 && beat >= 60 && beat < 66)
			return IN_START;
	}
	return 0;
}

static int fail(const char *format, ...)
{
	char why[256];
	va_list list;

	va_start(list, format);
	vsnprintf(why, sizeof(why), format, list);
	va_end(list);
	note("  FAILED: %s (%s of the game's time into the run)\n", why, game_time(frame - run_began));
	failed = true;
	finished = true;
	write_report();
	return PLAYTEST_FAILED;
}

static void set_phase(int to)
{
	phase = to;
	phase_frames = 0;
}

int playtest_frame(void)
{
	const unsigned mode = gm_word(GM_MODE);
	const bool attract = mode != 7u && mode != 8u && mode != 0xBu;

	if (finished)
		return failed ? PLAYTEST_FAILED : PLAYTEST_PASSED;
	frame++;
	phase_frames++;
	if (mode == 7u || mode == 8u)
	{
		/* kept from harm (as INVINCIBLE=1), and at MAX power (the bosses down sooner: a quicker test) */
		*cave_work_ram_byte(GUARD) = GUARD_HELD;
		if (gm_flying(0))
		{
			set_word(OWN_POWER, MAX_POWER);
			set_word(OTHER_POWER, MAX_POWER);
		}
	}
	switch (phase)
	{
	case WAITING:
		if (mode == 0xBu)
		{
			set_phase(SELECTING);
			select_frames = 0;
			run_began = frame;
			note("%s (%d of %d)\n", run_names[run], run - first_run + 1, last_run - first_run + 1);
		}
		else if (phase_frames > 20L * 60L * 58L)
			return fail("no game began (the ship select never came)");
		break;
	case SELECTING:
		if (mode == 0xBu)
			select_frames++;
		else if (mode == 7u)
		{
			const unsigned ship = gm_word(GM_SHIP_1), style = gm_word(GM_STYLE_1);

			if ((int)ship != run / 2 || (int)style != run % 2)
				return fail("the ship select gave type %c %s, not %s", 'A' + (ship % 3u), style ? "laser" : "shot",
					run_names[run]);
			set_phase(PLAYING);
			play_frames = since_progress = seen_for = 0;
			second_loop_played = hibachi_seen = false;
			place_loop = place_stage = seen_loop = seen_stage = -1;
		}
		else if (attract && phase_frames > 600)
			return fail("the game went back to its title from the ship select");
		break;
	case PLAYING:
	{
		const int loop = (int)gm_word(GM_LOOP), stage = (int)gm_word(GM_STAGE);

		play_frames++;
		since_progress++;
		if (loop != seen_loop || stage != seen_stage)
		{
			seen_loop = loop;
			seen_stage = stage;
			seen_for = 0;
		}
		if (++seen_for >= PLACE_STEADY && (loop != place_loop || stage != place_stage))
		{
			place_loop = loop;
			place_stage = stage;
			since_progress = 0;
			if (loop == 1 && stage < 6)
				second_loop_played = true;
			note("  %-18s at %s\n", place_name(loop, stage), game_time(frame - run_began - PLACE_STEADY));
			if (loop >= 2)
				set_phase(ENDING);
		}
		if (!hibachi_seen && loop == 1 && stage == 6 && second_loop_played && gm_boss_music())
		{
			hibachi_seen = true;
			note("  %-18s at %s\n", "Hibachi's WARNING", game_time(frame - run_began));
		}
		if (mode == 8u)
			return fail("the ship was lost in %s (the continue's count down came)", place_name(loop, stage));
		if (attract)
			return fail("the game ended in %s, before the ending", place_name(loop, stage));
		if (since_progress > STUCK_FRAMES)
			return fail("%s went on for 35 minutes of the game's time (stuck?)", place_name(loop, stage));
		break;
	}
	case ENDING:
		play_frames++;
		if (attract)
		{
			if (!hibachi_seen)
				return fail("the ending came without Hibachi (2-7's true last boss)");
			note("  %-18s at %s: PASSED\n\n", "back at the title", game_time(frame - run_began));
			if (run == last_run)
			{
				finished = true;
				return PLAYTEST_PASSED;
			}
			run++;
			set_phase(WAITING);
		}
		else if (phase_frames > ENDING_MOST)
			return fail("the ending did not come back to the title in 10 minutes of the game's time");
		break;
	}
	return PLAYTEST_GOING;
}

const char *playtest_status(void)
{
	const char *where = phase == WAITING ? "starting a game" : phase == SELECTING ? "the ship select"
		: phase == ENDING ? "the ending" : place_loop >= 0 ? place_name(place_loop, place_stage) : "";

	snprintf(status, sizeof(status), "%s (%d of %d)  %s  %s", run_names[run], run - first_run + 1,
		last_run - first_run + 1, where, game_time(frame - run_began));
	return status;
}

const char *playtest_finish(double seconds, bool stopped_by_player)
{
	if (!finished && stopped_by_player)
	{
		note("  STOPPED by the player in %s\n", playtest_status());
		failed = true;
	}
	finished = true;
	snprintf(summary, sizeof(summary), "%s: %s of the game's time in %.0f seconds (%.1f times the arcade's speed)",
		failed ? "NOT PASSED" : "ALL PASSED", game_time(frame), seconds,
		seconds > 0.0 ? frame / FRAMES_A_SECOND / seconds : 0.0);
	note("%s\n", summary);
	write_report();
	return report;
}

const char *playtest_summary(void)
{
	return summary;
}
