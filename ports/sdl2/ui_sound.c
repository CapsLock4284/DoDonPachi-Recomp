/**
 *    Name:        ui_sound.c
 *    Description: the port's own sounds, borrowed from the game's (ui_sound.h)
 *    Version:     0.1.0
 *    Created:     19-11-2026
 *
 *    license:BSD-3-Clause
 *    The ADPCM decoding after MAME's src/devices/sound/ymz280b.cpp (BSD-3-Clause, Aaron Giles): generate_adpcm(), as
 *    runtime/src/sound.c has it.
 */
#include "ui_sound.h"

#include "cave/board.h"
#include "cave/rom_set.h"
#include "cave/sound.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define CHIP_RATE     (88200.0 * 93.0 / 512.0)    /* the voice's samples a second, at the game's frequency $5C */
#define MAX_SAMPLES   (CAVE_SOUND_RATE * 3u)      /* the longest: 3 seconds */

/* Where each sound is in the YMZ280B's samples (its byte; nibbles: 2 to a byte, the high one first) and the level
   the game plays it at (0-255) */
static const struct
{
	uint32_t at, nibbles;
	int level;
} sources[UI_SOUNDS] = {
	{ 0x2F1A68u, 8466u, 96 },      /* the ship select's cursor */
	{ 0x2F2AF1u, 14034u, 96 },     /* the ship select's choice */
	{ 0x2F465Au, 9544u, 80 },      /* a short one of the game's */
	{ 0x12960Au, 33202u, 112 },    /* the extra life's */
};

static int16_t *decoded[UI_SOUNDS];
static unsigned lengths[UI_SOUNDS];
static const int16_t *playing;
static unsigned playing_left;

static const int index_scale[8] = { 0x0E6, 0x0E6, 0x0E6, 0x0E6, 0x133, 0x199, 0x200, 0x266 };

/* the sound decoded to 48 kHz, once */
static bool decode(ui_sound sound)
{
	const uint8_t *const region = rom_set.oki[0];
	const uint32_t size = rom_set_region_size(CAVE_ROM_SOUND0);
	uint32_t nibbles = sources[sound].nibbles;
	float *chip;
	int signal = 0, step = 0x7F;
	unsigned count;

	if (decoded[sound])
		return true;
	if (!region || sources[sound].at >= size)
		return false;
	if (sources[sound].at + nibbles / 2u > size)
		nibbles = 2u * (size - sources[sound].at);
	chip = malloc(sizeof(float) * (nibbles + 1u));
	if (!chip)
		return false;
	for (uint32_t index = 0; index < nibbles; index++)
	{
		const int nibble = (region[sources[sound].at + index / 2u] >> ((index & 1u) ? 0 : 4)) & 15;
		const int diff = ((nibble & 7) * 2 + 1) * ((nibble & 8) ? -1 : 1);

		signal += (step * diff) / 8;
		signal = signal > 32767 ? 32767 : signal < -32768 ? -32768 : signal;
		step = (step * index_scale[nibble & 7]) >> 8;
		step = step > 0x6000 ? 0x6000 : step < 0x7F ? 0x7F : step;
		/* (as the runtime mixes a voice in the middle: both outputs at its level, MAME's scale) */
		chip[index] = (float)signal * (float)sources[sound].level / (32768.0f * 256.0f);
	}
	chip[nibbles] = 0.0f;
	count = (unsigned)(nibbles * (double)CAVE_SOUND_RATE / CHIP_RATE);
	if (count > MAX_SAMPLES)
		count = MAX_SAMPLES;
	decoded[sound] = malloc(sizeof(int16_t) * (count ? count : 1u));
	if (!decoded[sound])
	{
		free(chip);
		return false;
	}
	for (unsigned index = 0; index < count; index++)
	{
		/* (between the chip's samples on either side: a straight line) */
		const double position = index * CHIP_RATE / CAVE_SOUND_RATE;
		const unsigned below = (unsigned)position;
		const double fraction = position - below, a = chip[below], b = chip[below + 1u < nibbles ? below + 1u : below];

		decoded[sound][index] = (int16_t)lrint((a + (b - a) * fraction) * 32767.0);
	}
	lengths[sound] = count;
	free(chip);
	return true;
}

void ui_sound_play(ui_sound sound)
{
	if (sound < 0 || sound >= UI_SOUNDS || !decode(sound))
		return;
	playing = decoded[sound];
	playing_left = lengths[sound];
}

void ui_sound_mix(int16_t *samples, unsigned count)
{
	for (unsigned index = 0; index < count && playing_left; index++, playing_left--)
	{
		const int sum = samples[index] + *playing++;

		samples[index] = (int16_t)(sum > 32767 ? 32767 : sum < -32768 ? -32768 : sum);
	}
}

int ui_sound_playing(void)
{
	return playing_left > 0;
}
