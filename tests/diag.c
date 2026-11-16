/* diag [FRAMES]: what the program has put in the board's memories after FRAMES frames */
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include <stdio.h>
#include <stdlib.h>

static int nonzero(const uint16_t *w, int n) { int c = 0; for (int i = 0; i < n; i++) c += w[i] != 0; return c; }

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 600;
	const char *problem;

	cave_board_describe(&cave_board_ddonpach);
	if ((problem = rom_set_load("ddonpach.zip")) || (problem = program_start()))
		return printf("%s\n", problem), 1;
	for (long f = 0; f < frames; f++)
	{
		cave_board.in0 = cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
	}
	printf("palette %d/32768 nonzero; layer RAM %d %d %d; sprite RAM %d; layer regs", nonzero(cave_board.palette, 0x8000),
		nonzero(cave_board.layer_ram[0], 0x4000), nonzero(cave_board.layer_ram[1], 0x4000),
		nonzero(cave_board.layer_ram[2], 0x4000), nonzero(cave_board.sprite_ram, 0x8000));
	for (int l = 0; l < 3; l++)
		printf(" [%04x %04x %04x]", cave_board.layer_regs[l][0], cave_board.layer_regs[l][1], cave_board.layer_regs[l][2]);
	printf("\nvideo regs:");
	for (int i = 0; i < 8; i++)
		printf(" %04x", cave_board.video_regs[i]);
	printf("\nframe_flag %04x; work RAM 100000-10003F:", *(uint16_t *)cave_work_ram_at(0x100F44, 2));
	for (uint32_t a = 0x100000; a < 0x100040; a += 2)
		printf(" %04x", *(uint16_t *)cave_work_ram_at(a, 2));
	printf("\n1015F0-10161F:");
	for (uint32_t a = 0x1015F0; a < 0x101620; a += 2)
		printf(" %04x", *(uint16_t *)cave_work_ram_at(a, 2));
	{
		int n = 0;
		for (uint32_t a = 0x10E8F2; a < 0x10E8F2 + 0x800; a += 2) n += *(uint16_t *)cave_work_ram_at(a, 2) != 0;
		printf("\npalette buffer 10E8F2: %d nonzero words of 1024", n);
	}
	printf("\nymz registers written:");
	for (int i = 0; i < 256; i++)
		if (cave_board.sound.ymz[i])
			printf(" %02x=%02x", i, cave_board.sound.ymz[i]);
	printf("\n");
	return 0;
}
