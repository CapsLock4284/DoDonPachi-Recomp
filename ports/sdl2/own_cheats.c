/**
 *    Name:        own_cheats.c
 *    Description: the port's own cheats for DoDonPachi (own_cheats.h)
 *    Version:     0.1.0
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    Written from what the port knows of the game's memory (game_memory.h), each for a player in play (the game's
 *    mode 7 or 8), after every frame (cave/cheats.h's cheats_apply):
 *      $102C8C / $102CCC the ships' records: +0 bit 15 in play; +$3E (byte) the time without harm, the frames the
 *        ship can't be hit after a life (the ship's routine, 006600 on: not 0, the ship isn't hit and the byte
 *        counts down; $FF, the game's own "for now" (a bomb, the stage's end), doesn't count down)
 *    Held at $80, as the checks' INVINCIBLE=1 (tests/slowdown_check: through both loops, the same as the original
 *    on MAME's 68000 at every frame); a $FF of the game's own is left alone.
 */
#include "own_cheats.h"
#include "game_memory.h"

#include "cave/cheats.h"
#include "cave/machine.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GUARD       0x3Eu       /* (in the record: $102CCA for player 1) */
#define GUARD_HELD  0x80u

static unsigned word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

/* a game is on (playing, or the continue's count down) and the player's ship is in it */
static bool in_play(int player)
{
	const unsigned mode = word(GM_MODE);
	const uint32_t record = GM_RECORD_1 + (uint32_t)player * GM_RECORD_STEP;

	return (mode == 7u || mode == 8u) && (word(record) & 0x8000u) != 0;
}

/* the time without harm kept up: nothing hits the ship */
static void invincible(int player)
{
	uint8_t *const guard = cave_work_ram_byte(GM_RECORD_1 + (uint32_t)player * GM_RECORD_STEP + GUARD);

	if (in_play(player) && *guard < GUARD_HELD)
		*guard = GUARD_HELD;
}

static void p1_invincible(void) { invincible(0); }
static void p2_invincible(void) { invincible(1); }

void own_cheats_add(void)
{
	cheats_add_own("P1 INVINCIBLE", "NOTHING CAN HIT PLAYER 1'S SHIP (ITS TIME WITHOUT HARM AFTER A LIFE KEPT UP).",
		p1_invincible);
	cheats_add_own("P2 INVINCIBLE", "NOTHING CAN HIT PLAYER 2'S SHIP (ITS TIME WITHOUT HARM AFTER A LIFE KEPT UP).",
		p2_invincible);
}
