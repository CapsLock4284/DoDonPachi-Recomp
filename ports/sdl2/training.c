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
#define NEXT_STOP   0x101996u   /* the scroll's next stop: $FFFF once it has ended (the boss) */
#define GUARD       0x102CCAu   /* (byte) player 1's time without harm (after a life) */
#define SCORE       0x10161Eu   /* (BCD long) */
#define LIVES       0x101965u   /* (byte) the one in play counted */
#define BOMBS       0x102CB0u   /* (byte) the bombs, the next the most the ship carries */
#define OWN_POWER   0x102CACu   /* the style's own weapon's, 0-4 */
#define OTHER_POWER 0x102CAEu
#define HUD_SCORE   0x10162Au   /* player 1's score on the HUD: 8 sprites of 8 bytes (+0 the attribute, +2 the code,
                                   $100 + the digit) */
#define MAX_FRAMES  60000L

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

int training_lives(void)
{
	return *cave_work_ram_byte(LIVES);
}

const char *training_name(const training_setup *setup)
{
	static char name[32];

	snprintf(name, sizeof(name), "STAGE %d-%d%s", setup->stage / 6 + 1, setup->stage % 6 + 1,
		setup->point == TRAINING_BOSS ? " BOSS" : "");
	return name;
}

static int area_now(void)
{
	const unsigned stage = word(STAGE), loop = word(LOOP);

	return stage < 6u && loop < 2u ? (int)(loop * 6u + stage) : -1;
}

const char *training_prepare(const training_setup *setup)
{
	const unsigned stage = (unsigned)(setup->stage % 6), loop = (unsigned)(setup->stage / 6);
	long frame = 0, select_frames = 0;
	bool playing = false, sent = setup->stage == 0, begun = false, scrolling = false, done = false;

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
				if (!begun)
					begun = true;
				if (setup->point == TRAINING_START)
					done = true;
				else
				{
					/* the boss: where the stage's scroll has ended (after it has run in this stage) */
					if (word(NEXT_STOP) != 0xFFFFu)
						scrolling = true;
					else if (scrolling)
						done = true;
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
	set_byte(LIVES, 3u);
	{
		const unsigned bombs = setup->max_bombs ? 6u : 3u;

		set_byte(BOMBS, bombs);
		set_byte(BOMBS + 1u, bombs);
	}
	set_word(OWN_POWER, (unsigned)setup->power);
	set_word(OTHER_POWER, (unsigned)setup->power);
	set_byte(GUARD, 0u);
	/* the HUD's score made again (the game draws it only as the score changes: the way here's stage clear bonus
	   would stay on it): its 8 digits' sprites, 8 bytes each (the attribute, the code: 0 none), all none, as a game
	   starts */
	for (uint32_t digit = 0; digit < 8u; digit++)
	{
		set_word(HUD_SCORE + 8u * digit, 0);
		set_word(HUD_SCORE + 8u * digit + 2u, 0);
	}
	cave_board.in0 = 0xFFFFu;
	return NULL;
}
