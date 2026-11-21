/**
 *    Name:        rom_drop.h
 *    Description: the first run's window when there is no game data and no ROM set: "drop the arcade ROM set here"
 *    Version:     0.1.0 (draft)
 *    Created:     10-11-2026
 *
 *    license:BSD-3-Clause
 *    Drawn with its own small font (the game's font is in the ROMs it is waiting for).
 */
#ifndef DDONPACH_ROM_DROP_H
#define DDONPACH_ROM_DROP_H

#include <stdbool.h>

/* A window that waits for ddonpach.zip (or a folder of the set) to be dropped onto it: read with rom_set_load,
   then saved as the game's data at 'data_path' (rom_set_save_data). 'why' is shown small, under the request (what
   was wrong with the data or the 'rom' setting). true: the set is read ('saved': and saved as the data); false:
   the window was closed */
bool rom_drop_screen(const char *data_path, const char *why, bool *saved);

/* The same for a version's set (the board description's: 0 the parent): a clone asks for its own .zip (read with
   rom_set_load_version, or over the parent's set already loaded: rom_set_load_clone_over) */
bool rom_drop_screen_version(const char *data_path, const char *why, bool *saved, int version);

/* The first run's: the World set just read and saved as its data at 'world_data_path', the Japanese version's data
   too, so that choosing JAPAN later needs no window: from 'source' (a merged ddonpach.zip has ddonpachj's chips in
   it), or its folder (ddonpachj.zip beside it). Saved at 'japan_data_path'; the World set loaded again after,
   whatever happened. true: the Japanese data saved (DonPachi Recomp's, for this game) */
bool rom_drop_japan_too(const char *source, const char *world_data_path, const char *japan_data_path);

/* rom_drop_screen with the Japanese version as well ('japan_data_path'): a set with both (merged) gives both at
   once; otherwise, the World set read, the same window asks for ddonpachj.zip too, which can be skipped
   ('japan_saved': its data saved) */
bool rom_drop_screen_both(const char *data_path, const char *japan_data_path, const char *why, bool *saved,
	bool *japan_saved);

#endif
