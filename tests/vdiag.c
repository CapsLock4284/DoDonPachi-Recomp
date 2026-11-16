/* vdiag [FRAMES]: the picture's raw material after FRAMES frames (a coin and Start as boot's): which 256-colour banks
   of the palette are filled, the colour codes the layers' tiles use, the sprite records */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 1900;
	const char *problem;

	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load("ddonpach.zip")) || (problem = program_start()))
		return printf("%s\n", problem), 1;
	for (long f = 0; f < frames; f++)
	{
		unsigned held = f >= 600 && f < 606 ? 0x0100u : f >= 700 && f < 706 ? 0x0080u : 0u;
		cave_board.in0 = (uint16_t)~held;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
	}
	printf("palette banks of 256 (nonzero words in each, of 16 per 16-colour group shown as count of groups used):\n");
	for (int bank = 0; bank < 128; bank++)
	{
		int n = 0, first16 = 0;
		for (int i = 0; i < 256; i++) { n += cave_board.palette[bank * 256 + i] != 0; first16 += i < 16 && cave_board.palette[bank * 256 + i]; }
		if (n) printf("  %02x: %3d (first 16: %d)\n", bank, n, first16);
	}
	for (int layer = 0; layer < 3; layer++)
	{
		int counts[64] = { 0 }, big = (cave_board.layer_regs[layer][1] & 0x2000u) != 0;
		const int base = big && layer != 2 ? 0 : 0x2000, n = big && layer != 2 ? 0x1000 : 0x2000;
		for (int i = 0; i < n; i += 2)
		{
			const uint32_t tile = (uint32_t)cave_board.layer_ram[layer][base + i] << 16 | cave_board.layer_ram[layer][base + i + 1];
			if (tile & 0xFFFFFF) counts[tile >> 24 & 0x3F]++;
		}
		printf("layer %d (%s, regs %04x %04x %04x): colour codes used:", layer, big ? "16x16" : "8x8",
			cave_board.layer_regs[layer][0], cave_board.layer_regs[layer][1], cave_board.layer_regs[layer][2]);
		for (int c = 0; c < 64; c++) if (counts[c]) printf(" %02x(%d)", c, counts[c]);
		printf("\n");
	}
	for (int layer = 0; layer < 3; layer++)
	{
		int n = 0, first = -1;
		for (int i = 0; i < 0x4000; i++) if (cave_board.layer_ram[layer][i]) { n++; if (first < 0) first = i; }
		printf("layer %d RAM: %d nonzero words of 0x4000, the first at word %04x\n", layer, n, first);
	}
	printf("video regs:");
	for (int i = 0; i < 8; i++) printf(" %04x", cave_board.video_regs[i]);
	printf("\nsprites (bank %d): the first records of 8 words with a size:\n", cave_board.video_regs[4] & 3);
	{
		const uint16_t *s = &cave_board.sprite_ram[0x2000u * (cave_board.video_regs[4] & 3u)];
		int shown = 0;
		for (int i = 0; i < 0x2000 && shown < 60; i += 8)
			if ((s[i + 4] & 0x1F1F) && s[i] != 0x1f30)
			{
				printf("  %04x %04x %04x %04x %04x %04x %04x %04x\n", s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5], s[i+6], s[i+7]);
				shown++;
			}
	}
	return 0;
}
