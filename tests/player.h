/* The test player's inputs for the board's VBLANK 'frame' (active high: CAVE_UP 0x01, DOWN 0x02, LEFT 0x04, RIGHT
   0x08, BUTTON1 0x10, BUTTON2 0x20, START 0x80, COIN 0x100), by the frame alone: the same in tests/slowdown_check.c
   and tools/cref/cref_timed.cpp (included as the body of their player_inputs), whatever the game does. Every 600
   VBLANKs a coin and Start; between them, the shot tapped (and held a while: the laser), sweeping left and right */
unsigned held = 0;
const unsigned long long in_cycle = frame % 600u;

if (in_cycle >= 100u && in_cycle < 104u)
	held = 0x0100u;
else if (in_cycle >= 160u && in_cycle < 164u)
	held = 0x0080u;
else
{
	held = (frame / 2u) % 2u ? 0x0010u : 0u;
	if ((frame / 240u) % 4u == 3u)
		held |= 0x0010u;
	held |= (frame / 90u) % 2u ? 0x0004u : 0x0008u;
	if ((frame / 45u) % 3u == 0u)
		held |= 0x0001u;
}
return held;
