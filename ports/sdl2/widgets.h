/**
 *    Name:        widgets.h
 *    Description: the side panels: what the game is doing, from its memory, beside the picture on a wide screen. Left:
 *                 the player's ship, its shot and laser power (1 to MAX) and bombs, and the board's speed (the game's
 *                 frames a second and the slowdown, now, this stage and this game), and the stage's 13 bees,
 *                 each greyed until it is taken. Right: the score chasing the high
 *                 score table (#5 first, then #4 ... #1) with a graph of the run (a coloured line where each was
 *                 passed), and the loop's stages one under the other, each one's score against the best ever
 *                 for it (+ or -)
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's side panels (ports/sdl2/widgets.c), made again for DoDonPachi, and as wide as the screen
 *    leaves room for (main.c works the width out: WIDGET_W at least). From the game's memory, player 1 / player 2
 *    (found by watching it through whole games: tests/ and the owner's notes in TODO.md):
 *      $10161E / $101622  the score (BCD; shown with the continues used as its last digit)
 *      $1016EA            the high score table's five scores (BCD, #1 first: hiscore.h)
 *      $101754 / $101758  the ship, 0-2 (type A, B, C)     $101756 / $10175A  its style, 0 shot, 1 laser
 *      $102C8C / $102CCC  the player's record (bit 15: the player in the game)
 *      $102CAC / $102CEC  the style's own weapon's power, 0-4 (4: MAX; the shot for a shot style, the laser for a
 *                         laser style)    $102CAE / $102CEE  the other weapon's, 0-4. A power-up adds one to each
 *                         (09:5174); a life lost takes one from the first and the second back to 0
 *      $102CB0 / $102CF0  the bombs (high byte) and the most the ship can carry (low byte: 3, up to 6)
 *      $101978            the stage of the loop, 0-5 (6: between the loops, and the ending)   $10197A  the loop, 0/1
 *      $1017A5 bit 7      no game (the attract mode, the title)
 *      $103F8E            the bees taken since the stage began or the last miss (the game sets it back to 0 then:
 *                         the panel counts its rises in each stage); the bee's sprite $4496, palette $17
 *    The game is not touched: the panels only read. The stages' best scores are kept in ddonpach.stages (or
 *    ddonpach_jpn.stages) beside the program, from clean games only (no cheat, no save state, the slowdown on).
 */
#ifndef DDONPACH_WIDGETS_H
#define DDONPACH_WIDGETS_H

#include <stdbool.h>
#include <stdint.h>

#define WIDGET_W        120     /* a panel's least width, in the game's pixels: drawn at the picture's scale */
#define WIDGET_MAX_W    256     /* and its most (a very wide screen keeps some black at its sides) */
#define WIDGET_H        320
#define WIDGET_GAP      8       /* between the picture and a panel */

/* The stages' best scores from 'path' (or NULL: ddonpach.stages beside the program); a file that is not there is no
   problem. NULL, or what was wrong with it */
const char *widgets_open(const char *path);

/* After each of the board's frames (not the look-ahead's): 'clean' the game counts (no cheat, no save state, the
   slowdown on): only then are the stages' best scores kept */
void widgets_frame(bool clean);

/* Forgets the run (the board was reset) */
void widgets_reset(void);

/* A game begins where the game is now, its ship select unseen (training's place: the game came there unseen) */
void widgets_game_begun(void);

/* An achievement earned (achievements.h): "ACHIEVEMENT UNLOCKED" and its name over the right panel's top (never over
   the game) for 4 seconds; busy while one is shown, so the next waits ('more': waiting after it, "+2 MORE") */
void widgets_toast(const char *name, int more);
bool widgets_toast_busy(void);

/* The FPS meter (main.c, F9: its second's figures), shown on the right panel (over the score's graph) while the
   panels are shown, so it never covers the game: 'on' false, not shown */
typedef struct
{
	bool on;
	double game, board, windows;    /* frames a second: the game's own, the board's, the pictures shown */
	unsigned late;                  /* frames the PC came to late */
	double work, work_most;         /* a frame's work and picture, ms: on average, at most */
} widgets_meter;

void widgets_set_meter(const widgets_meter *meter);

/* Both panels, 'width' x WIDGET_H each (WIDGET_W to WIDGET_MAX_W), upright as the player sees them; a row is 'width'
   pixels */
void widgets_draw(uint32_t *left, uint32_t *right, int width);

#endif
