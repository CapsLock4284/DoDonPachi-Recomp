/**
 *    Name:        training.c
 *    Description: the main menu's TRAINING (training.h)
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "training.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/sound.h"

#include <stdint.h>
#include <stdio.h>

#define GAME_MODE   0x1017A6u   /* 0xB the ship select, 7 playing */
#define NO_GAME     0x1017A5u   /* bit 7: the attract mode or the title */
#define PHASE       0x1017A4u   /* (byte) 3 as a stage is cleared, 4 as a loop is */
#define STAGE       0x101978u   /* 0-5 (6: between the loops, the ending) */
#define LOOP        0x10197Au
#define GUARD       0x102CCAu   /* (byte) player 1's time without harm (after a life) */
#define SCORE       0x10161Eu   /* (BCD long) */
#define LIVES       0x101965u   /* (byte) the one in play counted */
#define BOMBS       0x102CB0u   /* (byte) the bombs, the next the most the ship carries */
#define OWN_POWER   0x102CACu   /* the style's own weapon's, 0-4 */
#define OTHER_POWER 0x102CAEu
#define HUD_HIGH    0x101626u   /* the HIGH the game shows (a BCD long) */
#define HUD_DIGIT   0x101750u   /* its last digit: the continues its holder used, a tile ($100 + them) */
#define ITEMS       0x10A9F6u   /* the items on the screen: 10 records of $20 (+0: $8010 a P, $8012 a bomb; 0 none) */
#define HUD_SCORE   0x10162Au   /* player 1's score on the HUD: 8 sprites of 8 bytes (+0 the attribute, +2 the code,
                                   $100 + the digit) */
#define MAX_FRAMES  120000L       /* (2-7's Hibachi: Hachi shot down on the way, about 5 minutes of the game's) */
#define BOSS_MUSIC  0x502B8Eu   /* the boss's music (#56) on the YMZ280B's voice 0, from its WARNING: in nibbles */
#define HACHI_AFTER 2220L       /* 2-7: its frames from the start (a speech) to Hachi coming in (38.6 s) */

static unsigned word(uint32_t address)
{
	return *cave_work_ram_byte(address) << 8 | *cave_work_ram_byte(address + 1u);
}

static void set_word(uint32_t address, unsigned value)
{
	*cave_work_ram_byte(address) = (uint8_t)(value >> 8);
	*cave_work_ram_byte(address + 1u) = (uint8_t)value;
}

static void set_byte(uint32_t address, unsigned value)
{
	*cave_work_ram_byte(address) = (uint8_t)value;
}

/* ON PACE: each power-up (a P) raises both weapons a level, and the run there has one in 1-1 (before its boss),
   two in 1-2 (both before its boss) and the last that counts early in 1-3 (its first): MAX from 1-3's boss on, and
   so through the second loop (the power carries over). Found by logging the items as they come out (their records
   at $10A9F6, $20 each: $8010 a P, $8012 a bomb) through both loops with the ship shooting all it can, each P taken
   seen in the power as it rises. Bombs need no ON PACE: their most is 3 until a life is lost (each loss raises it
   by one, to 6), and the stock is full at each place on the way */
static const int on_pace[3][2] = { { 0, 1 }, { 1, 3 }, { 3, 4 } };

int training_on_pace(int stage, int point)
{
	return stage >= 0 && stage < 3 ? on_pace[stage][point == TRAINING_BOSS] : TRAINING_MAX_POWER;
}

int training_lives(void)
{
	return *cave_work_ram_byte(LIVES);
}

int training_place(int stage, int point)
{
	if (stage == TRAINING_AREA_7)
		return point == TRAINING_TRUE_LAST ? TRAINING_TRUE_LAST : TRAINING_BOSS;
	return point == TRAINING_BOSS ? TRAINING_BOSS : TRAINING_START;
}

const char *training_name(const training_setup *setup)
{
	static char name[32];
	const int point = training_place(setup->stage, setup->point);

	if (setup->stage == TRAINING_AREA_7)
		snprintf(name, sizeof(name), "STAGE 2-7 %s", point == TRAINING_TRUE_LAST ? "HIBACHI" : "HACHI");
	else
		snprintf(name, sizeof(name), "STAGE %d-%d%s", setup->stage / 6 + 1, setup->stage % 6 + 1,
			point == TRAINING_BOSS ? " BOSS" : "");
	return name;
}

/* the stage being played, as training's (0-12), or -1: stage 6 of the second loop is 2-7 (in the first, the loop's
   end) */
static int area_now(void)
{
	const unsigned stage = word(STAGE), loop = word(LOOP);

	if (stage == 6u)
		return loop == 1u ? TRAINING_AREA_7 : -1;
	return stage < 6u && loop < 2u ? (int)(loop * 6u + stage) : -1;
}

/* the boss's music begun on the chip's voice 0: its WARNING on the screen */
static bool boss_music(void)
{
	const cave_ymz_voice *const music = &cave_board.sound.ymz280b.voice[0];

	return music->playing && music->start == BOSS_MUSIC;
}

const char *training_prepare(const training_setup *setup)
{
	const bool area_7 = setup->stage == TRAINING_AREA_7;
	const unsigned stage = area_7 ? 6u : (unsigned)(setup->stage % 6), loop = area_7 ? 1u : (unsigned)(setup->stage / 6);
	const int point = training_place(setup->stage, setup->point);
	long frame = 0, select_frames = 0, there = 0;
	uint8_t high[4] = { 0 }, high_digit[2] = { 0 };
	bool playing = false, sent = setup->stage == 0, done = false;

	program_reset();
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	program_run_frame();    /* (the reset takes place: the board starts again) */
	cave_sound_drop();
	for (; frame < MAX_FRAMES && !done; frame++)
	{
		const unsigned mode = word(GAME_MODE);
		unsigned held = 0;

		if (mode == 0xBu)
		{
			/* the ship select (as tests/players/'s): the ship (down presses), shot, the style (right: laser), shot */
			if (select_frames == 0)
			{
				/* (the HIGH as the game began, and its last digit) */
				for (uint32_t at = 0; at < 4u; at++)
					high[at] = *cave_work_ram_byte(HUD_HIGH + at);
				high_digit[0] = *cave_work_ram_byte(HUD_DIGIT);
				high_digit[1] = *cave_work_ram_byte(HUD_DIGIT + 1u);
			}
			select_frames++;
			if ((select_frames >= 100 && select_frames < 104 && setup->ship >= 1)
					|| (select_frames >= 160 && select_frames < 164 && setup->ship >= 2))
				held = CAVE_DOWN;
			else if ((select_frames >= 250 && select_frames < 254) || (select_frames >= 350 && select_frames < 354))
				held = CAVE_BUTTON1;
			else if (select_frames >= 300 && select_frames < 304 && setup->laser)
				held = CAVE_RIGHT;
		}
		else if (mode == 7u && select_frames > 0)
		{
			playing = true;
			set_byte(GUARD, 0x80u);    /* (kept from harm on the way: not firing, no points) */
			if (!sent && *cave_work_ram_byte(PHASE) == 3u)
			{
				/* 1-1 cleared: the stage before the place's (2-1: 1-6, the loop's end, into the second) */
				set_word(STAGE, stage == 0u ? 5u : stage - 1u);
				set_word(LOOP, stage == 0u ? 0u : loop);
				sent = true;
			}
			if (sent && area_now() == setup->stage)
			{
				there++;
				if (point == TRAINING_START)
					done = true;
				else if (!area_7)
				{
					done = boss_music();    /* (the boss: from its WARNING, as it begins) */
					if (!done && stage >= 4u)
					{
						/* (5 and 6's way to their boss: the ship firing at MAX, as a player does, or the enemies
						   left alive wait at the WARNING, firing where the ship comes in: a miss at once) */
						set_word(OWN_POWER, TRAINING_MAX_POWER);
						set_word(OTHER_POWER, TRAINING_MAX_POWER);
						set_word(SCORE, 0);    /* (its points kept from the HIGH) */
						set_word(SCORE + 2u, 0);
						held = frame % 4 < 2 ? CAVE_BUTTON1 : 0u;
					}
				}
				else if (point == TRAINING_BOSS)
					done = there > HACHI_AFTER;    /* (Hachi: the speech over, as it comes in) */
				else
				{
					/* Hibachi: Hachi shot down on the way (the ship firing all the while, at MAX, from below
					   its middle), then from Hibachi's WARNING, the boss's music again */
					set_word(OWN_POWER, TRAINING_MAX_POWER);
					set_word(OTHER_POWER, TRAINING_MAX_POWER);
					set_word(SCORE, 0);    /* (its points kept from the HIGH) */
					set_word(SCORE + 2u, 0);
					done = there > HACHI_AFTER && boss_music();
					if (!done)
						held = frame % 4 < 2 ? CAVE_BUTTON1 : 0u;
				}
			}
		}
		else if (!playing && (*cave_work_ram_byte(NO_GAME) & 0x80u) && program_frames_since_reset() > 1500u)
		{
			/* the attract mode: a coin, then Start, until the ship select comes */
			const long beat = frame % 40;

			if (beat < 5)
				held = CAVE_COIN;
			else if (beat >= 20 && beat < 25)
				held = CAVE_START;
		}
		if (done)
			break;
		cave_board.in0 = (uint16_t)~held;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		if (playing && word(GAME_MODE) != 7u)
			return "the game left the stage on the way (game over?)";
	}
	if (!done)
		return "the place was not reached";
	/* a fresh run from here: score 0, 3 lives, the bombs and power chosen, no guard */
	set_word(SCORE, 0);
	set_word(SCORE + 2u, 0);
	/* (and the HIGH as it was: the way here's points, the ship firing at the boss's way or Hachi, not in it) */
	for (uint32_t at = 0; at < 4u; at++)
		set_byte(HUD_HIGH + at, high[at]);
	set_byte(HUD_DIGIT, high_digit[0]);
	set_byte(HUD_DIGIT + 1u, high_digit[1]);
	set_byte(LIVES, 3u);
	{
		const unsigned bombs = setup->max_bombs ? 6u : 3u;

		set_byte(BOMBS, bombs);
		set_byte(BOMBS + 1u, bombs);
	}
	{
		const int power = setup->power == TRAINING_POWER_ON_PACE ? training_on_pace(setup->stage, setup->point)
			: setup->power;

		set_word(OWN_POWER, (unsigned)power);
		set_word(OTHER_POWER, (unsigned)power);
	}
	set_byte(GUARD, 0u);
	/* the items the way here dropped (the firing on the way to a boss) taken away: the power is the one chosen */
	for (uint32_t item = 0; item < 10u; item++)
		set_word(ITEMS + 0x20u * item, 0);
	/* the HUD's score made again (the game draws it only as the score changes: the way here's stage clear bonus
	   would stay on it): its 8 digits' sprites, 8 bytes each (the attribute, the code: 0 none), all none, as a game
	   starts */
	for (uint32_t digit = 0; digit < 8u; digit++)
	{
		set_word(HUD_SCORE + 8u * digit, 0);
		set_word(HUD_SCORE + 8u * digit + 2u, 0);
	}
	{
		/* and the HIGH's, the next 8 sprites, from its value put back (drawn, too, only as it changes: a second
		   loop's area clear bonus on the way, 6,000,000 before 2-6, would stay on it): the leading zeros none, each
		   other digit $0F30 and $100 + the digit */
		bool shown = false;

		for (uint32_t digit = 0; digit < 8u; digit++)
		{
			const unsigned value = (high[digit / 2u] >> (digit % 2u ? 0 : 4)) & 15u;
			const uint32_t sprite = HUD_SCORE + 8u * (8u + digit);

			shown = shown || value != 0u || digit == 7u;
			set_word(sprite, shown ? 0x0F30u : 0u);
			set_word(sprite + 2u, shown ? 0x100u + value : 0u);
		}
	}
	cave_board.in0 = 0xFFFFu;
	return NULL;
}
