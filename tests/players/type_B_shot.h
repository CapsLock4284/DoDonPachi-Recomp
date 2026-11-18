/* tests/players/type_B_shot.h: a test player for tests/slowdown_check.c and tools/cref/cref_timed.cpp (in place of
   tests/player.h, which they include): from the power on, a coin, Start, ship type B, its shot style; then button C
   (the game's auto-fire), sweeping, the shot held now and then (the laser), a bomb every 900 VBLANKs, a coin and Start
   every 3,000 (continues). By the VBLANK alone, as tests/player.h */
unsigned held = 0;
if (frame < 3500u)
{
	/* a coin, Start, the ship type (1 downs), the style (0 rights), button A twice */
	if (frame >= 2500u && frame < 2506u) held = 0x100u;
	if (frame >= 2600u && frame < 2606u) held = 0x80u;
	if (1 >= 1 && frame >= 2700u && frame < 2704u) held = 0x02u;
	if (1 >= 2 && frame >= 2760u && frame < 2764u) held = 0x02u;
	if (frame >= 2850u && frame < 2854u) held = 0x10u;
	if (0 && frame >= 2900u && frame < 2904u) held = 0x08u;
	if (frame >= 2950u && frame < 2954u) held = 0x10u;
	return held;
}
if (frame % 3000u < 6u) return 0x100u;                     /* a coin and Start every 3000 VBLANKs: continues */
if (frame % 3000u >= 60u && frame % 3000u < 66u) return 0x80u;
held = 0x40u;                                              /* button C: the game's auto-fire */
held |= (frame / 50u) % 2u ? 0x04u : 0x08u;
if ((frame / 70u) % 3u == 0u) held |= 0x01u;
if ((frame / 130u) % 5u == 4u) held = (held & ~0x40u) | 0x10u;    /* the shot held: the laser */
if (frame % 900u == 450u) held |= 0x20u;                    /* a bomb now and then */
return held;
