/**
 *    Name:        cref_timed.cpp
 *    Description: DoDonPachi's original program on MAME's 68000 core at the board's own timing: the VBLANK interrupt
 *                 every CAVE_FRAME_USEC of 68000 cycles (278,016 at 16 MHz), whether or not the program is waiting
 *                 for it. A frame whose work runs past the next interrupt makes the program wait a whole frame more:
 *                 the arcade's slowdown. Logs each frame's work in cycles and what was on the screen
 *    Version:     0.4.0 (draft)
 *    Created:     11-11-2026 (DonPachi Recomp's); DoDonPachi's 16-11-2026: its board (cave_board_ddonpach), its
 *                 frame wait (0571AC) and frame-ready flag (10120C bit 0), the board's clock for the interrupt cause's
 *                 bit 2 (cave_clock), and a player that reads no RAM (inputs by the VBLANK alone)
 *
 *    license:BSD-3-Clause
 *    After cref.cpp (the reference, frame timed). MAME's 68000 (BSD-3-Clause) built from m68000.lst.
 *
 *    Usage: cref_timed [--rom PATH] [--frames N] [--log FILE] [--waits FILE] [--hashes FILE] [--play]
 *      --rom     the ROM set's .zip (default ddonpach.zip): the International program
 *                (VERSION=1 in the environment: the Japanese set's, ddonpachj's chips from the same .zip or beside it)
 *      --frames  the board's frames (VBLANKs) to run (default 18000: about 5 minutes)
 *      --log     a line per VBLANK: "vblank game_frame busy_cycles lagged"
 *      --waits   a line per frame of the game, as it comes to its wait for the next one, its work done:
 *                "game_frame vblank hash" (tests/slowdown_check writes the same from the port; tests/slowdown_compare.py
 *                compares them)
 *      --hashes  a line per VBLANK, "vblank lagged hash"
 *      --play    the inputs of tests/slowdown_check's player (by the VBLANK: coins, Start, firing, sweeping)
 *      INVINCIBLE=1 (environment, with --play): player 1 kept from harm from VBLANK 2,500 on (the time without harm after
 *                a life, $102CCA, held at $80 as the inputs are set): the player goes through both loops, the same in
 *                tests/slowdown_check with INVINCIBLE=1
 *    For finding where the port goes another way (environment):
 *      DUMPFRAME=N    the board's memories at game frame N's wait, in mame_dump.bin (the port's: port_dump.bin)
 *      TRACE_FRAME=N  every instruction of game frame N's work, in mame_trace.txt (the port's, built from
 *                     tools/decompile.py --trace: tests/slowdown_check_trace; tests/trace_compare.py lines them up)
 *      DUMP_AT=ADDR   with TRACE_FRAME: the memories as the frame first comes to ADDR, and D0, D1, A6 there
 *      DEBUG_JUMPS=1  where the computed jumps tools/call_tables.py lists by hand really go, in jumps.txt
 *      CYCLELOG=FILE  each game frame's cycle as it comes to its wait, "game_frame cycle" (tests/slowdown_check the
 *                     same): the first frame whose work took another time
 *      TRACE_CYCLES=1 with TRACE_FRAME: each instruction's cycle beside its address
 *    The YMZ280B's interrupt (16-11-2026): the run stops at each cycle the chip's interrupt may come by
 *    (cave_sound_event_at) and raises the line there; the chip itself is brought there as the next instruction
 *    begins, after that instruction's accesses, as the decompiled program has it. The chip's and the cause's bit 2
 *    read the time at the instruction's start (cave_access_clock), and only the VBLANK's interrupt ends a wait.
 *    Checked (16-11-2026): 150,000 VBLANKs of tests/player.h's play, the port and this the same at all 147,505 game
 *    frames (the same VBLANK, the same board; 2,494 VBLANKs lagged in both)
 */
#include "emu.h"
#include "m68000.h"

extern "C" {
#include "cave/boards.h"
#include "cave/machine.h"
#include "cave/rom_set.h"
}

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

/* tools/game_profile.py's FRAME_WAIT: the wait for the next frame (the Japanese program's: game_profile.here's place
   for it, VERSION=1) */
static u32 CREF_FRAME_WAIT = 0x0571ACu;
#define CREF_READY      0x10120Cu    /* bit 0: the frame made ready for the interrupt (DonPachi's 1011FC) */

namespace {

class reference_cpu : public m68000_device
{
public:
	reference_cpu() : m68000_device(m_config, "maincpu", nullptr, CAVE_CPU_CLOCK) {}
	u32 pc() const { return m_ipc; }
	void shorten(int by) { m_icount -= by; }    // (the run ends sooner: the cycles it has left, less 'by')
	int left() const { return m_icount; }
	u32 reg(int n) const { return m_da[n]; }

private:
	static inline machine_config m_config;
};

reference_cpu *g_cpu = nullptr;
FILE *g_waits = nullptr;
FILE *g_trace = nullptr;    // (TRACE_FRAME=N: every instruction of game frame N's work, in mame_trace.txt)
int g_irq_level = 0;
uint64_t g_total = 0;                // the cycles done
uint64_t g_cycles_before_run = 0;    // the cycles done before the current run()
int g_run_length = 0;
int g_shortened = 0;                 // (cycles taken off the current run(): a sooner event of the sound chip's)
uint64_t g_instruction_start = 0;    // where the instruction running began, in cycles
uint64_t g_sound_due = UINT64_MAX;   // a sound chip's event the run stopped at part way into an instruction: the chip
bool g_sound_line = false;           // is brought there as the next begins; its interrupt line up meanwhile
uint64_t g_work_began = 0;           // where the game's work began (its wait ended), in cycles
bool g_waiting = false;              // the program is in its wait for the next frame
uint64_t g_game_frames = 0;
int64_t g_last_busy = -1;            // the last frame's work, in cycles (-1: none ended since the last VBLANK)

uint16_t ram(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

uint64_t now()
{
	return g_cycles_before_run + uint64_t(g_run_length > 0 ? g_run_length - g_cpu->cycles_left() : 0);
}

uint64_t clock_now()
{
	return now();
}

uint64_t instruction_start()
{
	return g_instruction_start;
}

/* the sound chip's next interrupt sooner than the run's end (a voice keyed on): the run ends there */
void sound_events_changed()
{
	const uint64_t event = cave_sound_event_at();

	if (g_run_length <= 0 || !g_cpu || g_sound_due != UINT64_MAX)
		return;
	const uint64_t end = g_cycles_before_run + uint64_t(g_run_length);
	if (event >= end)
		return;
	int by = int(end - event);
	if (by > g_cpu->left())
		by = g_cpu->left();
	if (by <= 0)
		return;
	g_cpu->shorten(by);
	g_run_length -= by;
	g_shortened += by;
}

void sync_irq()
{
	const int level = cave_irq_level() ? cave_irq_level() : g_sound_line ? 1 : 0;
	if (level == g_irq_level)
		return;
	if (g_irq_level)
		g_cpu->set_input_line(g_irq_level, CLEAR_LINE);
	if (level)
		g_cpu->set_input_line(level, ASSERT_LINE);
	g_irq_level = level;
}

u16 bus_read(offs_t address, u16 mem_mask)
{
	const u16 value = cave_read16(address, mem_mask);
	sync_irq();
	return value;
}

void bus_write(offs_t address, u16 data, u16 mem_mask)
{
	cave_write16(address, data, mem_mask);
	sync_irq();
}

bool g_play = false;
unsigned player_inputs(uint64_t frame);
uint64_t g_acks = 0;

u16 acknowledge(offs_t address, u16 mem_mask)
{
	(void)mem_mask;
	g_acks++;
	if (g_play && ((address >> 1) & 7) == 1)
	{
		/* (the player's inputs for this VBLANK, from the game as the interrupt is taken: when the headless port's
		   harness sets them, its frame coming as the 68000 takes the interrupt) */
		g_play = false;
		cave_board.in0 = uint16_t(~player_inputs(cave_board.frame));
		if (getenv("INVINCIBLE") && cave_board.frame > 2500u)
			*cave_work_ram_byte(0x102CCAu) = 0x80u;    /* (player 1's time without harm after a life: held) */
	}
	if (g_waiting && ((address >> 1) & 7) == 1 && cave_board.vblank_irq)
	{
		/* (the VBLANK's interrupt ends the wait: the work of the next frame begins; the sound chip's does not) */
		g_waiting = false;
		g_work_began = now();
	}
	return u16(0x18 + ((address >> 1) & 7));
}

void on_instruction(offs_t pc)
{
	g_instruction_start = now();
	if (g_sound_due != UINT64_MAX && g_instruction_start >= g_sound_due)
	{
		/* (the chip brought to its event, after the accesses of the instruction it came in: the decompiled program's
		   order, its events at an instruction's end) */
		cave_sound_run_to(g_sound_due);
		g_sound_due = UINT64_MAX;
		g_sound_line = false;
		sync_irq();
	}
	/* (DEBUG_JUMPS: where the computed jumps and calls tools/call_tables.py lists by hand really go: each site's next
	   instruction, in jumps.txt as "site target") */
	static FILE *jumps = getenv("DEBUG_JUMPS") ? std::fopen("jumps.txt", "w") : nullptr;
	static u32 site = 0;
	if (jumps)
	{
		static const u32 sites[] = { 0x048AAE, 0x048B0A, 0x0031C8, 0x0362A6, 0x0514DC, 0x055436, 0x058954, 0x058A0C,
			0x05A118, 0x05A1FC, 0x051436, 0x055390, 0x053D16, 0x00B3F6, 0x036278, 0x02F098, 0x04F47E,
			0x003288, 0x04E0EA, 0x053020, 0x0532D8, 0x053E20, 0x056E82, 0x05713A, 0x0596EC, 0x0599A8 };
		const u32 at = pc & 0xFFFFFFu;
		if (site)
			std::fprintf(jumps, "%06X %06X\n", site, at);
		site = 0;
		for (u32 each : sites)
			if (at == each)
				site = each;
	}
	const bool at_wait = (pc & 0xFFFFFF) == CREF_FRAME_WAIT;

	if (at_wait && !g_waiting && (ram(CREF_READY) & 1u))
	{
		/* (a wait the program has made ready, its first round: a round the interrupt comes in after it tested its
		   flag, or before the interrupt is taken, is the wait the program is already in) */
		/* (the work done: from where the last wait ended to here) */
		g_waiting = true;
		g_last_busy = int64_t(now() - g_work_began);
		{
			static FILE *cycles = getenv("CYCLELOG") ? std::fopen(getenv("CYCLELOG"), "w") : nullptr;
			if (cycles)
				std::fprintf(cycles, "%" PRIu64 " %" PRIu64 "\n", g_game_frames + 1u, uint64_t(now()));
		}
		g_game_frames++;
		if (getenv("DUMPFRAME") && g_game_frames == std::strtoull(getenv("DUMPFRAME"), nullptr, 10))
			cave_dump("mame_dump.bin");
		if (g_waits)
			std::fprintf(g_waits, "%" PRIu64 " %" PRIu64 " %016" PRIx64 "\n", g_game_frames, cave_board.frame, cave_state_hash());
		if (getenv("TRACE_FRAME"))
		{
			static FILE *trace;
			const uint64_t traced = std::strtoull(getenv("TRACE_FRAME"), nullptr, 10);
			if (g_game_frames == traced - 1u)
				g_trace = trace = std::fopen("mame_trace.txt", "w");
			else if (g_game_frames == traced && trace)
			{
				std::fclose(trace);
				g_trace = trace = nullptr;
			}
		}
	}
	if (g_trace)
	{
		static bool dumped;
		if (getenv("TRACE_CYCLES"))
			std::fprintf(g_trace, "%06X %" PRIu64 "\n", unsigned(pc & 0xFFFFFFu), uint64_t(now()));
		else
			std::fprintf(g_trace, "%06X\n", unsigned(pc & 0xFFFFFFu));
		if (!dumped && getenv("DUMP_AT") && (pc & 0xFFFFFFu) == std::strtoul(getenv("DUMP_AT"), nullptr, 16))
		{
			dumped = true;
			cave_dump("mame_dump.bin");
			std::fprintf(stderr, "at %06X: D0 %08X D1 %08X A6 %08X\n", unsigned(pc & 0xFFFFFFu), g_cpu->reg(0), g_cpu->reg(1), g_cpu->reg(14));
		}
	}
}

/* tests/slowdown_check.c's player: player 1's inputs (active high) for the VBLANK 'frame', by the frame alone */
unsigned player_inputs(uint64_t frame)
{
#include "../../tests/player.h"
}

} // namespace

int main(int argc, char **argv)
{
	std::string rom = "ddonpach.zip";
	unsigned long long frames = 18000;
	FILE *log = nullptr, *hashes = nullptr;
	bool play = false;

	for (int index = 1; index < argc; index++)
	{
		const std::string argument = argv[index];
		const bool value = index + 1 < argc;

		if (argument == "--rom" && value)
			rom = argv[++index];
		else if (argument == "--frames" && value)
			frames = std::strtoull(argv[++index], nullptr, 10);
		else if (argument == "--log" && value)
			log = std::fopen(argv[++index], "w");
		else if (argument == "--waits" && value)
			g_waits = std::fopen(argv[++index], "w");
		else if (argument == "--hashes" && value)
			hashes = std::fopen(argv[++index], "w");
		else if (argument == "--play")
			play = true;
		else
		{
			std::fprintf(stderr, "usage: cref_timed [--rom PATH] [--frames N] [--log FILE] [--hashes FILE] [--waits FILE] "
				"[--play]\n");
			return 2;
		}
	}
	cave_board_describe(&cave_board_ddonpach);
	cave_clock = clock_now;
	cave_access_clock = instruction_start;
	cave_sound_events_changed = sound_events_changed;
	/* (VERSION=1: the Japanese set, ddonpachj: its program chips from the .zip, merged or beside it) */
	if (const char *problem = rom_set_load_version(rom.c_str(), getenv("VERSION") ? std::atoi(getenv("VERSION")) : 0))
	{
		std::fprintf(stderr, "The game does not start: %s\n", problem);
		return 1;
	}
	cave_board.program = rom_set.program;
	if (rom_set.version == 1)
		CREF_FRAME_WAIT = 0x056DF4u;    /* (0571AC's place in the Japanese program: the same wait loop) */
	std::memcpy(cave_board.eeprom.words, rom_set.eeprom, sizeof(cave_board.eeprom.words));
	cave_reset();
	cave_board.in0 = cave_board.in1 = 0xFFFFu;

	reference_cpu cpu;
	g_cpu = &cpu;
	for (int space : { int(AS_PROGRAM), int(AS_OPCODES) })
	{
		cpu.space(space).read16 = bus_read;
		cpu.space(space).write16 = bus_write;
	}
	cpu.space(AS_OPCODES).instructions = true;
	cpu.space(m68000_base_device::AS_CPU_SPACE).read16 = acknowledge;
	cpu.space(m68000_base_device::AS_CPU_SPACE).write16 = bus_write;
	cpu.instruction_hook = on_instruction;
	cpu.log_handler = [](const std::string &message) { std::fputs(message.c_str(), stderr); };
	cpu.start();
	cpu.reset();

	const uint64_t frame_cycles = uint64_t(CAVE_CPU_CLOCK) * CAVE_FRAME_USEC / 1000000u;
	/* (the VBLANK's interrupt early by the board's description, as the port's runtime has it: cave/program.h's
	   program_set_vblank_lead; NO_VBLANK_LEAD=1: at the frame's end, as before 25-11-2026) */
	const uint64_t lead = getenv("NO_VBLANK_LEAD") ? 0u
		: uint64_t(cave_board_description->vblank_lead_usec) * (CAVE_CPU_CLOCK / 1000000u);
	const auto vblank_cycle = [&](uint64_t number) { return number * frame_cycles - lead; };
	uint64_t lagged = 0, busiest = 0;

	for (uint64_t vblank = 1; vblank <= frames; vblank++)
	{
		/* (the frame's cycles, to the next VBLANK; the sound chip's interrupts on the way, each at its sample) */
		for (;;)
		{
			const uint64_t sound = g_sound_due != UINT64_MAX ? UINT64_MAX : cave_sound_event_at();

			if (sound <= g_total && sound < vblank_cycle(vblank))
			{
				/* (its interrupt line now; the chip there as the next instruction begins) */
				g_sound_due = sound;
				g_sound_line = cave_sound_irq_at(sound);
				sync_irq();
				continue;
			}
			if (g_total >= vblank_cycle(vblank))
				break;
			uint64_t end = sound < vblank_cycle(vblank) ? sound : vblank_cycle(vblank);
			if (g_sound_due != UINT64_MAX && end > g_total + 4u)
				end = g_total + 4u;    /* (on to the next instruction, a little at a time, for the next event) */

			g_cycles_before_run = g_total;
			g_run_length = int(end - g_total);
			g_shortened = 0;
			g_total += uint64_t(cpu.run(g_run_length) - g_shortened);
			g_cycles_before_run = g_total;
			g_run_length = 0;
		}
		/* VBLANK: the interrupt; the program's wait (if it is waiting) ends as its handler sets the frame's flag */
		const bool was_waiting = g_waiting;
		const bool lag = !was_waiting && vblank > 1;

		g_play = play;
		if (log)
			std::fprintf(log, "%" PRIu64 " %" PRIu64 " %" PRId64 " %d\n", vblank, g_game_frames,
				was_waiting ? g_last_busy : int64_t(-1), lag ? 1 : 0);
		if (lag)
			lagged++;
		if (was_waiting && g_last_busy > int64_t(busiest))
			busiest = uint64_t(g_last_busy);
		cave_vblank_start();
		cave_board.vblank_began = vblank_cycle(vblank);    /* (its own cycle: the run may stop a few after it) */
		if (getenv("DUMP") && vblank == std::strtoull(getenv("DUMP"), nullptr, 10))
			cave_dump("mame_dump.bin");
		if (hashes)
			std::fprintf(hashes, "%" PRIu64 " %d %016" PRIx64 "\n", vblank, lag ? 1 : 0, cave_state_hash());
		sync_irq();
	}
	std::printf("vblanks %llu, game frames %" PRIu64 ", lagged %" PRIu64 ", the busiest frame %" PRIu64
		" cycles of %" PRIu64 "\n", frames, g_game_frames, lagged, busiest, frame_cycles);
	if (log)
		std::fclose(log);
	if (hashes)
		std::fclose(hashes);
	return 0;
}
