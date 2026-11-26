/**
 *    Name:        gadgets.h
 *    Description: the gadget panels: the side panels drawn as blue brushed-metal gadgets at the screen's own
 *                 resolution (SIDE PANELS: GADGETS), the other look of what widgets.h shows (CLASSIC)
 *    Version:     0.1.0 (draft)
 *    Created:     25-11-2026
 *
 *    license:BSD-3-Clause
 *    The design agreed with the owner (25-11-2026, a mockup on a design canvas): left, the ship (its type and style,
 *    the shot's and the laser's power, the bombs), the rank as a bar green to red, the chain (its hits in big
 *    outlined digits, its timer, this chain's hits by the shot, the laser and bombs, the game's best), the board
 *    (the game's frames a second, the slowdown, the bullets and their last 5 seconds), and the stage's 13 bees as
 *    the game's own bee; right, the score chasing the high score table (the next place, how much more, the next
 *    extra life, the second loop's line), the loop's stages against their bests, the last points scored, the
 *    controls as the board is given them, and the stage's achievements as they stand. What they show comes from
 *    widgets.h's run (widgets_view_now), game_watch.h and achievements.h: the game is not touched. The text is
 *    Chakra Petch (gadget_font.h, SIL Open Font License 1.1), drawn by gadget_canvas.h.
 */
#ifndef DDONPACH_GADGETS_H
#define DDONPACH_GADGETS_H

#include <stdint.h>

/* The places of a panel's picture drawn again (pixels), to give the graphics card only those */
typedef struct
{
	int x, y, w, h;
} gadgets_rect;

#define GADGETS_RECTS 8

typedef struct
{
	int count[2];                           /* left, right: 0 when nothing changed */
	gadgets_rect rect[2][GADGETS_RECTS];
} gadgets_dirty;

/* Both panels, 'width' units wide (WIDGET_W to WIDGET_MAX_W, as widgets_draw's) and WIDGET_H tall, at 'k' pixels a
   unit: each 'width' x 'k' pixels a row, WIDGET_H x 'k' rows. Only the gadgets whose contents changed are drawn
   again over last frame's pictures (the same pictures each time: others are drawn whole); 'dirty' (or NULL) gets
   their places */
void gadgets_draw(uint32_t *left, uint32_t *right, int width, int k, gadgets_dirty *dirty);

/* The next gadgets_draw draws both panels whole (their textures lost: a graphics device reset) */
void gadgets_invalidate(void);

/* The right panel as the menu's help: the chosen entry's name and what it does (drawn over gadgets_draw's: give the
   whole panel to the graphics card then) */
void gadgets_draw_help(uint32_t *pixels, int width, int k, const char *title, const char *text);

#endif
