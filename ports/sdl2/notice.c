/**
 *    Name:        notice.c
 *    Description: a line of text over the game for a moment (notice.h)
 *    Version:     0.1.0 (draft)
 *    Created:     16-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "notice.h"

#include "font5x7.h"

#include "cave/video.h"

#include <stdio.h>
#include <string.h>

#define NOTICE_FRAMES 115    /* about two seconds */
#define PICTURE_W     VIDEO_HEIGHT    /* (the picture as the cabinet shows it: 240 x 320) */
#define PICTURE_H     VIDEO_WIDTH
#define SCALE         1
#define TOP           296    /* the line's top: near the bottom, over the bomb gauge */

static char shown[48];
static int frames_left;

void notice_show(const char *text)
{
	snprintf(shown, sizeof(shown), "%s", text);
	frames_left = NOTICE_FRAMES;
}

const uint32_t *notice_over(const uint32_t *picture)
{
	static uint32_t out[PICTURE_W * PICTURE_H];
	const int width = (int)strlen(shown) * 6 * SCALE - SCALE;
	const int left = (PICTURE_W - width) / 2;

	if (frames_left <= 0)
		return picture;
	frames_left--;
	memcpy(out, picture, sizeof(out));
	/* a dark band, then the letters in white */
	for (int y = TOP - 3; y < TOP + 7 * SCALE + 3; y++)
		for (int x = left - 4; x < left + width + 4; x++)
			if (x >= 0 && x < PICTURE_W && y >= 0 && y < PICTURE_H)
			{
				const uint32_t pixel = out[y * PICTURE_W + x];

				out[y * PICTURE_W + x] = (pixel >> 2 & 0x3F3F3Fu);
			}
	for (int index = 0, x = left; shown[index]; index++, x += 6 * SCALE)
	{
		const glyph *const shape = glyph_of(shown[index]);

		if (!shape)
			continue;
		for (int row = 0; row < 7; row++)
			for (int column = 0; column < 5; column++)
				if (shape->rows[row][column] == '#')
					for (int dy = 0; dy < SCALE; dy++)
						for (int dx = 0; dx < SCALE; dx++)
						{
							const int px = x + column * SCALE + dx, py = TOP + row * SCALE + dy;

							if (px >= 0 && px < PICTURE_W && py >= 0 && py < PICTURE_H)
								out[py * PICTURE_W + px] = 0xF0F0F0u;
						}
	}
	return out;
}
