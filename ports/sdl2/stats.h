/**
 *    Name:        stats.h
 *    Description: lifetime statistics: totals and records across every arcade game played, kept in ddonpach.stats
 *                 beside the program and shown on the main menu's STATISTICS page
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's (ports/sdl2/stats.h), with DoDonPachi's own. From the game watched (game_watch.h): the same
 *    games count as for the achievements. Saved as each stage and each game ends, every minute of play, and as the
 *    program closes.
 */
#ifndef DDONPACH_STATS_H
#define DDONPACH_STATS_H

#include <stdbool.h>

/* The file's statistics (missing or unreadable: all 0) */
void stats_load(const char *path);
void stats_save(void);

/* After each frame of the game (game_watch_frame first) */
void stats_frame(void);

/* The STATISTICS page's lines: a heading (value NULL) or a label and its value */
int stats_lines(void);
const char *stats_label(int line);
const char *stats_value(int line);    /* NULL: the line is a heading */

#endif
