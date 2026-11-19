/**
 *    Name:        ui_sound.h
 *    Description: the port's own sounds (the menus' and the achievement pop-up's), borrowed from the game's: samples
 *                 of its YMZ280B, decoded from the ROM set's data and mixed over the game's sound (as DonPachi
 *                 Recomp's, whose are its effects chip's)
 *    Version:     0.1.0
 *    Created:     19-11-2026
 *
 *    license:BSD-3-Clause
 *    The samples (chosen by the owner, by ear): found by the sounds the game plays (the YMZ280B's voices as each one
 *    starts, logged frame by frame): the ship select's cursor and choice, a short one of its own for going back, and
 *    the extra life's. Decoded as the chip does (its 4-bit ADPCM at the rate the game plays them, frequency $5C:
 *    88,200 Hz x 93 / 512, about 16,021 a second), at the game's own level for each, to the port's 48 kHz. One plays
 *    at a time: a new one cuts the last.
 */
#ifndef DDONPACH_UI_SOUND_H
#define DDONPACH_UI_SOUND_H

#include <stdint.h>

typedef enum
{
	UI_SOUND_MOVE,          /* the menu's cursor moved, or a value changed */
	UI_SOUND_CHOOSE,        /* an entry chosen */
	UI_SOUND_BACK,          /* back out of a page, or the menu closed */
	UI_SOUND_ACHIEVEMENT,   /* the achievement pop-up */
	UI_SOUNDS
} ui_sound;

void ui_sound_play(ui_sound sound);

/* The sound playing mixed into 'count' samples of the game's (48 kHz mono), its samples taken */
void ui_sound_mix(int16_t *samples, unsigned count);

/* A sound is playing */
int ui_sound_playing(void);

#endif
