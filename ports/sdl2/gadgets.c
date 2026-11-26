/**
 *    Name:        gadgets.c
 *    Description: the gadget panels (gadgets.h)
 *    Version:     0.1.0 (draft)
 *    Created:     25-11-2026
 *
 *    license:BSD-3-Clause
 *    Laid out in the panel's units (the game's pixels: WIDGET_H tall, 'width' wide) and drawn at k pixels to each
 *    (gadget_canvas.h). Each gadget is a box framed in brushed steel with a blue header strip; they are stacked down
 *    each panel, the room left over shared out between them.
 */
#include "gadgets.h"
#include "achievements.h"
#include "gadget_canvas.h"
#include "game_watch.h"
#include "widgets.h"

#include "cave/board.h"
#include "cave/machine.h"
#include "cave/rom_set.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the look ---- */

#define BACK_TOP    0x0B1A36u
#define BACK_FOOT   0x02060Fu
#define INNER       0x0A1A38u
#define WELL        0x0F2648u   /* a well in a gadget: a bar's track, a box */
#define WELL_EDGE   0x2A4A7Eu
#define HEADER_TEXT 0xF2F7FFu
#define HEADER_NOTE 0xBFD6FFu
#define LABEL       0x9DB4DAu
#define NUMBER      0xE6EFFFu
#define DIM         0x5D7AA8u
#define GOLD        0xFFC83Cu
#define ORANGE      0xFF8A2Au
#define RED         0xFF4F4Fu
#define PINK        0xFF6BD8u
#define CYAN        0x3FA9FFu
#define SKY         0x6CD8FFu
#define TEAL        0x2FD3C7u
#define GREEN       0x3DDC5Cu
#define YELLOW      0xFFE040u
#define EXTEND      0xC8FFC8u

#define MARGIN      3.0f        /* around the panel's gadgets */
#define GAP         3.0f        /* between them, at least */
#define FRAME       1.1f        /* the steel's width */
#define RADIUS      3.0f
#define HEADER      8.5f        /* the header strip's height */
#define PAD         2.6f        /* inside a gadget's body */
#define TITLE       5.0f        /* the header's text */
#define NOTE        3.6f        /* the header's note at its right */
#define SMALL       3.9f        /* a label */
#define VALUE       4.6f        /* a value beside it */

static const uint32_t steel[4] = { 0xE3E9F1u, 0x9AA7B9u, 0x6F7D91u, 0xC7D0DCu };
static const float steel_at[4] = { 0.0f, 0.38f, 0.52f, 1.0f };
static const uint32_t strip[3] = { 0x3B7CF0u, 0x1A4BB0u, 0x123A8Eu };
static const float strip_at[3] = { 0.0f, 0.55f, 1.0f };
static const uint32_t places[5] = { GOLD, 0xFF5CE0u, ORANGE, GREEN, SKY };    /* (the classic panel's #1 ... #5) */

typedef struct
{
	float x, y, w, h;           /* the body: inside the frame, under the header */
} body;

/* The panels' frames, header strips and background are drawn once for a width and k (the chrome: kept, and copied
   under each frame's panels), their text and contents each frame: true while the chrome is drawn */
static bool chrome_pass;

/* Each gadget is drawn again, and given to the graphics card, only when what it shows has changed: first recorded
   (gcanvas.hash: its drawing as a hash, nothing drawn), then, when the hash isn't last frame's, its place put back
   to the chrome and the gadget drawn there. A side is drawn whole (fresh false) when its chrome is new, its picture
   isn't the one drawn last time, the help or an achievement's box has been over it, or gadgets_invalidate */
#define PARTS 5

static struct
{
	uint32_t hash[2][PARTS];
	bool fresh[2];
	const uint32_t *pixels[2];  /* the pictures last drawn */
	bool toast;                 /* an achievement's box over the right panel last frame */
	bool help;                  /* the help drawn over the right panel */
} kept;

static int side_now;               /* the side being drawn: 0 left, 1 right */
static const uint32_t *chrome_now; /* its chrome */
static gadgets_dirty *dirty_now;   /* the places drawn again, or NULL */

typedef void (*part_fn)(gcanvas *c, const widgets_view *v, float x, float y, float w, float h);

static void part(gcanvas *c, int id, part_fn draw, const widgets_view *v, float x, float y, float w, float h)
{
	uint32_t hash = 2166136261u;
	gcanvas record = *c;
	const float place[4] = { x, y, w, h };

	if (chrome_pass)
	{
		draw(c, v, x, y, w, h);
		return;
	}
	record.hash = &hash;
	gc_record(&record, place, sizeof(place));
	draw(&record, v, x, y, w, h);
	if (kept.fresh[side_now] && kept.hash[side_now][id] == hash)
		return;
	kept.hash[side_now][id] = hash;
	if (!kept.fresh[side_now] || !chrome_now)
	{
		draw(c, v, x, y, w, h);    /* (the side drawn whole: its chrome copied already) */
		return;
	}
	{
		/* (its place, a pixel more each way, back to the chrome: then drawn) */
		int left = (int)floorf(x * c->k) - 1, top = (int)floorf(y * c->k) - 1;
		int right = (int)ceilf((x + w) * c->k) + 1, bottom = (int)ceilf((y + h) * c->k) + 1;

		left = left < 0 ? 0 : left;
		top = top < 0 ? 0 : top;
		right = right > c->w ? c->w : right;
		bottom = bottom > c->h ? c->h : bottom;
		if (left >= right || top >= bottom)
			return;
		for (int row = top; row < bottom; row++)
			memcpy(c->pixels + (size_t)row * (size_t)c->w + (size_t)left,
				chrome_now + (size_t)row * (size_t)c->w + (size_t)left, (size_t)(right - left) * sizeof(uint32_t));
		draw(c, v, x, y, w, h);
		if (dirty_now)
		{
			int *const count = &dirty_now->count[side_now];

			if (*count < GADGETS_RECTS)
			{
				const gadgets_rect rect = { left, top, right - left, bottom - top };

				dirty_now->rect[side_now][(*count)++] = rect;
			}
			else
			{
				const gadgets_rect whole = { 0, 0, c->w, c->h };

				dirty_now->rect[side_now][0] = whole;
				*count = 1;
			}
		}
	}
}

static void background(gcanvas *c, float width, float height)
{
	static const uint32_t colours[2] = { BACK_TOP, BACK_FOOT };
	static const float at[2] = { 0.0f, 1.0f };

	gc_gradient(c, 0.0f, 0.0f, width, height, 0.0f, colours, at, 2);
}

/* The panel drawn larger where it has the room: the gadgets as they are laid out need 'natural' of its height (their
   gaps at their least); the canvas zoomed so that they fill up to 1.3 times, as far as the width keeps 118 units (the
   narrowest panel's room); the panel's width and height returned in the zoomed units */
static gcanvas zoomed(const gcanvas *c, float natural, float *width, float *height)
{
	gcanvas z = *c;
	float zoom = ((float)WIDGET_H - 2.0f * MARGIN) / natural;

	if (zoom > 1.3f)
		zoom = 1.3f;
	if (zoom > *width / 118.0f)
		zoom = *width / 118.0f;
	if (zoom < 1.0f)
		zoom = 1.0f;
	z.k = c->k * zoom;
	*width /= zoom;
	*height = (float)WIDGET_H / zoom;
	return z;
}

/* a gadget's frame and header ('note' at the header's right, or NULL), its body returned */
static body gadget(gcanvas *c, float x, float y, float w, float h, const char *title, const char *note)
{
	const float ix = x + FRAME, iy = y + FRAME, iw = w - 2.0f * FRAME, ih = h - 2.0f * FRAME;
	body b;

	if (chrome_pass)
	{
		gc_box(c, x - 0.3f, y + 0.6f, w + 0.6f, h + 0.6f, RADIUS + 0.3f, 0x000000u, 120u);    /* (its shadow) */
		gc_gradient(c, x, y, w, h, RADIUS, steel, steel_at, 4);
		gc_box(c, ix, iy, iw, ih, RADIUS - FRAME * 0.6f, INNER, 255u);
		gc_gradient(c, ix, iy, iw, HEADER, RADIUS - FRAME * 0.6f, strip, strip_at, 3);
		gc_box(c, ix, iy + HEADER - 1.5f, iw, 1.5f, 0.0f, strip[2], 255u);    /* (its foot square) */
		gc_brushed(c, ix, iy, iw, HEADER, 0.75f, 0xFFFFFFu, 14u);
		gc_box(c, ix, iy + HEADER, iw, 0.5f, 0.0f, 0x061230u, 255u);
	}
	else
	{
		const float note_w = note ? gc_text_width(GC_FONT_TEXT, NOTE, note) + 2.0f : 0.0f;
		const float size = gc_fit(GC_FONT_BOLD, TITLE, title, iw - 6.0f - note_w);

		gc_text(c, GC_FONT_BOLD, size, ix + 3.0f, gc_middle(GC_FONT_BOLD, size, iy, HEADER), title, HEADER_TEXT,
			GC_LEFT);
		if (note)
			gc_text(c, GC_FONT_TEXT, NOTE, ix + iw - 3.0f, gc_middle(GC_FONT_TEXT, NOTE, iy, HEADER), note,
				HEADER_NOTE, GC_RIGHT);
	}
	b.x = ix + PAD;
	b.y = iy + HEADER + 0.5f + PAD * 0.8f;
	b.w = iw - 2.0f * PAD;
	b.h = ih - HEADER - 0.5f - PAD * 1.6f;
	return b;
}

/* a label at the left and its value at the right, on a line from y, 'height' tall */
static void row(gcanvas *c, const body *b, float y, const char *label, uint32_t label_colour, const char *value,
	uint32_t value_colour)
{
	gc_text(c, GC_FONT_TEXT, SMALL, b->x, gc_middle(GC_FONT_TEXT, SMALL, y, VALUE), label, label_colour, GC_LEFT);
	if (value)
		gc_text(c, GC_FONT_TEXT, VALUE, b->x + b->w, gc_middle(GC_FONT_TEXT, VALUE, y, VALUE), value, value_colour,
			GC_RIGHT);
}

/* 'count' cells across the body, 'filled' lit ('last' the colour of the last cell when all are) */
static void pips(gcanvas *c, const body *b, float y, float height, int count, int filled, uint32_t colour,
	uint32_t last)
{
	const float gap = 1.2f, cell = (b->w - gap * (float)(count - 1)) / (float)count;

	for (int at = 0; at < count; at++)
	{
		const float x = b->x + (float)at * (cell + gap);

		if (at < filled)
			gc_box(c, x, y, cell, height, 0.5f, at == count - 1 ? last : colour, 255u);
		else
		{
			gc_box(c, x, y, cell, height, 0.5f, 0x33507Fu, 255u);
			gc_box(c, x + 0.35f, y + 0.35f, cell - 0.7f, height - 0.7f, 0.4f, INNER, 255u);
		}
	}
}

/* a bar's track and its filled part ('part' 0-1) shaded across */
static void bar(gcanvas *c, float x, float y, float w, float h, float part, const uint32_t *colours, const float *at,
	int count)
{
	gc_box(c, x, y, w, h, h * 0.5f, WELL_EDGE, 255u);
	gc_box(c, x + 0.3f, y + 0.3f, w - 0.6f, h - 0.6f, h * 0.5f - 0.3f, WELL, 255u);
	if (part > 0.0f)
	{
		/* (the shading laid over the whole track, shown as far as the part: its colour says where it is) */
		gcanvas cut = *c;

		cut.clip = (int)((x + 0.3f + (w - 0.6f) * (part > 1.0f ? 1.0f : part)) * c->k + 0.5f);
		gc_gradient_across(&cut, x + 0.3f, y + 0.3f, w - 0.6f, h - 0.6f, h * 0.5f - 0.3f, colours, at, count);
	}
}

/* a score as the game shows it (the game's count and a 0 after), its thousands parted with commas */
static void points_text(char *out, size_t size, uint32_t value, bool sign)
{
	char digits[16], grouped[24];
	int length, at = 0;

	if (value == 0u)
		snprintf(digits, sizeof(digits), "0");
	else
		snprintf(digits, sizeof(digits), "%lu0", (unsigned long)value);
	length = (int)strlen(digits);
	for (int i = 0; i < length; i++)
	{
		grouped[at++] = digits[i];
		if ((length - 1 - i) % 3 == 0 && i < length - 1)
			grouped[at++] = ',';
	}
	grouped[at] = '\0';
	snprintf(out, size, "%s%s", sign ? "+" : "", grouped);
}

static const char *stage_name(int area)
{
	static char text[16];

	snprintf(text, sizeof(text), "%d-%d", area / 6 + 1, area % 6 + 1);
	return text;
}

/* ---- the bee: the game's own sprite ($4496, 32 x 32, palette $17), turned as the picture is ---- */

#define BEE_CODE    0x4496u
#define BEE_PALETTE 0x17u

static unsigned bee_pen(int x, int y)
{
	const uint32_t pixel = (BEE_CODE << 8) + (uint32_t)(y * 32 + x);
	const uint8_t byte = rom_set.sprites[(pixel >> 1) & (rom_set_region_size(CAVE_ROM_SPRITES) - 1u)];

	return (pixel & 1u) ? (byte & 15u) : (byte >> 4);
}

static uint32_t bee_colour(unsigned pen)
{
	const uint16_t value = cave_board.palette[(BEE_PALETTE * 256u + pen) & 0x7FFFu];
	const uint32_t g = value >> 10 & 31u, r = value >> 5 & 31u, b = value & 31u;

	if (!cave_board.palette[BEE_PALETTE * 256u + 2u])
		return pen < 12u ? GOLD : 0xC0E000u;    /* (its palette not set up yet) */
	return (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

/* the bee drawn at a side in pixels, its colours or grey and dim ('lit' false): kept (the colour and its cover, 0 to
   4, of each pixel) while the side and the bee's palette stay the same */
typedef struct
{
	int side;
	uint32_t palette;
	uint32_t *pixels;           /* colour << 8 | cover */
} bee_icon;

static const uint32_t *bee_pixels(int side, bool lit)
{
	static bee_icon icons[2];
	bee_icon *const icon = &icons[lit ? 1 : 0];
	uint32_t palette = 2166136261u;

	for (unsigned pen = 0; pen < 16u; pen++)
		palette = (palette ^ cave_board.palette[(BEE_PALETTE * 256u + pen) & 0x7FFFu]) * 16777619u;
	if (icon->pixels && icon->side == side && icon->palette == palette)
		return icon->pixels;
	free(icon->pixels);
	icon->pixels = malloc((size_t)side * (size_t)side * sizeof(uint32_t));
	icon->side = side;
	icon->palette = palette;
	if (!icon->pixels)
		return NULL;
	for (int py = 0; py < side; py++)
		for (int px = 0; px < side; px++)
		{
			/* (the picture's (u, v) is the sprite's (31 - v, u): the board's screen lies on its side; 2 x 2 samples
			   each pixel, as many of them as are the bee's making its cover) */
			unsigned r = 0, g = 0, b = 0, count = 0;

			for (int s = 0; s < 4; s++)
			{
				const float u = ((float)px + 0.25f + 0.5f * (float)(s & 1)) / (float)side;
				const float v = ((float)py + 0.25f + 0.5f * (float)(s >> 1)) / (float)side;
				const int sx = 31 - (int)(v * 32.0f), sy = (int)(u * 32.0f);
				const unsigned pen = bee_pen(sx < 0 ? 0 : sx > 31 ? 31 : sx, sy < 0 ? 0 : sy > 31 ? 31 : sy);

				if (pen)
				{
					const uint32_t colour = bee_colour(pen);

					r += colour >> 16 & 255u;
					g += colour >> 8 & 255u;
					b += colour & 255u;
					count++;
				}
			}
			if (count)
			{
				r /= count;
				g /= count;
				b /= count;
				if (!lit)
				{
					const unsigned grey = (r * 30u + g * 59u + b * 11u) / 100u * 45u / 100u + 20u;

					r = g = b = grey;
				}
			}
			icon->pixels[py * side + px] = (r << 16 | g << 8 | b) << 8 | count;
		}
	return icon->pixels;
}

/* the bee in a square (units), its colours, or grey and dim ('lit' false) */
static void bee(gcanvas *c, float x, float y, float size, bool lit)
{
	const int left = (int)(x * c->k + 0.5f), top = (int)(y * c->k + 0.5f), side = (int)(size * c->k + 0.5f);
	const uint32_t *icon;

	if (c->hash)
	{
		/* (recorded: its place, size and colours; the palette's 16 colours with them) */
		const float numbers[4] = { x, y, size, c->k };

		gc_record(c, numbers, sizeof(numbers));
		gc_record(c, &lit, sizeof(lit));
		gc_record(c, &cave_board.palette[(BEE_PALETTE * 256u) & 0x7FFFu], 16u * sizeof(uint16_t));
		return;
	}
	if (!rom_set.sprites || side <= 0 || !(icon = bee_pixels(side, lit)))
		return;
	for (int py = 0; py < side; py++)
		for (int px = 0; px < side; px++)
		{
			const uint32_t made = icon[py * side + px];
			const unsigned count = made & 255u, r = made >> 24, g = made >> 16 & 255u, b = made >> 8 & 255u;
			const int dx = left + px, dy = top + py;

			if (count && dx >= 0 && dx < c->w && dy >= 0 && dy < c->h)
			{
				uint32_t *const pixel = c->pixels + (size_t)dy * (size_t)c->w + (size_t)dx;
				const unsigned a = count * 64u - 1u, keep = 255u - a;
				const uint32_t old = *pixel;

				*pixel = (((old >> 16 & 255u) * keep + r * a) / 255u) << 16
					| (((old >> 8 & 255u) * keep + g * a) / 255u) << 8 | (((old & 255u) * keep + b * a) / 255u);
			}
		}
}

/* ---- the left panel ---- */

static float ship_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 8.0f + 2.0f + 3.0f * (VALUE + 1.0f + 3.0f + 2.0f);
}

static void draw_ship(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	static const char *const names[3] = { "TYPE-A", "TYPE-B", "TYPE-C" };
	static const uint32_t colours[3] = { 0xFF6B5Au, SKY, GREEN };
	const bool on = v->showing;
	const body b = gadget(c, x, y, w, h, v->player ? "2P \x80 SHIP" : "1P \x80 SHIP",
		on ? (v->laser ? "LASER STYLE" : "SHOT STYLE") : NULL);
	if (chrome_pass)
		return;
	char text[24];
	float at = b.y;

	gc_text(c, GC_FONT_BOLD, 7.5f, b.x, gc_middle(GC_FONT_BOLD, 7.5f, at, 8.0f), on ? names[v->ship] : "------",
		on ? colours[v->ship] : DIM, GC_LEFT);
	at += 10.0f;
	for (int weapon = 0; weapon < 2; weapon++)
	{
		/* (the style's own weapon first) */
		const unsigned level = weapon == 0 ? v->own_power : v->other_power;
		const bool is_laser = (weapon == 0) == v->laser;

		if (level >= 4u)
			snprintf(text, sizeof(text), "MAX");
		else
			snprintf(text, sizeof(text), "LV %u", level + 1u);
		row(c, &b, at, is_laser ? "LASER" : "SHOT", on ? LABEL : DIM, on ? text : "-",
			!on ? DIM : level >= 4u ? GOLD : NUMBER);
		at += VALUE + 1.0f;
		pips(c, &b, at, 3.0f, 5, on ? (int)level + 1 : 0, ORANGE, GOLD);
		at += 5.0f;
	}
	snprintf(text, sizeof(text), "%u / %u", v->bombs, v->bombs_most);
	row(c, &b, at, "BOMBS", on ? LABEL : DIM, on ? text : "-", on ? NUMBER : DIM);
	at += VALUE + 1.0f;
	pips(c, &b, at, 3.0f, v->bombs_most >= 3u && v->bombs_most <= 6u ? (int)v->bombs_most : 3, on ? (int)v->bombs : 0,
		YELLOW, YELLOW);
}

static float rank_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 4.0f + 1.4f + 3.6f;
}

static void draw_rank(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	static const uint32_t colours[3] = { GREEN, YELLOW, RED };
	static const float at[3] = { 0.0f, 0.5f, 1.0f };
	char note[24];
	body b;

	snprintf(note, sizeof(note), "%u / 63", v->rank);
	b = gadget(c, x, y, w, h, "RANK", v->showing ? note : "- / 63");
	if (chrome_pass)
		return;
	bar(c, b.x, b.y, b.w, 4.0f, v->showing ? (float)v->rank / 63.0f : 0.0f, colours, at, 3);
	gc_text(c, GC_FONT_TEXT, 3.4f, b.x, b.y + 4.0f + 1.4f + 3.0f, "EASY", DIM, GC_LEFT);
	gc_text(c, GC_FONT_TEXT, 3.4f, b.x + b.w, b.y + 4.0f + 1.4f + 3.0f, "HARD", DIM, GC_RIGHT);
}

static float chain_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 15.0f + 2.0f + 3.4f + 2.4f + 12.0f;
}

static void draw_chain(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	static const uint32_t colours[2] = { TEAL, CYAN };
	static const float at[2] = { 0.0f, 1.0f };
	const bool on = v->showing && v->chain_known;
	char note[24], number[16];
	body b;
	float size, line;

	snprintf(note, sizeof(note), "BEST %u", v->best_hits);
	b = gadget(c, x, y, w, h, "CHAIN", on ? note : NULL);
	if (chrome_pass)
		return;
	snprintf(number, sizeof(number), "%u", on ? v->hits : 0u);
	size = gc_fit(GC_FONT_BOLD, 15.0f, number, b.w - gc_text_width(GC_FONT_BOLD, 6.0f, "HIT") - 3.0f);
	line = b.y + 13.0f;
	gc_text_outlined(c, GC_FONT_BOLD, size, b.x, line, number, on ? HEADER_TEXT : DIM, on ? CYAN : WELL_EDGE, 0.45f,
		GC_LEFT);
	gc_text(c, GC_FONT_BOLD, 6.0f, b.x + gc_text_width(GC_FONT_BOLD, size, number) + 2.0f, line, "HIT",
		on ? PINK : DIM, GC_LEFT);
	bar(c, b.x, b.y + 17.0f, b.w, 3.4f, on ? (float)v->chain_timer / 56.0f : 0.0f, colours, at, 2);
	{
		/* this chain's hits: by the shot, the laser, bombs */
		static const char *const names[3] = { "SHOT", "LASER", "BOMB" };
		const unsigned counts[3] = { v->shot_hits, v->laser_hits, v->bomb_hits };
		const float gap = 1.6f, cell = (b.w - 2.0f * gap) / 3.0f, top = b.y + 22.8f;

		for (int at_cell = 0; at_cell < 3; at_cell++)
		{
			const float cx = b.x + (float)at_cell * (cell + gap);

			gc_box(c, cx, top, cell, 12.0f, 1.2f, WELL, 255u);
			gc_text(c, GC_FONT_TEXT, 3.3f, cx + 1.6f, top + 4.2f, names[at_cell], on ? LABEL : DIM, GC_LEFT);
			snprintf(number, sizeof(number), "%u", counts[at_cell]);
			gc_text(c, GC_FONT_BOLD, gc_fit(GC_FONT_BOLD, 5.8f, number, cell - 3.2f), cx + 1.6f, top + 10.4f, on ? number
				: "-", on ? NUMBER : DIM, GC_LEFT);
		}
	}
}

static float board_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 11.0f + 1.6f + VALUE + 1.4f + 9.0f + 1.6f + 4.0f;
}

static void draw_board(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	char text[32];
	const body b = gadget(c, x, y, w, h, "BOARD", "OF 57.55");
	if (chrome_pass)
		return;
	float at = b.y;

	if (v->meter.on)
	{
		/* the FPS meter (F9): its second's figures, two to a line */
		const struct { const char *name; double value; const char *format; uint32_t colour; } rows[6] = {
			{ "GAME", v->meter.game, "%.1f", v->meter.game < v->meter.board - 0.5 ? YELLOW : GREEN },
			{ "BOARD", v->meter.board, "%.1f", v->meter.board < 57.0 ? RED : NUMBER },
			{ "WINDOW", v->meter.windows, "%.1f", NUMBER },
			{ "LATE", (double)v->meter.late, "%.0f", v->meter.late ? RED : NUMBER },
			{ "WORK MS", v->meter.work, "%.1f", NUMBER },
			{ "MOST MS", v->meter.work_most, "%.1f", v->meter.work_most > 17.4 ? YELLOW : NUMBER },
		};

		gc_text(c, GC_FONT_BOLD, 4.6f, b.x + b.w * 0.5f, at + 4.2f, "FPS METER", GOLD, GC_CENTRE);
		for (int line = 0; line < 6; line++)
		{
			const body half = { b.x + (line % 2 ? b.w * 0.5f + 1.5f : 0.0f), b.y, b.w * 0.5f - 1.5f, b.h };

			snprintf(text, sizeof(text), rows[line].format, rows[line].value);
			row(c, &half, at + 6.5f + (float)(line / 2) * 8.0f, rows[line].name, LABEL, text, rows[line].colour);
		}
		return;
	}
	{
		const uint32_t colour = v->slowdown_now == 0u ? GREEN : v->slowdown_now < 25u ? YELLOW : RED;

		snprintf(text, sizeof(text), "%.1f", v->game_rate);
		gc_text(c, GC_FONT_BOLD, 11.0f, b.x, at + 9.5f, text, colour, GC_LEFT);
		gc_text(c, GC_FONT_TEXT, SMALL, b.x + gc_text_width(GC_FONT_BOLD, 11.0f, text) + 1.5f, at + 9.5f, "FPS", LABEL,
			GC_LEFT);
		snprintf(text, sizeof(text), "%u%%", v->slowdown_now);
		gc_text(c, GC_FONT_TEXT, VALUE, b.x + b.w, at + 9.5f, text, colour == GREEN ? NUMBER : colour, GC_RIGHT);
		gc_text(c, GC_FONT_TEXT, 3.2f, b.x + b.w - gc_text_width(GC_FONT_TEXT, VALUE, text) - 1.5f, at + 9.5f,
			"SLOWDOWN", LABEL, GC_RIGHT);
	}
	at += 12.6f;
	snprintf(text, sizeof(text), "%d", v->bullets_now);
	row(c, &b, at, "BULLETS", LABEL, text, NUMBER);
	at += VALUE + 1.4f;
	{
		/* the last 5 seconds: a column for the bullets each frame (as many as the graph has room for), red where
		   the game slowed down */
		const float left = b.x, width = b.w, top = at, height = 9.0f;
		int most = 1;

		gc_box(c, left, top, width, height, 0.8f, WELL_EDGE, 255u);
		gc_box(c, left + 0.3f, top + 0.3f, width - 0.6f, height - 0.6f, 0.6f, 0x071430u, 255u);
		for (int i = 0; i < v->history_count; i++)
			most = v->bullets[i] > most ? v->bullets[i] : most;
		most = most < 40 ? 40 : most;
		{
			const int columns = (int)((width - 1.0f) * c->k);

			for (int col = 0; col < columns && v->history_count > 0; col++)
			{
				/* (each pixel's column: the frames that fall in it, their most and any slowdown) */
				const int from = WIDGET_HISTORY - (columns - col) * WIDGET_HISTORY / columns;
				const int to = WIDGET_HISTORY - (columns - col - 1) * WIDGET_HISTORY / columns;
				int count = 0;
				bool slowed = false;

				for (int f = from; f < to; f++)
				{
					const int index = f - (WIDGET_HISTORY - v->history_count);

					if (index < 0)
						continue;
					count = v->bullets[index] > count ? v->bullets[index] : count;
					slowed |= v->lagged[index];
				}
				if (count > 0)
				{
					const float tall = (height - 1.2f) * (float)count / (float)most;

					gc_box(c, left + 0.5f + (float)col / c->k, top + height - 0.6f - tall, 1.0f / c->k, tall, 0.0f,
						slowed ? RED : 0xC99A2Eu, 255u);
				}
			}
		}
	}
	at += 9.0f + 1.6f;
	{
		const unsigned stage = v->stage_frames ? (v->stage_lagged * 100u + v->stage_frames / 2u) / v->stage_frames : 0u;
		const unsigned game = v->game_frames ? (v->game_lagged * 100u + v->game_frames / 2u) / v->game_frames : 0u;

		snprintf(text, sizeof(text), v->showing ? "THIS STAGE %u%%" : "THIS STAGE -", stage);
		gc_text(c, GC_FONT_TEXT, 3.4f, b.x, at + 3.0f, text, LABEL, GC_LEFT);
		snprintf(text, sizeof(text), v->on ? "GAME %u%%" : "GAME -", game);
		gc_text(c, GC_FONT_TEXT, 3.4f, b.x + b.w, at + 3.0f, text, LABEL, GC_RIGHT);
	}
}

static void draw_bees(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	char title[24], note[16];
	body b;

	if (v->bee_area >= 0)
		snprintf(title, sizeof(title), "BEES \x80 %s", stage_name(v->bee_area));
	else
		snprintf(title, sizeof(title), "BEES");
	snprintf(note, sizeof(note), "%u / 13", v->bees_taken);
	b = gadget(c, x, y, w, h, title, v->on ? note : NULL);
	if (chrome_pass)
		return;
	{
		/* 7 to a row (the last row centred), as big as the body's width allows and its height */
		const float gap = 1.6f;
		float cell = (b.w - 6.0f * gap) / 7.0f;

		if (2.0f * cell + gap > b.h)
			cell = (b.h - gap) / 2.0f;
		for (int at = 0; at < 13; at++)
		{
			const int line = at / 7, column = at % 7, in_line = line ? 6 : 7;
			const float left = b.x + (b.w - ((float)in_line * cell + (float)(in_line - 1) * gap)) * 0.5f;
			const float cx = left + (float)column * (cell + gap), cy = b.y + (float)line * (cell + gap);
			const bool taken = (unsigned)at < v->bees_taken, missed = !taken && v->bee_area_over;
			const uint32_t edge = taken ? GOLD : missed ? RED : WELL_EDGE;

			gc_box(c, cx, cy, cell, cell, 1.2f, edge, 255u);
			gc_box(c, cx + 0.45f, cy + 0.45f, cell - 0.9f, cell - 0.9f, 0.9f,
				taken ? 0x2A2208u : missed ? 0x2A0C14u : WELL, 255u);
			bee(c, cx + cell * 0.08f, cy + cell * 0.08f, cell * 0.84f, taken);
			if (missed)
			{
				gc_line(c, cx + cell * 0.25f, cy + cell * 0.25f, cx + cell * 0.75f, cy + cell * 0.75f, cell * 0.1f, RED,
					255u);
				gc_line(c, cx + cell * 0.75f, cy + cell * 0.25f, cx + cell * 0.25f, cy + cell * 0.75f, cell * 0.1f, RED,
					255u);
			}
		}
	}
}

static float bees_least(float w)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 2.0f * ((w - 2.0f * FRAME - 2.0f * PAD - 9.6f) / 7.0f) + 1.6f;
}

static void draw_left(gcanvas *full, const widgets_view *v, float width)
{
	const float heights[4] = { ship_height(), rank_height(), chain_height(), board_height() };
	const float fixed = heights[0] + heights[1] + heights[2] + heights[3] + 4.0f * GAP;
	float height, w, used, gap, y = MARGIN;
	gcanvas zoom, *c = &zoom;

	if (chrome_pass)
		background(full, width, (float)WIDGET_H);
	/* (the bees' least height is the width's: zoomed, the width is less; once again with the zoomed width) */
	zoom = zoomed(full, fixed + bees_least(width - 2.0f * MARGIN), &width, &height);
	w = width - 2.0f * MARGIN;
	used = heights[0] + heights[1] + heights[2] + heights[3] + bees_least(w);
	gap = (height - 2.0f * MARGIN - used) / 4.0f;
	if (gap > 12.0f)
		gap = 12.0f;
	if (gap < GAP * 0.5f)
		gap = GAP * 0.5f;
	part(c, 0, draw_ship, v, MARGIN, y, w, heights[0]);
	y += heights[0] + gap;
	part(c, 1, draw_rank, v, MARGIN, y, w, heights[1]);
	y += heights[1] + gap;
	part(c, 2, draw_chain, v, MARGIN, y, w, heights[2]);
	y += heights[2] + gap;
	part(c, 3, draw_board, v, MARGIN, y, w, heights[3]);
	y += heights[3] + gap;
	part(c, 4, draw_bees, v, MARGIN, y, w, height - MARGIN - y);
}

/* ---- the right panel ---- */

static float score_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 15.0f + 2.0f + 4.0f * (VALUE + 1.6f);
}

static void draw_score(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	char text[32], value[32];
	const bool on = v->on;
	body b;
	float at, size;

	if (v->showing && v->stages[0].state != WIDGET_STAGE_TODO)
	{
		int live = -1;

		for (int stage = 0; stage < 6; stage++)
			if (v->stages[stage].state == WIDGET_STAGE_LIVE)
				live = stage;
		snprintf(text, sizeof(text), "%s", live >= 0 ? stage_name(v->loop_shown * 6 + live) : "");
	}
	else
		text[0] = '\0';
	b = gadget(c, x, y, w, h, "SCORE", text[0] ? text : NULL);
	if (chrome_pass)
		return;
	points_text(value, sizeof(value), v->score, false);
	size = gc_fit(GC_FONT_BOLD, 15.0f, value, b.w - 1.0f);
	gc_text_outlined(c, GC_FONT_BOLD, size, b.x + b.w, b.y + 13.0f, value, on ? HEADER_TEXT : DIM, on ? CYAN : WELL_EDGE,
		0.5f, GC_RIGHT);
	at = b.y + 17.0f;
	if (!on)
	{
		row(c, &b, at, "NO GAME", DIM, NULL, DIM);
		return;
	}
	if (v->chasing >= 0)
	{
		snprintf(text, sizeof(text), "NEXT #%d", v->chasing + 1);
		points_text(value, sizeof(value), v->chase, false);
		row(c, &b, at, text, places[v->chasing], value, places[v->chasing]);
		at += VALUE + 1.6f;
		points_text(value, sizeof(value), v->chase > v->score ? v->chase - v->score : 0u, false);
		row(c, &b, at, "TO GO", LABEL, value, NUMBER);
	}
	else
	{
		row(c, &b, at, "PAST #1!", GOLD, NULL, GOLD);
		at += VALUE + 1.6f;
		row(c, &b, at, "THE TOP SCORE", LABEL, NULL, LABEL);
	}
	at += VALUE + 1.6f;
	if (v->next_extend)
		points_text(value, sizeof(value), v->next_extend > v->score ? v->next_extend - v->score : 0u, false);
	row(c, &b, at, "1UP IN", v->next_extend ? EXTEND : DIM, v->next_extend ? value : "NO MORE",
		v->next_extend ? NUMBER : DIM);
	at += VALUE + 1.6f;
	row(c, &b, at, v->loop2_title, GOLD, v->loop2_text, v->loop2_colour == 0x40404Cu ? DIM : v->loop2_colour);
}

static float stages_height(bool two_lines)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 6.0f * (two_lines ? 12.4f : 8.4f);
}

/* how a stage went against its best: "+312,000", "-88,400", "NEW BEST" ... */
static void difference(char *out, size_t size, uint32_t made, uint32_t against)
{
	char value[24];

	points_text(value, sizeof(value), made > against ? made - against : against - made, false);
	snprintf(out, size, "%s%s", made >= against ? "+" : "-", value);
}

static void draw_stages(gcanvas *c, const widgets_view *v, float x, float y, float w, float h, bool two_lines)
{
	char title[32];
	body b;

	snprintf(title, sizeof(title), "STAGES \x80 LOOP %d", v->loop_shown + 1);
	b = gadget(c, x, y, w, h, title, two_lines ? NULL : "VS YOUR BEST");
	if (chrome_pass)
		return;
	for (int stage = 0; stage < 6; stage++)
	{
		const float line = two_lines ? 12.4f : 8.4f, top = b.y + (float)stage * line;
		const int area = v->loop_shown * 6 + stage, state = v->stages[stage].state;
		const uint32_t made = v->stages[stage].made, best = v->stages[stage].best;
		char score[40], note[48];
		uint32_t label_colour = LABEL, score_colour = NUMBER, note_colour = LABEL;
		const float base = top + 5.8f;

		if (stage > 0)
			gc_box(c, b.x, top - 0.6f, b.w, 0.25f, 0.0f, 0x16305Au, 255u);
		note[0] = '\0';
		if (state == WIDGET_STAGE_LIVE)
		{
			label_colour = YELLOW;
			points_text(score, sizeof(score), made, false);
			if (!v->stages[stage].counts)
				snprintf(note, sizeof(note), "UNRANKED");
			else if (best)
			{
				difference(note, sizeof(note), made, best);
				note_colour = made > best ? GOLD : LABEL;
			}
		}
		else if (state == WIDGET_STAGE_DONE)
		{
			points_text(score, sizeof(score), made, false);
			if (v->stages[stage].new_best)
				score_colour = GOLD;
			if (!v->stages[stage].counts)
			{
				snprintf(note, sizeof(note), "UNRANKED");
				note_colour = DIM;
			}
			else if (!best)
			{
				snprintf(note, sizeof(note), "NEW BEST");
				note_colour = GOLD;
			}
			else
			{
				char diff[32];

				difference(diff, sizeof(diff), made, best);
				snprintf(note, sizeof(note), "%s%s", v->stages[stage].new_best ? "BEST " : "", diff);
				note_colour = v->stages[stage].new_best ? GOLD : RED;
			}
		}
		else
		{
			label_colour = score_colour = DIM;
			if (best)
			{
				char value[24];

				points_text(value, sizeof(value), best, false);
				snprintf(score, sizeof(score), "BEST %s", value);
			}
			else
				snprintf(score, sizeof(score), "-");
		}
		gc_text(c, GC_FONT_BOLD, 5.0f, b.x, base, stage_name(area), label_colour, GC_LEFT);
		if (two_lines)
		{
			gc_text(c, GC_FONT_TEXT, gc_fit(GC_FONT_TEXT, VALUE, score, b.w - 12.0f), b.x + b.w, base, score,
				score_colour, GC_RIGHT);
			if (note[0])
				gc_text(c, GC_FONT_TEXT, 3.6f, b.x + b.w, base + 5.0f, note, note_colour, GC_RIGHT);
		}
		else
		{
			const float note_w = note[0] ? gc_text_width(GC_FONT_TEXT, 3.6f, note) + 3.0f : 0.0f;

			gc_text(c, GC_FONT_TEXT, gc_fit(GC_FONT_TEXT, VALUE, score, b.w - 12.0f - note_w),
				b.x + b.w - note_w, base, score, score_colour, GC_RIGHT);
			if (note[0])
				gc_text(c, GC_FONT_TEXT, 3.6f, b.x + b.w, base, note, note_colour, GC_RIGHT);
		}
	}
}

static float last_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 8.0f;
}

static void draw_last(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	char note[24], value[24];
	const bool on = v->on && v->gained;
	body b;

	snprintf(note, sizeof(note), "AT %u HIT", v->last_gain_hits);
	b = gadget(c, x, y, w, h, "LAST SCORED", on && v->last_gain_hits ? note : NULL);
	if (chrome_pass)
		return;
	points_text(value, sizeof(value), on ? v->last_gain : 0u, on);
	gc_text(c, GC_FONT_BOLD, gc_fit(GC_FONT_BOLD, 8.0f, value, b.w), b.x + b.w, b.y + 7.0f, on ? value : "-",
		on ? GOLD : DIM, GC_RIGHT);
}

static float input_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 14.0f;
}

static void draw_input(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	const body b = gadget(c, x, y, w, h, "INPUT", NULL);
	if (chrome_pass)
		return;
	const bool on = v->showing;
	const uint16_t held = on ? v->held : 0u;
	const int dx = held & CAVE_LEFT ? -1 : held & CAVE_RIGHT ? 1 : 0, dy = held & CAVE_UP ? -1 : held & CAVE_DOWN ? 1 : 0;
	static const struct { uint16_t bit; const char *name; } buttons[3] = {
		{ CAVE_BUTTON1, "S" }, { CAVE_BUTTON2, "B" }, { CAVE_BUTTON3, "C" }
	};
	const float cell = 4.2f, stick = 3.0f * cell + 2.0f * 0.8f, radius = 5.6f;
	const float together = stick + 4.0f + 3.0f * 2.0f * radius + 2.0f * 2.4f;
	const float left = b.x + (b.w - together) * 0.5f, top = b.y + (b.h - stick) * 0.5f;

	for (int row_at = -1; row_at <= 1; row_at++)
		for (int column = -1; column <= 1; column++)
		{
			const float cx = left + (float)(column + 1) * (cell + 0.8f), cy = top + (float)(row_at + 1) * (cell + 0.8f);
			const bool lit = row_at == dy && column == dx && !(row_at == 0 && column == 0);

			gc_box(c, cx, cy, cell, cell, 0.7f, lit ? 0xFFE08Au : WELL_EDGE, 255u);
			gc_box(c, cx + 0.3f, cy + 0.3f, cell - 0.6f, cell - 0.6f, 0.5f,
				lit ? GOLD : row_at == 0 && column == 0 ? 0x1C3A6Bu : WELL, 255u);
		}
	for (int button = 0; button < 3; button++)
	{
		const float cx = left + stick + 4.0f + radius + (float)button * (2.0f * radius + 2.4f), cy = top + stick * 0.5f;
		const bool lit = (held & buttons[button].bit) != 0;

		gc_disc(c, cx, cy, radius, lit ? 0xFFE08Au : WELL_EDGE, 255u);
		gc_disc(c, cx, cy, radius - 0.5f, lit ? GOLD : WELL, 255u);
		gc_text(c, GC_FONT_BOLD, 5.0f, cx, gc_middle(GC_FONT_BOLD, 5.0f, cy - 2.5f, 5.0f), buttons[button].name,
			lit ? 0x1A1206u : (on ? LABEL : DIM), GC_CENTRE);
	}
}

static float goals_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 5.0f * 6.6f;
}

/* The goals: the achievements not yet earned that this game is nearest to (achievements_goal), chosen again as a
   stage begins and every 10 seconds (between, a line keeps its place: met, it is ticked; lost, crossed); under
   them, to make five, the stage's own (NO MISS, NO BOMB, ALL 13 BEES, the next HITS, SURVIVOR) with a gold dot when
   earned before. With no game that counts (training, a replay, a cheat): the stage's own, as they stand */
#define GOALS 5
#define GOALS_EVERY 575    /* (frames drawn: 10 seconds) */

static struct
{
	int index[GOALS], count;
	int stage;
	unsigned frames;
} goals;

static void goals_choose(void)
{
	const game_watch_state *const g = &game_watch;
	static const char *const own[5] = { "no_miss_stage", "no_bomb_stage", "bees_stage", "hit100", "survivor" };
	int order[64], found = 0;
	float parts[64];

	goals.frames++;
	if (!g->on || !g->counts || g->stage < 0)
	{
		goals.count = 0;
		return;
	}
	if (goals.count && goals.stage == g->stage && goals.frames < GOALS_EVERY)
		return;
	goals.stage = g->stage;
	goals.frames = 0;
	/* (the ones not earned and in reach, nearest first) */
	for (int index = 0; index < achievements_count() && found < 64; index++)
	{
		achievement_goal goal;

		if (achievements_date(index) || !achievements_goal(index, &goal) || goal.state == GOAL_LOST)
			continue;
		{
			int at = found++;

			while (at > 0 && parts[at - 1] < goal.part)
			{
				order[at] = order[at - 1];
				parts[at] = parts[at - 1];
				at--;
			}
			order[at] = index;
			parts[at] = goal.part;
		}
	}
	goals.count = 0;
	for (int at = 0; at < found && goals.count < GOALS; at++)
		goals.index[goals.count++] = order[at];
	/* (the stage's own after them: the next HITS as achievements_goal has it) */
	for (int at = 0; at < 5 && goals.count < GOALS; at++)
	{
		int index = achievements_find(own[at]);
		bool shown = false;

		if (at == 3)
			for (int step = 0; step < 2 && index >= 0 && achievements_date(index); step++)
				index++;    /* (hit100, hit500, hit1000: one after another in the list) */
		for (int other = 0; other < goals.count; other++)
			shown |= goals.index[other] == index;
		if (index >= 0 && !shown)
			goals.index[goals.count++] = index;
	}
}

static void draw_goals(gcanvas *c, const widgets_view *v, float x, float y, float w, float h, bool narrow)
{
	enum { KEPT, LOST, GOING };
	const game_watch_state *const g = &game_watch;
	const bool on = v->showing && g->on;
	static const char *const fixed[5] = { "no_miss_stage", "no_bomb_stage", "bees_stage", "hit100", "survivor" };
	char title[48], note[24];
	body b;

	snprintf(title, sizeof(title), narrow ? "GOALS" : "ACHIEVEMENTS");
	if (v->showing && v->bee_area >= 0)
		snprintf(title + strlen(title), sizeof(title) - strlen(title), " \x80 %s", stage_name(v->bee_area));
	snprintf(note, sizeof(note), "%d / %d", achievements_done(), achievements_count());
	b = gadget(c, x, y, w, h, title, note);
	if (chrome_pass)
		return;
	for (int line = 0; line < GOALS; line++)
	{
		const float top = b.y + (float)line * 6.6f, box_size = 4.4f;
		int index = goals.count ? (line < goals.count ? goals.index[line] : -1) : achievements_find(fixed[line]);
		achievement_goal goal;
		bool earned, have;
		int state;
		uint32_t name_colour;

		if (!goals.count && line == 3)
			for (int step = 0; step < 2 && index >= 0 && achievements_date(index); step++)
				index++;    /* (the next HITS) */
		if (index < 0)
			continue;
		have = achievements_goal(index, &goal);
		if (!have && !goals.count)
		{
			/* (no game: the stage's own, by name) */
			static const char *const labels[5] = { "NO MISS", "NO BOMB", "ALL 13 BEES", "HITS", "SURVIVOR" };
			static const char *const hit_labels[3] = { "100 HITS", "500 HITS", "1000 HITS" };

			const int hit = index - achievements_find("hit100");

			goal.label = line == 3 ? hit_labels[hit > 2 ? 2 : hit < 0 ? 0 : hit] : labels[line];
			goal.value[0] = '\0';
			goal.state = GOAL_GOING;
			goal.part = 0.0f;
		}
		else if (!have)
		{
			/* (out of this game's reach since it was chosen: its name, crossed) */
			goal.label = achievements_name(index);
			goal.value[0] = '\0';
			goal.state = GOAL_LOST;
			goal.part = 0.0f;
		}
		earned = achievements_date(index) != NULL;
		state = goal.state == GOAL_LOST ? LOST : goal.state == GOAL_KEPT ? KEPT : GOING;
		if (index == achievements_find("bees_stage") && state == GOING && v->bee_area_over)
			state = LOST;    /* (the stage's bees over: one was missed) */
		name_colour = !on ? DIM : state == LOST ? 0x7F98C2u : NUMBER;
		if (!on)
			gc_box(c, b.x, top, box_size, box_size, 0.8f, 0x1C3A6Bu, 255u);
		else if (state == KEPT)
		{
			gc_box(c, b.x, top, box_size, box_size, 0.8f, TEAL, 255u);
			gc_line(c, b.x + 0.9f, top + 2.3f, b.x + 1.9f, top + 3.3f, 0.6f, INNER, 255u);
			gc_line(c, b.x + 1.9f, top + 3.3f, b.x + 3.6f, top + 1.0f, 0.6f, INNER, 255u);
		}
		else if (state == LOST)
		{
			gc_box(c, b.x, top, box_size, box_size, 0.8f, 0x5A1620u, 255u);
			gc_line(c, b.x + 1.1f, top + 1.1f, b.x + 3.3f, top + 3.3f, 0.6f, RED, 255u);
			gc_line(c, b.x + 3.3f, top + 1.1f, b.x + 1.1f, top + 3.3f, 0.6f, RED, 255u);
		}
		else
		{
			/* (under way: the box filled up from the bottom as it nears) */
			const float filled = (box_size - 1.0f) * (goal.part < 0.0f ? 0.0f : goal.part > 1.0f ? 1.0f : goal.part);

			gc_box(c, b.x, top, box_size, box_size, 0.8f, 0x1C3A6Bu, 255u);
			if (filled > 0.15f)
				gc_box(c, b.x + 0.5f, top + box_size - 0.5f - filled, box_size - 1.0f, filled, 0.4f, 0x3B7CF0u, 255u);
		}
		gc_text(c, GC_FONT_TEXT, 4.0f, b.x + box_size + 2.0f, top + 3.7f, goal.label, name_colour, GC_LEFT);
		if (earned)
			gc_disc(c, b.x + box_size + 2.0f + gc_text_width(GC_FONT_TEXT, 4.0f, goal.label) + 1.8f, top + 2.2f, 0.9f,
				GOLD, 255u);
		if (on && goal.value[0])
		{
			/* (the value made to fit beside the name) */
			const float room = b.w - box_size - 2.0f - gc_text_width(GC_FONT_TEXT, 4.0f, goal.label) - 4.5f;

			gc_text(c, GC_FONT_TEXT, gc_fit(GC_FONT_TEXT, 3.6f, goal.value, room), b.x + b.w, top + 3.7f, goal.value,
				LABEL, GC_RIGHT);
		}
	}
}

/* an achievement earned: its box over the score's gadget, as the classic panel's */
static void draw_toast(gcanvas *c, const widgets_view *v, float width)
{
	const float x = MARGIN, y = MARGIN, w = width - 2.0f * MARGIN, h = 40.0f;
	const int shown = 230 - v->toast_frames;
	const uint32_t rim = shown < 30 && shown / 5 % 2 ? 0xFFFFFFu : GOLD;
	char more[24];

	gc_box(c, x - 0.3f, y + 0.6f, w + 0.6f, h + 0.6f, RADIUS, 0x000000u, 140u);
	gc_box(c, x, y, w, h, RADIUS, rim, 255u);
	gc_box(c, x + 0.9f, y + 0.9f, w - 1.8f, h - 1.8f, RADIUS - 0.6f, 0x14142Au, 255u);
	gc_text(c, GC_FONT_BOLD, 4.4f, x + w * 0.5f, y + 8.0f, "ACHIEVEMENT UNLOCKED", GOLD, GC_CENTRE);
	gc_text(c, GC_FONT_BOLD, gc_fit(GC_FONT_BOLD, 8.0f, v->toast, w - 6.0f), x + w * 0.5f, y + 21.0f, v->toast,
		HEADER_TEXT, GC_CENTRE);
	if (v->toast_more > 0)
	{
		snprintf(more, sizeof(more), "+%d MORE", v->toast_more);
		gc_text(c, GC_FONT_TEXT, 3.6f, x + w * 0.5f, y + 31.0f, more, LABEL, GC_CENTRE);
	}
}

static bool stages_two_lines, goals_narrow;

static void part_stages(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	draw_stages(c, v, x, y, w, h, stages_two_lines);
}

static void part_goals(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	draw_goals(c, v, x, y, w, h, goals_narrow);
}

static void draw_right(gcanvas *full, const widgets_view *v, float width)
{
	const bool wide = width >= 150.0f;
	float heights[5] = { score_height(), stages_height(!wide), last_height(), input_height(), goals_height() };
	float height, w, used, gap, y = MARGIN;
	bool two_lines, narrow;
	gcanvas zoom, *c = &zoom;

	if (chrome_pass)
		background(full, width, (float)WIDGET_H);
	{
		/* (zoomed narrower than 150 units, the stages take two lines each: zoomed again for those) */
		const float full_width = width;

		zoom = zoomed(full, heights[0] + heights[1] + heights[2] + heights[3] + heights[4] + 4.0f * GAP, &width,
			&height);
		if (wide && width < 150.0f)
		{
			width = full_width;
			zoom = zoomed(full, heights[0] + stages_height(true) + heights[2] + heights[3] + heights[4] + 4.0f * GAP,
				&width, &height);
		}
	}
	w = width - 2.0f * MARGIN;
	two_lines = width < 150.0f;
	narrow = width < 140.0f;
	heights[1] = stages_height(two_lines);
	used = heights[0] + heights[1] + heights[2] + heights[3] + heights[4];
	gap = (height - 2.0f * MARGIN - used) / 4.0f;
	if (gap > 12.0f)
		gap = 12.0f;
	if (gap < GAP * 0.5f)
		gap = GAP * 0.5f;
	stages_two_lines = two_lines;
	goals_narrow = narrow;
	part(c, 0, draw_score, v, MARGIN, y, w, heights[0]);
	y += heights[0] + gap;
	part(c, 1, part_stages, v, MARGIN, y, w, heights[1]);
	y += heights[1] + gap;
	part(c, 2, draw_last, v, MARGIN, y, w, heights[2]);
	y += heights[2] + gap;
	part(c, 3, draw_input, v, MARGIN, y, w, heights[3]);
	y += heights[3] + gap;
	part(c, 4, part_goals, v, MARGIN, y, w, height - MARGIN - y);
	if (v->toast_frames > 0 && !chrome_pass)
		draw_toast(c, v, width);
}

/* ---- the panels ---- */

static int clamp_width(int width)
{
	return width < WIDGET_W ? WIDGET_W : width > WIDGET_MAX_W ? WIDGET_MAX_W : width;
}

static struct
{
	int width, k;
	uint32_t *left, *right;
} chrome;

void gadgets_invalidate(void)
{
	kept.fresh[0] = kept.fresh[1] = false;
}

void gadgets_draw(uint32_t *left, uint32_t *right, int width, int k, gadgets_dirty *dirty)
{
	widgets_view v;
	gcanvas c;
	size_t pixels;
	uint32_t *const outs[2] = { left, right };

	width = clamp_width(width);
	pixels = (size_t)width * (size_t)k * (size_t)WIDGET_H * (size_t)k;
	widgets_view_now(&v);
	c.w = width * k;
	c.h = WIDGET_H * k;
	c.k = (float)k;
	c.clip = 0;
	c.hash = NULL;
	if (dirty)
		dirty->count[0] = dirty->count[1] = 0;
	if (chrome.width != width || chrome.k != k)
	{
		/* the chrome for this width and k: drawn once */
		free(chrome.left);
		free(chrome.right);
		chrome.width = chrome.k = 0;
		gadgets_invalidate();
		chrome.left = malloc(pixels * sizeof(uint32_t));
		chrome.right = malloc(pixels * sizeof(uint32_t));
		if (chrome.left && chrome.right)
		{
			chrome_pass = true;
			c.pixels = chrome.left;
			draw_left(&c, &v, (float)width);
			c.pixels = chrome.right;
			draw_right(&c, &v, (float)width);
			chrome_pass = false;
			chrome.width = width;
			chrome.k = k;
		}
	}
	/* (the right side whole while an achievement's box is over it and the frame after, and after the help) */
	goals_choose();
	if (v.toast_frames > 0 || kept.toast || kept.help)
		kept.fresh[1] = false;
	kept.toast = v.toast_frames > 0;
	kept.help = false;
	dirty_now = dirty;
	for (int side = 0; side < 2; side++)
	{
		if (outs[side] != kept.pixels[side])
			kept.fresh[side] = false;
		side_now = side;
		chrome_now = chrome.width ? (side ? chrome.right : chrome.left) : NULL;
		c.pixels = outs[side];
		if (!kept.fresh[side] || !chrome_now)
		{
			/* (drawn whole: the chrome, or drawn again when there is no memory for it) */
			if (chrome_now)
				memcpy(outs[side], chrome_now, pixels * sizeof(uint32_t));
			else
			{
				chrome_pass = true;
				(side ? draw_right : draw_left)(&c, &v, (float)width);
				chrome_pass = false;
			}
			if (dirty)
			{
				const gadgets_rect whole = { 0, 0, c.w, c.h };

				dirty->rect[side][0] = whole;
				dirty->count[side] = 1;
			}
		}
		(side ? draw_right : draw_left)(&c, &v, (float)width);
		kept.fresh[side] = chrome_now != NULL;
		kept.pixels[side] = outs[side];
		if (dirty && dirty->count[side] > 1)
		{
			/* (a whole side once drawn whole: the gadgets' places after it are in it) */
			if (dirty->rect[side][0].w == c.w && dirty->rect[side][0].h == c.h)
				dirty->count[side] = 1;
		}
	}
	dirty_now = NULL;
}

void gadgets_draw_help(uint32_t *pixels, int width, int k, const char *title, const char *text)
{
	gcanvas c;
	body b;
	float y;

	width = clamp_width(width);
	c.w = width * k;
	c.h = WIDGET_H * k;
	c.k = (float)k;
	c.clip = 0;
	c.hash = NULL;
	c.pixels = pixels;
	kept.help = true;
	background(&c, (float)width, (float)WIDGET_H);
	chrome_pass = true;
	gadget(&c, MARGIN, MARGIN, (float)width - 2.0f * MARGIN, (float)WIDGET_H - 2.0f * MARGIN, "HELP", NULL);
	chrome_pass = false;
	b = gadget(&c, MARGIN, MARGIN, (float)width - 2.0f * MARGIN, (float)WIDGET_H - 2.0f * MARGIN, "HELP", NULL);
	y = b.y + 5.0f;
	for (int part = 0; part < 2; part++)
	{
		/* the entry's name (gold, larger), then what it does: in lines broken at their spaces to the body's width */
		const char *at = part ? text : title;
		const int font = part ? GC_FONT_TEXT : GC_FONT_BOLD;
		const float size = part ? 4.2f : 5.4f, line = part ? 6.0f : 7.2f;

		while (*at && y < b.y + b.h)
		{
			char buffer[96];
			int take = 0, last_space = -1;

			while (at[take] && take < (int)sizeof(buffer) - 1)
			{
				buffer[take] = at[take];
				buffer[take + 1] = '\0';
				if (gc_text_width(font, size, buffer) > b.w)
					break;
				if (at[take] == ' ')
					last_space = take;
				take++;
			}
			if (at[take] && last_space > 0)
				take = last_space;
			if (take == 0)
				take = 1;
			snprintf(buffer, sizeof(buffer), "%.*s", take, at);
			gc_text(&c, font, size, b.x, y, buffer, part ? NUMBER : GOLD, GC_LEFT);
			y += line;
			at += take;
			while (*at == ' ')
				at++;
		}
		if (!part)
		{
			gc_box(&c, b.x, y - 3.0f, b.w, 0.3f, 0.0f, WELL_EDGE, 255u);
			y += 3.0f;
		}
	}
}
