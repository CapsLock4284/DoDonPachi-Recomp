/**
 *    Name:        hitbox.c
 *    Description: the ships' hitboxes and the laser's aura shown over the picture (hitbox.h)
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "hitbox.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/video.h"

#include <string.h>

#define PICTURE_W   VIDEO_HEIGHT    /* 240 */
#define PICTURE_H   VIDEO_WIDTH     /* 320 */
#define HISTORY     5
#define GLOW        2               /* pixels of glow around the box */
#define SEEN_IN_A_ROW 3             /* a ship's box shown once its sprite has been on the screen this many frames */
#define SEEN_FOR    8               /* a ship's box kept this many frames after its sprite was last seen (a flicker) */
#define SPRITE_BANK 0x2000u         /* the sprite RAM's words a bank: 8 a sprite */
#define GAME_MODE   0x1017A6u       /* 7: playing */
#define AURA_COLOUR 0x40E0FFu
#define BULLETS     0x106CB6u       /* the enemies' bullets: $40 bytes each */
#define BULLET_SIZE 0x40u
#define BULLETS_MOST 245            /* the most the game tests: 225 in the first loop, 245 in the second */
#define LOOP        0x10197Au
#define BULLET_COLOUR 0x50FF70u

typedef struct
{
	bool active;
	int x0, x1, y0, y1;             /* the board's pixels, both ends in */
} box;

typedef struct
{
	box ship, aura;
} boxes;

/* the bullets that can hit a ship this frame: their points, the board's pixels */
typedef struct
{
	int count;
	int16_t x[BULLETS_MOST], y[BULLETS_MOST];
} bullets;

static const uint32_t records[2] = { 0x102C8Cu, 0x102CCCu }, auras[2] = { 0x103BAEu, 0x103ECEu };
static boxes history[HISTORY][2];
static bullets bullet_history[HISTORY];
static unsigned recorded;
static unsigned unseen[2] = { SEEN_FOR, SEEN_FOR };    /* the frames since each ship's sprite was last on the screen */
static unsigned seen[2];           /* the frames in a row it has been */
static bool showing[2];

static int word(uint32_t address)
{
	return *cave_work_ram_byte(address) << 8 | *cave_work_ram_byte(address + 1u);
}

/* 64ths of a pixel to pixels, rounding down */
static int pixels(int units)
{
	return units >= 0 ? units >> 6 : -((-units + 63) >> 6);
}

/* The ship's own sprite on the screen (the bank the board shows): its code, the record's +04, near its position (the
   sprite's corner 16 and 14 pixels from it on the owner's checks: up to 32 x 48 it) */
static bool ship_shown(uint32_t record, int x, int y)
{
	const uint16_t *sprite = &cave_board.sprite_ram[SPRITE_BANK * (cave_board.video_regs[4] & 3u)];
	const unsigned code = (unsigned)word(record + 0x04u);

	for (unsigned index = 0; index < SPRITE_BANK; index += 8u, sprite += 8)
	{
		int sx = sprite[2] & 0x3FF, sy = sprite[3] & 0x3FF;

		if (sprite[1] != code || (sprite[0] & 3u))
			continue;
		if (sx & 0x200)
			sx -= 0x400;
		if (sy & 0x200)
			sy -= 0x400;
		if (sx > x - 64 && sx < x + 16 && sy > y - 64 && sy < y + 16)
			return true;
	}
	return false;
}

/* a record's box: position +06, +08; reaching +10 above +06, +12 below, +14 above +08, +16 below */
static void box_of(uint32_t record, box *b)
{
	const int x = word(record + 0x06u), y = word(record + 0x08u);

	b->x0 = pixels(x - word(record + 0x12u));
	b->x1 = pixels(x + word(record + 0x10u));
	b->y0 = pixels(y - word(record + 0x16u));
	b->y1 = pixels(y + word(record + 0x14u));
}

void hitbox_record(void)
{
	boxes *const now = history[recorded++ % HISTORY];
	const bool playing = word(GAME_MODE) == 7;

	bullets *const shots = &bullet_history[(recorded - 1u) % HISTORY];

	if (program_frame_lagged())
	{
		/* (a frame of slowdown: the picture is the last one again, and so are its boxes; the records are half way
		   through the frame) */
		if (recorded > 1u)
		{
			memcpy(now, history[(recorded - 2u) % HISTORY], sizeof(history[0]));
			*shots = bullet_history[(recorded - 2u) % HISTORY];
		}
		return;
	}
	/* the bullets the game tests (func_0057EE): in play (+00 bit 15; the game's own test doesn't look, but the others
	   are away from the screen), and none of +00's bits 14, 12, 8 (the test's mask $51 on its top byte: a bullet
	   that has hit, or one that can't) */
	shots->count = 0;
	if (playing)
	{
		const int most = word(LOOP) != 0 ? BULLETS_MOST : 225;

		for (int index = 0; index < most; index++)
		{
			const uint32_t record = BULLETS + BULLET_SIZE * (uint32_t)index;
			const int flags = word(record);

			if ((flags & 0x8000) && !(flags & 0x5100))
			{
				shots->x[shots->count] = (int16_t)pixels(word(record + 0x06u));
				shots->y[shots->count] = (int16_t)pixels(word(record + 0x08u));
				shots->count++;
			}
		}
	}
	for (int player = 0; player < 2; player++)
	{
		const uint32_t record = records[player];
		const bool in_play = (word(record) & 0x8000) != 0;

		if (in_play && playing && ship_shown(record, pixels(word(record + 6u)), pixels(word(record + 8u))))
		{
			unseen[player] = 0;
			if (seen[player] < SEEN_IN_A_ROW)
				seen[player]++;
		}
		else
		{
			seen[player] = 0;
			if (unseen[player] < SEEN_FOR)
				unseen[player]++;
		}
		/* (shown once the ship has been for a few frames in a row; kept through a flicker) */
		if (seen[player] >= SEEN_IN_A_ROW)
			showing[player] = true;
		else if (unseen[player] >= SEEN_FOR || !in_play || !playing)
			showing[player] = false;
		now[player].ship.active = showing[player];
		box_of(record, &now[player].ship);
		now[player].aura.active = showing[player] && (word(auras[player]) & 0x8000) != 0;
		box_of(auras[player], &now[player].aura);
	}
}

/* 'percent' of 'colour' over the pixel at the picture's (column, row) */
static void tint(uint32_t *picture, int column, int row, uint32_t colour, int percent)
{
	uint32_t *pixel;
	uint32_t out = 0;

	if (column < 0 || column >= PICTURE_W || row < 0 || row >= PICTURE_H)
		return;
	pixel = &picture[row * PICTURE_W + column];
	for (int shift = 0; shift <= 16; shift += 8)
	{
		const uint32_t from = *pixel >> shift & 255u, to = colour >> shift & 255u;

		out |= (from + (to > from ? (to - from) * (uint32_t)percent / 100u : 0u)
			- (to < from ? (from - to) * (uint32_t)percent / 100u : 0u)) << shift;
	}
	*pixel = out;
}

/* a box's place on the cabinet's picture: its column is the board's y, its row the board's x from the far side */
static void picture_box(const box *b, int *left, int *right, int *top, int *bottom)
{
	*left = b->y0;
	*right = b->y1;
	*top = (PICTURE_H - 1) - b->x1;
	*bottom = (PICTURE_H - 1) - b->x0;
}

void hitbox_draw(uint32_t *picture, int frames_ago, bool hitboxes, bool show_auras, bool show_bullets)
{
	const boxes *now;

	if (recorded <= (unsigned)frames_ago || frames_ago >= HISTORY)
		return;
	now = history[(recorded - 1u - (unsigned)frames_ago) % HISTORY];
	if (show_bullets)
	{
		/* each bullet's point: what the game tests against the ships' boxes (a bullet hits as its point comes inside
		   one), a white pixel in a small green cross so it shows on any bullet */
		const bullets *const shots = &bullet_history[(recorded - 1u - (unsigned)frames_ago) % HISTORY];

		for (int index = 0; index < shots->count; index++)
		{
			const box point = { true, shots->x[index], shots->x[index], shots->y[index], shots->y[index] };
			int left, right, top, bottom;

			picture_box(&point, &left, &right, &top, &bottom);
			tint(picture, left - 1, top, BULLET_COLOUR, 85);
			tint(picture, left + 1, top, BULLET_COLOUR, 85);
			tint(picture, left, top - 1, BULLET_COLOUR, 85);
			tint(picture, left, top + 1, BULLET_COLOUR, 85);
			tint(picture, left, top, 0xFFFFFFu, 100);
		}
	}
	for (int player = 0; player < 2; player++)
	{
		int left, right, top, bottom;

		if (show_auras && now[player].aura.active)
		{
			/* the aura: a light blue box, its edge brighter */
			picture_box(&now[player].aura, &left, &right, &top, &bottom);
			for (int row = top; row <= bottom; row++)
				for (int column = left; column <= right; column++)
				{
					const bool edge = row == top || row == bottom || column == left || column == right;

					tint(picture, column, row, AURA_COLOUR, edge ? 70 : 18);
				}
		}
		if (hitboxes && now[player].ship.active)
		{
			/* the hitbox: white, its middle half (the ship shows through), a glow with rounded corners */
			picture_box(&now[player].ship, &left, &right, &top, &bottom);
			for (int row = top - GLOW; row <= bottom + GLOW; row++)
				for (int column = left - GLOW; column <= right + GLOW; column++)
				{
					const int dx = column < left ? left - column : column > right ? column - right : 0;
					const int dy = row < top ? top - row : row > bottom ? row - bottom : 0;
					const int distance = dx > dy ? dx : dy;
					const bool edge = row == top || row == bottom || column == left || column == right;

					if (distance == 0)
						tint(picture, column, row, 0xFFFFFFu, edge ? 100 : 50);
					else if (dx + dy <= GLOW + 1)
						tint(picture, column, row, 0xFFFFFFu, distance == 1 ? 50 : 20);
				}
		}
	}
}
