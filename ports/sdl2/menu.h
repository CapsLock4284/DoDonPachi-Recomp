/**
 *    Name:        menu.h
 *    Description: the window port's options menu: drawn over the paused game with the game's own 8x8 font (its text
 *                 layer's tiles, from the player's ROM set), worked with a gamepad or the keys
 *    Version:     0.1.0 (draft)
 *    Created:     17-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's menu (ports/sdl2/menu.h), with the pages that do not read that game's memory: the main menu,
 *    the options, the game options, the graphics, the controls, the cheats.
 *    The gamepad's View button or Escape opens it; the game is not run while it is open (it goes on from the same
 *    frame). D-pad, stick or arrows move; A, Enter, Z or Space choose; left and right change a value; B, Backspace or
 *    X go back; View or Escape close. Its changes go to ddonpach.ini at once.
 */
#ifndef DDONPACH_MENU_H
#define DDONPACH_MENU_H

#include "settings.h"
#include "pads.h"

#include <SDL.h>

#include <stdbool.h>
#include <stdint.h>

/* What the menu works on: the port's settings and what it does with them */
typedef struct
{
	settings *options;          /* the settings in use (the file's, and the command line's over them) */
	settings *file;             /* the file's: the menu's changes go to both, and this is saved */
	const char *path;           /* ddonpach.ini */
	const pad_device *pads;     /* the two players' gamepads and joysticks (pads.h; .joy NULL: none) */
	void (*reset)(void);        /* the board's reset */
	void (*test_mode)(void);    /* the service switch held for the player until the test mode comes up */
	void (*apply_window)(void); /* the window's size and full screen from options */
	void (*apply_video)(void);  /* the smoothing and vsync from options */
	void (*apply_auto_fire)(void);    /* options->c_button_full_auto into the EEPROM (the game reads it at its reset) */
	void (*quit)(void);
	void (*state)(bool save);   /* the save state: kept, or gone back to (the chosen slot's) */
	int (*state_slot)(int step);    /* the save states' slot, 0 to STATE_SLOTS - 1, moved by 'step' (0: as it is) */
	bool (*state_used)(int slot);   /* a save state is kept in the slot */
	void (*main_menu)(void);    /* the board reset and kept at its start, for the main menu (then it shows) */
	bool (*training)(void);     /* the game run to the training's place (options' [training]): false if it failed */
	bool (*in_training)(void);  /* a training game is being played */
	void (*retry_training)(void);   /* back to the training's place */
	void (*keep_replay)(bool keep);     /* the game just over: its replay saved (true) or let go */
	bool (*play_replay)(const char *file);    /* a replay played from its start (false: it could not be) */
	bool (*in_replay)(void);    /* a replay is playing */
	void (*stop_replay)(void);  /* the replay stopped (the main menu's replays page then up) */
	void (*install_update)(void);   /* the update downloaded (updater.h): installed and the program closed to start
	                                   again, or updater_state() FAILED (the menu says why) */
	int (*version)(void);              /* the game's version: 0 the World one, 1 the Japanese one (rom_set.h) */
	bool (*set_version)(int version);  /* the game to that version, from its reset: false if it could not be */
} menu_host;

void menu_init(const menu_host *host);

bool menu_is_open(void);
void menu_open(void);

/* The main menu (START GAME, SETTINGS, QUIT TO DESKTOP), as the program starts ([game] main_menu) and from the
   options' MAIN MENU: the game is not run while it is up (menu_is_open), and START GAME closes it; Escape or View
   don't */
void menu_open_front(void);
bool menu_in_front(void);

/* The main menu's TRAINING page (from a training game's TRAINING MENU: the game reset first) */
void menu_open_training(void);

/* The main menu's REPLAYS page (a replay stopped: the game reset first) */
void menu_open_replays(void);

/* A game is over: "SAVE THIS GAME'S REPLAY?", YES or NO (host.keep_replay), over the paused game */
void menu_ask_save_replay(void);

/* An event while it is open: true when the menu took it (all are taken while it is open) */
bool menu_event(const SDL_Event *event);

/* Once a frame while it is open: the stick's and held buttons' repeats, the remapping's wait */
void menu_tick(void);

/* The picture to show: the game's picture (240 x 320, as the cabinet shows it) dimmed, the menu over it */
void menu_draw(const uint32_t *game, uint32_t *out);

/* A line of text over the game for a moment ("STATE SAVED"), in the game's font */
void menu_notice(const char *text);

/* The picture with the notice over it while there is one (else the picture itself): once a frame shown */
const uint32_t *menu_with_notice(const uint32_t *picture);

/* The chosen entry's name and what it does, while a page with one is up (the right side panel shows them as its
   help: widgets_draw_help): false when there is none (no page up, a key being waited for) */
bool menu_help(const char **title, const char **text);

/* The FPS meter's lines (main.c) over the picture's bottom right corner, on a dark band: 'picture' (240 x 320) is
   changed */
void menu_put_meter(uint32_t *picture, const char *const *lines, int count);

/* "SPEED 70%" at the picture's foot, centred (a training game slower than the arcade's) */
void menu_put_speed(uint32_t *picture, int percent);

/* Closed since the last call (then the port lets the game ignore what is still held, and starts the sound again) */
bool menu_just_closed(void);

#endif
