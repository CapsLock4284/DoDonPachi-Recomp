/**
 *    Name:        achievements.h
 *    Description: goals met in the arcade game (clear a stage without dying, all 13 bees, a 500 hit combo, ...),
 *                 kept in ddonpach.achievements beside the program
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's (ports/sdl2/achievements.h), with DoDonPachi's own goals. Watched from the game's memory after
 *    each frame. Only the arcade game counts: not training, not a replay, not a game that had a cheat on or went back
 *    to a save state (from its ship select on).
 */
#ifndef DDONPACH_ACHIEVEMENTS_H
#define DDONPACH_ACHIEVEMENTS_H

#include <stdbool.h>

/* The file's achievements (missing or unreadable: none yet) */
void achievements_load(const char *path);

/* After each frame of the game: 'counts' false while the game may not earn them */
void achievements_frame(bool counts);

/* The next achievement earned and not yet shown (its name), or NULL; and how many more wait after it */
const char *achievements_next_shown(void);
int achievements_waiting(void);

int achievements_count(void);
int achievements_done(void);
const char *achievements_name(int index);
const char *achievements_text(int index);       /* what it asks for */
const char *achievements_date(int index);       /* when it was earned ("2026-11-18"), or NULL */

#endif
