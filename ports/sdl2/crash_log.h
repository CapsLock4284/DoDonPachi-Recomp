/**
 *    Name:        crash_log.h
 *    Description: ddonpach.log beside the program: what the program writes to stderr (problems, the 68000 stopping)
 *                 kept in a file on Windows, where a windowed program has no console; and a crash written there as
 *                 it happens (the exception, where in the program, the game's frame), with the inputs since the
 *                 board's reset kept in a file of their own, to play the run again up to the crash
 *    Version:     0.2.0 (draft)
 *    Created:     14-11-2026
 *
 *    license:BSD-3-Clause
 *    The run's inputs: the game is the same from a reset with the same EEPROM and the same inputs at each frame (the
 *    board's time is the program's own count), so the inputs since the reset are the run. Kept in memory (4 bytes a
 *    frame: about 830 KB an hour), and written only when the program stops or crashes, to
 *    ddonpach_crash_DATE_TIME.inp beside the program: "DDPINP02", the frames (a 32-bit count, little endian), the
 *    slowdown setting, the game's version, 1 when a high score table is put back after the boot, 1 when the VBLANK comes early (program_set_vblank_lead;
 *    always 0 in a "DDPINP01" file, from before 25-11-2026), the
 *    EEPROM as the run began (128 bytes), the high score table (100 bytes: hiscore.h), then each frame's IN0 and IN1
 *    (16 bits each, little endian, active low: as the board reads them). tests/replay_crash plays it again. A run
 *    that went back to a save state or had a cheat on is not the same from its inputs: the log says so.
 *    The log of the run before is kept as ddonpach_previous.log.
 */
#ifndef CRASH_LOG_H
#define CRASH_LOG_H

#include <stdbool.h>
#include <stdint.h>

/* From the program's start: stderr to 'path' (Windows; elsewhere stderr stays as it is), the log before kept, and the
   crash handlers ('folder': where the inputs' file goes, ending in its separator) */
void crash_log_start(const char *path, const char *folder);

/* The board was reset (or started): the inputs kept from the next frame on, with the EEPROM as it is then.
   'slowdown' and 'version' as the run has them; 'hiscore_table' the high score table the port puts back after the
   boot (hiscore.h: HISCORE_TABLE_SIZE bytes), or NULL for none */
void crash_log_reset(bool slowdown, int version, const unsigned char *hiscore_table);

/* Before each frame the board runs: its inputs (cave_board.in0, in1 as set) */
void crash_log_frame(void);

/* The run is no longer its inputs alone: 'why' (a save state put back, a cheat on), told in the log at a crash */
void crash_log_not_replayable(const char *why);

/* The program ends as it should: no crash to write at the exit */
void crash_log_finish(void);

/* The run as kept so far (replay.h saves a game's replay from it): its frames' inputs (IN0, IN1 of each, active low),
   how many, the EEPROM as it began (CAVE_EEPROM_WORDS), the slowdown and version, the high score table put back after
   the boot (100 bytes; has_table false: none), and whether it is its inputs alone */
typedef struct
{
	const uint16_t *frames;
	uint32_t count;
	const uint16_t *eeprom;
	bool slowdown, has_table, replayable;
	bool vblank_lead;           /* the VBLANK early as the run began (program_set_vblank_lead) */
	int version;
	const uint8_t *table;
} crash_log_run;

void crash_log_view(crash_log_run *out);

#endif
