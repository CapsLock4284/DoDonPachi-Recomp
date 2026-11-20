/**
 *    Name:        menu.c
 *    Description: the window port's options menu (menu.h)
 *    Version:     0.1.0 (draft)
 *    Created:     17-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's menu (ports/sdl2/menu.c), its options, game options, graphics, controls and cheats pages.
 *    The font is the game's text layer (u62, rom_set.layer[2]): its 8x8 tiles, 8 bits a pixel, are ASCII as they are
 *    (the test mode's text: "0123456789ABCDEF" is tiles 30-39 and 41-46), uppercase only ('>' an arrow), pens 1-5 a
 *    shade from light to dark and pen 15 the outline, as DonPachi's. The tiles lie as the board's screen does, turned
 *    for the cabinet: a tile's row is the picture's column.
 */
#include "menu.h"
#include "achievements.h"
#include "hiscore.h"
#include "stats.h"
#include "updater.h"
#include "training.h"
#include "ui_sound.h"
#include "replay.h"
#include "scaler.h"

#include "cave/cheats.h"
#include "cave/machine.h"
#include "cave/board.h"
#include "cave/rom_set.h"
#include "cave/video.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define PICTURE_W   VIDEO_HEIGHT        /* 240: the picture as the cabinet shows it */
#define PICTURE_H   VIDEO_WIDTH         /* 320 */
#define COLUMNS     (PICTURE_W / 8)     /* 30 */

#define PANEL_LEFT  1                   /* the panel's columns and rows, in characters */
#define PANEL_RIGHT 28
#define PANEL_TOP   3
#define PANEL_ROWS  34

#define WHITE       0xF0F0F0u
#define GOLD        0xFFC83Cu
#define GREY        0x8C8C9Cu
#define CYAN        0x6CD8FFu

#define REPEAT_FIRST 18                 /* frames a direction is held before it repeats, then every REPEAT_NEXT */
#define REPEAT_NEXT  5
#define CAPTURE_FRAMES 345              /* the remapping's wait for a key or button: 6 s */
#define STICK_DEAD   16000
#define DIFFICULTY_WORD 4               /* the EEPROM word with the test mode's GAME DIFFICULTY (bits 8-9) */
#define CHEAT_ROWS   16                 /* the cheats page's: as many shown at a time, scrolled with the choice */

enum { PAGE_MAIN, PAGE_CONTROLS, PAGE_GRAPHICS, PAGE_GAME, PAGE_CHEATS, PAGE_FRONT, PAGE_TRAINING, PAGE_REPLAYS,
	PAGE_REPLAY_MENU, PAGE_DELETE, PAGE_SAVE_REPLAY, PAGE_ACHIEVEMENTS, PAGE_STATS, PAGE_UPDATE,
	PAGE_SOUND };
enum { NAV_NONE, NAV_UP, NAV_DOWN, NAV_LEFT, NAV_RIGHT, NAV_CHOOSE, NAV_BACK, NAV_CLOSE };

enum
{
	ITEM_RESUME, ITEM_RESET, ITEM_TEST_MODE, ITEM_FAST_SPEED, ITEM_VOLUME, ITEM_SCALE, ITEM_FULLSCREEN, ITEM_CONTROLS,
	ITEM_QUIT, ITEM_DEFAULTS, ITEM_BACK, ITEM_GRAPHICS, ITEM_SCALING, ITEM_SMOOTH, ITEM_SCANLINES, ITEM_VSYNC,
	ITEM_ROTATE, ITEM_DEADZONE, ITEM_SAVE_STATE, ITEM_LOAD_STATE, ITEM_GAME, ITEM_CHEATS, ITEM_SLOWDOWN, ITEM_CRT,
	ITEM_STATE_SLOT, ITEM_FILTER, ITEM_AUTO_FIRE, ITEM_TIMING, ITEM_FREE_PLAY, ITEM_HIGH_SCORES,
	ITEM_SKIP_WARNING, ITEM_FPS, ITEM_RENDERER, ITEM_PANELS,
	ITEM_START_GAME, ITEM_SETTINGS,   /* the main menu's */
	ITEM_MAIN_MENU,               /* the options': the game left, the main menu */
	ITEM_FRONT_AT_START,          /* the game options': the main menu as the program starts */
	ITEM_TRAINING,                /* the main menu's: the training page */
	ITEM_T_STAGE, ITEM_T_POINT, ITEM_T_SHIP, ITEM_T_STYLE, ITEM_T_POWER, ITEM_T_BOMBS, ITEM_T_RETRY, ITEM_T_START,
	ITEM_RETRY_TRAINING, ITEM_TRAINING_MENU,    /* a training game's options */
	ITEM_REPLAYS,                 /* the main menu's: the replays page */
	ITEM_WATCH, ITEM_DELETE, ITEM_DELETE_YES, ITEM_DELETE_NO,    /* a replay chosen */
	ITEM_YES, ITEM_NO,            /* a game over: its replay saved? */
	ITEM_STOP_REPLAY,             /* a replay playing: stopped */
	ITEM_HITBOX, ITEM_AURA,       /* the game options': the overlays (hitbox.h) */
	ITEM_ACHIEVEMENTS, ITEM_STATISTICS,    /* the main menu's pages */
	ITEM_UPDATE, ITEM_UPDATE_YES, ITEM_UPDATE_NO,    /* the main menu's, a newer release found (updater.h) */
	ITEM_CHECK_UPDATES,           /* the game options': the newest release looked up as the program starts */
	ITEM_DIFFICULTY,              /* the game options': the game's difficulty, the test mode's (in the EEPROM) */
	ITEM_SOUND,                   /* the sound's page: VOLUME, and MUSIC, EFFECTS and VOICES on their own */
	ITEM_MUSIC_VOLUME, ITEM_EFFECTS_VOLUME, ITEM_VOICES_VOLUME,
	ITEM_T_SPEED,                 /* the training page's: the game's speed, 50-100% */
	ITEM_BULLETS,                 /* the game options': the bullets' points (hitbox.h) */
	ITEM_FAST_FORWARD_KEY = 90,   /* the hotkeys (both players': [hotkeys]): fast forward, save state, load state */
	ITEM_SAVE_STATE_KEY, ITEM_LOAD_STATE_KEY, ITEM_SLOT_PREVIOUS_KEY, ITEM_SLOT_NEXT_KEY,
	ITEM_ACTION = 100,            /* + an action: a player's key and button for it */
	ITEM_CHEAT = 200,             /* + a cheat of the cheat file (cave/cheats.h) */
	ITEM_REPLAY = 1000,           /* + a replay of the list (replays[]) */
	ITEM_ACHIEVEMENT = 2000,      /* + an achievement (achievements.h) */
	ITEM_STAT_LINE = 3000         /* + a line of the statistics page (stats.h) */
};

static const char *const item_labels[] = {
	"RESUME", "RESET", "TEST MODE", "FAST FWD SPEED", "VOLUME", "WINDOW SIZE", "FULL SCREEN", "CONTROLS",
	"QUIT TO DESKTOP", "RESET TO DEFAULTS", "BACK", "GRAPHICS", "SCALING", "SMOOTHING", "SCANLINES", "VSYNC",
	"ROTATE", "STICK DEADZONE", "SAVE STATE", "LOAD STATE", "GAME OPTIONS", "CHEATS", "SLOWDOWN", "CRT",
	"STATE SLOT", "FILTER", "C BUTTON AUTO FIRE", "INPUT TIMING", "FREE PLAY", "SAVE HIGH SCORES", "SKIP WARNING",
	"FPS METER", "RENDERER", "SIDE PANELS", "START GAME", "SETTINGS", "MAIN MENU", "MAIN MENU AT START",
	"TRAINING", "STAGE", "PLACE", "SHIP", "STYLE", "POWER", "BOMBS", "RETRY ON MISS", "START TRAINING",
	"RETRY TRAINING", "TRAINING MENU", "REPLAYS", "WATCH", "DELETE", "YES, DELETE IT", "NO", "YES", "NO",
	"STOP REPLAY", "HITBOX", "AURA", "ACHIEVEMENTS", "STATISTICS", "UPDATE", "YES, UPDATE", "NOT NOW",
	"CHECK FOR UPDATES", "DIFFICULTY", "SOUND", "MUSIC", "EFFECTS", "VOICES", "SPEED", "BULLETS"
};

static const char *const action_labels[ACTIONS] = {
	"UP", "DOWN", "LEFT", "RIGHT", "SHOT", "BOMB", "AUTO FIRE", "START", "COIN", "SERVICE"
};

static menu_host host;
static bool open, just_closed;
static bool front;                      /* the main menu is up, or its SETTINGS (the game not started) */
static int training_pending;            /* START TRAINING: frames until the game is run there (LOADING... first) */
static int training_failed;             /* frames "COULD NOT REACH IT" is still shown */
static int page, selected, player;
static int items[128], item_count;
static int capture = -1;                /* the action waiting for its key or button, or -1 */
static int capture_frames;
static int held_direction, held_frames; /* the gamepads' d-pad and stick, for their repeats */
static bool update_found;               /* a newer release was found: UPDATE TO ... on the main menu */
static int update_listed = -1;          /* the updater's state the page's entries were made for */

static bool is_action(int item)
{
	return item >= ITEM_ACTION && item < ITEM_ACTION + ACTIONS;
}

static bool is_cheat(int item)
{
	return item >= ITEM_CHEAT && item < ITEM_REPLAY;
}

/* ---- the replays page's list (replay.h) ---- */

#define REPLAYS_LISTED 100
#define REPLAY_ROWS    15               /* shown at a time, scrolled with the choice */

static replay_info replays[REPLAYS_LISTED];
static int replay_count, replay_chosen;

/* the hotkeys' entries on the controls page */
static bool is_hotkey_item(int item)
{
	return item >= ITEM_FAST_FORWARD_KEY && item <= ITEM_SLOT_NEXT_KEY;
}

static int item_hotkey(int item)
{
	return HOTKEY_FAST_FORWARD + (item - ITEM_FAST_FORWARD_KEY);
}

/* ---- the entries ---- */

static void add_items(const int *list, size_t count)
{
	for (size_t index = 0; index < count; index++)
		items[item_count++] = list[index];
}

static void build_items(void)
{
	item_count = 0;
	if (page == PAGE_FRONT)
	{
		static const int front_items[] = { ITEM_START_GAME, ITEM_TRAINING, ITEM_REPLAYS, ITEM_ACHIEVEMENTS,
			ITEM_STATISTICS, ITEM_SETTINGS, ITEM_QUIT };

		if (update_found)
			items[item_count++] = ITEM_UPDATE;    /* (first: seen at once) */
		add_items(front_items, sizeof(front_items) / sizeof(front_items[0]));
		update_listed = update_found;
	}
	else if (page == PAGE_UPDATE)
	{
		/* (asked; while it downloads and installs, only BACK: the download goes on) */
		const int state = updater_state();

		if (state == UPDATER_AVAILABLE || state == UPDATER_FAILED)
		{
			items[item_count++] = ITEM_UPDATE_YES;
			items[item_count++] = ITEM_UPDATE_NO;
		}
		else
			items[item_count++] = ITEM_BACK;
		update_listed = state;
	}
	else if (page == PAGE_MAIN && front)
	{
		/* (the main menu's SETTINGS: what can be set before a game) */
		static const int settings_items[] = { ITEM_SOUND, ITEM_GAME, ITEM_GRAPHICS, ITEM_CONTROLS, ITEM_BACK };

		add_items(settings_items, sizeof(settings_items) / sizeof(settings_items[0]));
	}
	else if (page == PAGE_SOUND)
	{
		static const int sound_items[] = { ITEM_VOLUME, ITEM_MUSIC_VOLUME, ITEM_EFFECTS_VOLUME, ITEM_VOICES_VOLUME,
			ITEM_BACK };

		add_items(sound_items, sizeof(sound_items) / sizeof(sound_items[0]));
	}
	else if (page == PAGE_TRAINING)
	{
		static const int training_items[] = { ITEM_T_STAGE, ITEM_T_POINT, ITEM_T_SHIP, ITEM_T_STYLE, ITEM_T_POWER,
			ITEM_T_BOMBS, ITEM_T_SPEED, ITEM_T_RETRY, ITEM_T_START, ITEM_BACK };

		add_items(training_items, sizeof(training_items) / sizeof(training_items[0]));
	}
	else if (page == PAGE_REPLAYS)
	{
		for (int replay = 0; replay < replay_count; replay++)
			items[item_count++] = ITEM_REPLAY + replay;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_ACHIEVEMENTS)
	{
		for (int index = 0; index < achievements_count(); index++)
			items[item_count++] = ITEM_ACHIEVEMENT + index;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_STATS)
	{
		const int lines = stats_lines();

		for (int line = 0; line < lines && item_count < 120; line++)
			items[item_count++] = ITEM_STAT_LINE + line;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_REPLAY_MENU)
	{
		static const int replay_items[] = { ITEM_WATCH, ITEM_DELETE, ITEM_BACK };

		add_items(replay_items, sizeof(replay_items) / sizeof(replay_items[0]));
	}
	else if (page == PAGE_DELETE)
	{
		static const int delete_items[] = { ITEM_DELETE_NO, ITEM_DELETE_YES };

		add_items(delete_items, sizeof(delete_items) / sizeof(delete_items[0]));
	}
	else if (page == PAGE_SAVE_REPLAY)
	{
		static const int question_items[] = { ITEM_YES, ITEM_NO };

		add_items(question_items, sizeof(question_items) / sizeof(question_items[0]));
	}
	else if (page == PAGE_MAIN && host.in_replay && host.in_replay())
	{
		static const int replay_game_items[] = { ITEM_RESUME, ITEM_STOP_REPLAY, ITEM_SOUND, ITEM_GRAPHICS,
			ITEM_MAIN_MENU, ITEM_QUIT };

		add_items(replay_game_items, sizeof(replay_game_items) / sizeof(replay_game_items[0]));
	}
	else if (page == PAGE_MAIN && host.in_training && host.in_training())
	{
		/* (a training game's: its place again and its page; no RESET or save states: the training is its own save
		   state, F5 / F8 still there) */
		static const int training_game_items[] = { ITEM_RESUME, ITEM_RETRY_TRAINING, ITEM_TRAINING_MENU, ITEM_SOUND,
			ITEM_GAME, ITEM_GRAPHICS, ITEM_CONTROLS, ITEM_MAIN_MENU, ITEM_QUIT };

		add_items(training_game_items, sizeof(training_game_items) / sizeof(training_game_items[0]));
	}
	else if (page == PAGE_MAIN)
	{
		static const int main_items[] = { ITEM_RESUME, ITEM_RESET, ITEM_SOUND, ITEM_SAVE_STATE, ITEM_LOAD_STATE,
			ITEM_STATE_SLOT, ITEM_GAME, ITEM_GRAPHICS, ITEM_CONTROLS, ITEM_MAIN_MENU, ITEM_QUIT };

		add_items(main_items, sizeof(main_items) / sizeof(main_items[0]));
	}
	else if (page == PAGE_GAME)
	{
		static const int game_items[] = { ITEM_DIFFICULTY, ITEM_TIMING, ITEM_HITBOX, ITEM_AURA, ITEM_BULLETS,
			ITEM_FREE_PLAY, ITEM_HIGH_SCORES, ITEM_SKIP_WARNING, ITEM_FRONT_AT_START, ITEM_CHECK_UPDATES, ITEM_SLOWDOWN, ITEM_AUTO_FIRE, ITEM_FAST_SPEED, ITEM_TEST_MODE, ITEM_CHEATS, ITEM_BACK };

		for (size_t index = 0; index < sizeof(game_items) / sizeof(game_items[0]); index++)
			if (!(front && game_items[index] == ITEM_TEST_MODE))    /* (no game running to hold the switch for) */
				items[item_count++] = game_items[index];
	}
	else if (page == PAGE_CHEATS)
	{
		/* the cheat file's that the game can have (they write its RAM) */
		for (int cheat = 0; cheat < cheats_count() && item_count < 120; cheat++)
			if (cheats_usable(cheat))
				items[item_count++] = ITEM_CHEAT + cheat;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_GRAPHICS)
	{
		static const int graphics_items[] = { ITEM_SCALING, ITEM_SMOOTH, ITEM_FILTER, ITEM_SCANLINES, ITEM_CRT,
			ITEM_VSYNC, ITEM_RENDERER, ITEM_FPS, ITEM_PANELS, ITEM_ROTATE, ITEM_SCALE, ITEM_FULLSCREEN, ITEM_BACK };

		add_items(graphics_items, sizeof(graphics_items) / sizeof(graphics_items[0]));
	}
	else
	{
		for (int action = 0; action < ACTIONS; action++)
			items[item_count++] = ITEM_ACTION + action;
		for (int item = ITEM_FAST_FORWARD_KEY; item <= ITEM_SLOT_NEXT_KEY; item++)
			items[item_count++] = item;
		items[item_count++] = ITEM_DEADZONE;
		items[item_count++] = ITEM_DEFAULTS;
		items[item_count++] = ITEM_BACK;
	}
	if (selected >= item_count)
		selected = item_count - 1;
}

static void save(void)
{
	const char *const problem = settings_save(host.file, host.path);

	if (problem)
		fprintf(stderr, "%s: %s\n", host.path, problem);
}

static void close_menu(void)
{
	ui_sound_preview_stop();
	open = false;
	just_closed = true;
	capture = -1;
}

/* the key or button names as the menu shows them */
static const char *pad_label(pad_input input)
{
	static const struct { int button; const char *label; } labels[] = {
		{ SDL_CONTROLLER_BUTTON_A, "A" }, { SDL_CONTROLLER_BUTTON_B, "B" }, { SDL_CONTROLLER_BUTTON_X, "X" },
		{ SDL_CONTROLLER_BUTTON_Y, "Y" }, { SDL_CONTROLLER_BUTTON_BACK, "VIEW" },
		{ SDL_CONTROLLER_BUTTON_GUIDE, "XBOX" }, { SDL_CONTROLLER_BUTTON_START, "START" },
		{ SDL_CONTROLLER_BUTTON_LEFTSTICK, "LS" }, { SDL_CONTROLLER_BUTTON_RIGHTSTICK, "RS" },
		{ SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "LB" }, { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "RB" },
		{ SDL_CONTROLLER_BUTTON_DPAD_UP, "D-UP" }, { SDL_CONTROLLER_BUTTON_DPAD_DOWN, "D-DOWN" },
		{ SDL_CONTROLLER_BUTTON_DPAD_LEFT, "D-LEFT" }, { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "D-RIGHT" },
		{ SDL_CONTROLLER_BUTTON_MISC1, "SHARE" },
	};

	if (input.kind == PAD_TRIGGER)
		return input.index == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? "LT" : "RT";
	if (input.kind != PAD_BUTTON)
		return "-";
	for (size_t index = 0; index < sizeof(labels) / sizeof(labels[0]); index++)
		if (labels[index].button == input.index)
			return labels[index].label;
	return "PAD";
}

static void key_label(SDL_Scancode key, char *out, size_t size)
{
	const char *const name = key == SDL_SCANCODE_UNKNOWN ? "-" : SDL_GetScancodeName(key);

	snprintf(out, size, "%s", *name ? name : "?");
	for (char *letter = out; *letter; letter++)
		*letter = (char)toupper((unsigned char)*letter);
}

/* ---- changing the settings (in use and the file's) ---- */

/* a choice of 'count' moved by 'step', stopping at the ends, or round to the start (wrap) */
static int cycle(int value, int count, int step, bool wrap)
{
	value += step;
	if (value >= count)
		return wrap ? 0 : count - 1;
	if (value < 0)
		return wrap ? count - 1 : 0;
	return value;
}

static int clamp(int value, int low, int high)
{
	return value < low ? low : value > high ? high : value;
}

/* a setting changed: by 'step' (left -1, right +1, stopping at the ends), or by A (wrap: round to the start) */
static void change(int item, int step, bool wrap)
{
	settings *const in_use = host.options, *const file = host.file;

	if (is_cheat(item))
	{
		cheats_set(item - ITEM_CHEAT, !cheats_on(item - ITEM_CHEAT));    /* (for this run: not saved) */
		return;
	}
	switch (item)
	{
	case ITEM_SLOWDOWN:
		in_use->slowdown = file->slowdown = !in_use->slowdown;    /* (main.c puts it in use: game_program.c) */
		break;
	case ITEM_SKIP_WARNING:
		in_use->skip_warning = file->skip_warning = !in_use->skip_warning;
		break;
	case ITEM_FRONT_AT_START:
		in_use->main_menu = file->main_menu = !in_use->main_menu;
		break;
	case ITEM_CHECK_UPDATES:
		in_use->check_updates = file->check_updates = !in_use->check_updates;    /* (from the next start) */
		break;
	case ITEM_HITBOX:
		in_use->hitbox = file->hitbox = !in_use->hitbox;
		break;
	case ITEM_AURA:
		in_use->aura = file->aura = !in_use->aura;
		break;
	case ITEM_BULLETS:
		in_use->bullets = file->bullets = !in_use->bullets;
		break;
	case ITEM_T_STAGE:
		in_use->training_stage = file->training_stage = cycle(in_use->training_stage, TRAINING_STAGES, step, wrap);
		break;
	case ITEM_T_POINT:
		/* (START and BOSS; 2-7: BOSS, Hachi, and TRUE LAST BOSS, Hibachi) */
		in_use->training_point = file->training_point = in_use->training_stage == TRAINING_AREA_7
			? (training_place(in_use->training_stage, in_use->training_point) == TRAINING_TRUE_LAST
				? TRAINING_BOSS : TRAINING_TRUE_LAST)
			: (training_place(in_use->training_stage, in_use->training_point) == TRAINING_BOSS
				? TRAINING_START : TRAINING_BOSS);
		break;
	case ITEM_T_SHIP:
		in_use->training_ship = file->training_ship = cycle(in_use->training_ship, 3, step, wrap);
		break;
	case ITEM_T_STYLE:
		in_use->training_laser = file->training_laser = !in_use->training_laser;
		break;
	case ITEM_T_POWER:
		/* (1-4, MAX, ON PACE) */
		in_use->training_power = file->training_power = cycle(in_use->training_power, 6, step, wrap);
		break;
	case ITEM_T_BOMBS:
		in_use->training_max_bombs = file->training_max_bombs = !in_use->training_max_bombs;
		break;
	case ITEM_T_RETRY:
		in_use->training_retry = file->training_retry = !in_use->training_retry;
		break;
	case ITEM_T_SPEED:
		/* (50-100% in steps of 10: from 100 up, 50) */
		in_use->training_speed = file->training_speed = wrap && in_use->training_speed >= 100 ? 50
			: clamp(in_use->training_speed + step * 10, 50, 100);
		break;
	case ITEM_HIGH_SCORES:
		in_use->save_high_scores = file->save_high_scores = !in_use->save_high_scores;
		break;
	case ITEM_FREE_PLAY:
		in_use->free_play = file->free_play = !in_use->free_play;
		break;
	case ITEM_TIMING:
		in_use->arcade_timing = file->arcade_timing = !in_use->arcade_timing;
		break;
	case ITEM_AUTO_FIRE:
		in_use->c_button_full_auto = file->c_button_full_auto = !in_use->c_button_full_auto;
		host.apply_auto_fire();
		break;
	case ITEM_DIFFICULTY:
	{
		/* the test mode's GAME DIFFICULTY: EEPROM word 4, bits 8-9 (0 NORMAL, 1 EASY, 2 HARD, 3 VERY HARD), read by
		   the game at its reset (each stage's base rank, $101968, from its table at 005092); in the order the menu
		   goes through them: easy, normal, hard, very hard. The EEPROM: kept in its file by main.c */
		static const unsigned order[4] = { 1u, 0u, 2u, 3u };
		const unsigned now = cave_board.eeprom.words[DIFFICULTY_WORD] >> 8 & 3u;
		int at = 0;

		while (order[at] != now)
			at++;
		at = (at + (step < 0 ? 3 : 1)) % 4;
		cave_board.eeprom.words[DIFFICULTY_WORD] =
			(uint16_t)((cave_board.eeprom.words[DIFFICULTY_WORD] & ~0x0300u) | order[at] << 8);
		host.apply_auto_fire();    /* (the EEPROM saved now, as the auto-fire's change saves it; that bit as it was) */
		return;    /* (not a setting of the file's) */
	}
	case ITEM_FAST_SPEED:
	{
		/* the speeds the menu offers (the file may hold any of 2-16: the next one up or down from it) */
		static const int speeds[] = { 2, 3, 4, 8, 16 };
		const int count = (int)(sizeof(speeds) / sizeof(speeds[0]));
		int at = 0;

		while (at < count - 1 && speeds[at] < in_use->fast_forward_speed)
			at++;
		if (step > 0 && speeds[at] <= in_use->fast_forward_speed)
			at = at == count - 1 ? (wrap ? 0 : at) : at + 1;
		else if (step < 0)
			at = at > 0 ? at - 1 : 0;
		in_use->fast_forward_speed = file->fast_forward_speed = speeds[at];
		break;
	}
	case ITEM_VOLUME:
		in_use->volume = wrap && in_use->volume >= 100 ? 0 : clamp(in_use->volume + step * 5, 0, 100);
		file->volume = in_use->volume;
		ui_sound_preview(UI_PREVIEW_MUSIC);    /* (heard as it changes: the music, at the volume being set) */
		break;
	case ITEM_MUSIC_VOLUME:
	case ITEM_EFFECTS_VOLUME:
	case ITEM_VOICES_VOLUME:
	{
		/* (applied by main.c each frame: cave_sound_set_level) */
		int *const now = item == ITEM_MUSIC_VOLUME ? &in_use->music_volume
			: item == ITEM_EFFECTS_VOLUME ? &in_use->effects_volume : &in_use->voices_volume;
		int *const kept = item == ITEM_MUSIC_VOLUME ? &file->music_volume
			: item == ITEM_EFFECTS_VOLUME ? &file->effects_volume : &file->voices_volume;

		*now = wrap && *now >= 100 ? 0 : clamp(*now + step * 5, 0, 100);
		*kept = *now;
		/* (heard as it changes: an example of the group, at its level) */
		ui_sound_preview(item == ITEM_MUSIC_VOLUME ? UI_PREVIEW_MUSIC : item == ITEM_EFFECTS_VOLUME
			? UI_PREVIEW_EFFECTS : UI_PREVIEW_VOICES);
		break;
	}
	case ITEM_SCALE:
		in_use->scale = wrap && in_use->scale >= 8 ? 1 : clamp(in_use->scale + step, 1, 8);
		file->scale = in_use->scale;
		host.apply_window();
		break;
	case ITEM_FULLSCREEN:
	{
		/* OFF, BORDERLESS, EXCLUSIVE */
		int at = !in_use->fullscreen ? 0 : in_use->exclusive ? 2 : 1;

		at = wrap ? (at + 1) % 3 : clamp(at + step, 0, 2);
		in_use->fullscreen = file->fullscreen = at != 0;
		if (at != 0)
			in_use->exclusive = file->exclusive = at == 2;
		host.apply_window();
		break;
	}
	case ITEM_DEADZONE:
		in_use->stick_dead[player] = file->stick_dead[player] =
			wrap && in_use->stick_dead[player] >= 95 ? 5 : clamp(in_use->stick_dead[player] + step * 5, 5, 95);
		break;
	case ITEM_SCALING:
		in_use->scaling = file->scaling = cycle(in_use->scaling, VIDEO_SCALINGS, step, wrap);
		break;
	case ITEM_SMOOTH:
		in_use->smooth = file->smooth = !in_use->smooth;
		host.apply_video();
		break;
	case ITEM_FILTER:
		in_use->filter = file->filter = cycle(in_use->filter, SCALERS, step, wrap);
		host.apply_video();
		break;
	case ITEM_SCANLINES:
		in_use->scanlines = file->scanlines = cycle(in_use->scanlines, 4, step, wrap);
		break;
	case ITEM_CRT:
		in_use->crt = file->crt = cycle(in_use->crt, 3, step, wrap);
		break;
	case ITEM_STATE_SLOT:
		host.state_slot(step);
		return;    /* (not a setting: not saved) */
	case ITEM_VSYNC:
		in_use->vsync = file->vsync = !in_use->vsync;
		host.apply_video();
		break;
	case ITEM_FPS:
		in_use->show_fps = file->show_fps = !in_use->show_fps;
		break;
	case ITEM_RENDERER:
		in_use->renderer = file->renderer = cycle(in_use->renderer, RENDERERS, step, wrap);
		menu_notice("USED FROM THE NEXT START");
		break;
	case ITEM_PANELS:
		in_use->side_panels = file->side_panels = !in_use->side_panels;
		host.apply_window();    /* (the window wider or narrower for them) */
		break;
	case ITEM_ROTATE:
		in_use->rotate = file->rotate = cycle(in_use->rotate, ROTATIONS, step, wrap);
		host.apply_window();    /* (the window's shape turns with it) */
		break;
	default:
		return;
	}
	save();
}

/* to a page, on its first entry */
static void to_page(int to)
{
	page = to;
	selected = 0;
	build_items();
}

/* back to the page before, on the entry that opened this one */
static void replays_page(int at);

static void back(void)
{
	if (page == PAGE_REPLAY_MENU || page == PAGE_DELETE)
	{
		replays_page(replay_chosen);
		return;
	}
	if (page == PAGE_REPLAYS || page == PAGE_ACHIEVEMENTS || page == PAGE_STATS)
	{
		/* (back to the main menu, on the entry that opened it) */
		const int opened_by = page == PAGE_REPLAYS ? ITEM_REPLAYS : page == PAGE_STATS ? ITEM_STATISTICS
			: ITEM_ACHIEVEMENTS;

		page = PAGE_FRONT;
		build_items();
		for (selected = 0; selected < item_count - 1 && items[selected] != opened_by; selected++)
			;
		return;
	}
	if (page == PAGE_UPDATE)
	{
		/* (back to the main menu, on UPDATE) */
		page = PAGE_FRONT;
		build_items();
		for (selected = 0; selected < item_count - 1 && items[selected] != ITEM_UPDATE; selected++)
			;
		return;
	}
	if (page == PAGE_TRAINING)
	{
		/* (back to the main menu, on TRAINING) */
		page = PAGE_FRONT;
		build_items();
		for (selected = 0; selected < item_count - 1 && items[selected] != ITEM_TRAINING; selected++)
			;
		return;
	}
	if (page == PAGE_MAIN && front)
	{
		/* (the main menu's SETTINGS: back to the main menu, on SETTINGS) */
		page = PAGE_FRONT;
		build_items();
		for (selected = 0; selected < item_count - 1 && items[selected] != ITEM_SETTINGS; selected++)
			;
		return;
	}
	{
	const int opened_by = page == PAGE_GRAPHICS ? ITEM_GRAPHICS : page == PAGE_GAME ? ITEM_GAME
		: page == PAGE_SOUND ? ITEM_SOUND : page == PAGE_CHEATS ? ITEM_CHEATS : ITEM_CONTROLS;

	page = page == PAGE_CHEATS ? PAGE_GAME : PAGE_MAIN;
	build_items();
	for (selected = 0; selected < item_count - 1 && items[selected] != opened_by; selected++)
		;
	}
}

static void choose(int item)
{
	switch (item)
	{
	case ITEM_RESUME:
		close_menu();
		break;
	case ITEM_START_GAME:
		front = false;
		close_menu();    /* (the game starts: it runs from its reset, as the board switched on) */
		break;
	case ITEM_SETTINGS:
		to_page(PAGE_MAIN);
		break;
	case ITEM_TRAINING:
		to_page(PAGE_TRAINING);
		break;
	case ITEM_REPLAYS:
		replays_page(0);
		break;
	case ITEM_ACHIEVEMENTS:
		to_page(PAGE_ACHIEVEMENTS);
		break;
	case ITEM_STATISTICS:
		to_page(PAGE_STATS);
		break;
	case ITEM_UPDATE:
		to_page(PAGE_UPDATE);
		break;
	case ITEM_UPDATE_YES:
		updater_download();    /* (on its thread: the page shows how far it is) */
		build_items();
		selected = 0;
		break;
	case ITEM_UPDATE_NO:
		back();
		break;
	case ITEM_WATCH:
		if (host.play_replay(replays[replay_chosen].file))
		{
			front = false;
			close_menu();
		}
		break;
	case ITEM_DELETE:
		to_page(PAGE_DELETE);
		break;
	case ITEM_DELETE_YES:
		menu_notice(replay_delete(replays[replay_chosen].file) ? "REPLAY DELETED" : "IT COULD NOT BE DELETED");
		replays_page(replay_chosen);
		break;
	case ITEM_DELETE_NO:
		to_page(PAGE_REPLAY_MENU);
		break;
	case ITEM_YES:
	case ITEM_NO:
		host.keep_replay(item == ITEM_YES);
		close_menu();
		break;
	case ITEM_STOP_REPLAY:
		host.stop_replay();
		break;
	case ITEM_T_START:
		training_failed = 0;
		training_pending = 2;    /* (LOADING... on the screen first: menu_draw runs it a frame later) */
		break;
	case ITEM_RETRY_TRAINING:
		host.retry_training();
		close_menu();
		break;
	case ITEM_TRAINING_MENU:
		host.main_menu();
		menu_open_training();
		break;
	case ITEM_MAIN_MENU:
		host.main_menu();
		menu_open_front();
		break;
	case ITEM_RESET:
		host.reset();
		close_menu();
		break;
	case ITEM_TEST_MODE:
		host.test_mode();
		close_menu();
		break;
	case ITEM_CONTROLS:
		to_page(PAGE_CONTROLS);
		break;
	case ITEM_GRAPHICS:
		to_page(PAGE_GRAPHICS);
		break;
	case ITEM_GAME:
		to_page(PAGE_GAME);
		break;
	case ITEM_CHEATS:
		to_page(PAGE_CHEATS);
		break;
	case ITEM_SOUND:
		to_page(PAGE_SOUND);
		break;
	case ITEM_QUIT:
		host.quit();
		close_menu();
		break;
	case ITEM_SAVE_STATE:
	case ITEM_LOAD_STATE:
		close_menu();
		host.state(item == ITEM_SAVE_STATE);
		break;
	case ITEM_DEFAULTS:
	{
		/* the player's keys, buttons and stick, and the hotkeys, as the port had them before the file */
		settings defaults;

		settings_defaults(&defaults);
		for (settings *s = host.options; s; s = s == host.options ? host.file : NULL)
		{
			memcpy(s->keys[player], defaults.keys[player], sizeof(defaults.keys[player]));
			memcpy(s->buttons[player], defaults.buttons[player], sizeof(defaults.buttons[player]));
			s->stick_dead[player] = defaults.stick_dead[player];
			s->left_stick[player] = defaults.left_stick[player];
			for (int hotkey = HOTKEY_FAST_FORWARD; hotkey <= HOTKEY_SLOT_NEXT; hotkey++)
			{
				memcpy(s->hot_keys[hotkey], defaults.hot_keys[hotkey], sizeof(defaults.hot_keys[hotkey]));
				memcpy(s->hot_buttons[hotkey], defaults.hot_buttons[hotkey], sizeof(defaults.hot_buttons[hotkey]));
			}
		}
		save();
		break;
	}
	case ITEM_BACK:
		back();
		break;
	default:
		if (item >= ITEM_REPLAY && item < ITEM_ACHIEVEMENT)
		{
			replay_chosen = item - ITEM_REPLAY;
			to_page(PAGE_REPLAY_MENU);
			break;
		}
		if (is_action(item) || is_hotkey_item(item))
		{
			capture = item;
			capture_frames = CAPTURE_FRAMES;
		}
		else
			change(item, 1, true);
		break;
	}
}

static void navigate(int nav)
{
	const int item = items[selected];

	/* (the menu's sounds, ui_sound.h: the cursor's, a choice's, going back's) */
	if (nav == NAV_UP || nav == NAV_DOWN || nav == NAV_LEFT || nav == NAV_RIGHT)
	{
		if (page != PAGE_SOUND)
			ui_sound_play(UI_SOUND_MOVE);    /* (the SOUND page: its example is heard instead) */
	}
	else if (nav == NAV_CHOOSE)
		ui_sound_play(item == ITEM_BACK || item == ITEM_RESUME || item == ITEM_NO || item == ITEM_DELETE_NO
			|| item == ITEM_UPDATE_NO ? UI_SOUND_BACK : UI_SOUND_CHOOSE);
	else if ((nav == NAV_BACK || nav == NAV_CLOSE) && page != PAGE_FRONT)
		ui_sound_play(UI_SOUND_BACK);
	switch (nav)
	{
	case NAV_UP:
		selected = (selected + item_count - 1) % item_count;
		break;
	case NAV_DOWN:
		selected = (selected + 1) % item_count;
		break;
	case NAV_LEFT:
	case NAV_RIGHT:
		if (page == PAGE_CONTROLS && item != ITEM_DEADZONE)
			player ^= 1;
		else
			change(item, nav == NAV_RIGHT ? 1 : -1, false);
		break;
	case NAV_CHOOSE:
		choose(item);
		break;
	case NAV_BACK:
		if (page == PAGE_FRONT)
			break;    /* (the main menu stays until a game starts) */
		if (page == PAGE_SAVE_REPLAY)
		{
			choose(ITEM_NO);
			break;
		}
		if (page != PAGE_MAIN || front)
			back();
		else
			close_menu();
		break;
	case NAV_CLOSE:
		if (page == PAGE_SAVE_REPLAY)
		{
			choose(ITEM_NO);
			break;
		}
		if (front)
		{
			if (page != PAGE_FRONT)
				menu_open_front();    /* (from its settings: back to the main menu, not into a game) */
			break;
		}
		close_menu();
		break;
	}
}

/* ---- remapping: the next key or button is the action's ---- */

/* the keys and buttons of the item being remapped, in 's' */
static SDL_Scancode *captured_keys(settings *s)
{
	return is_hotkey_item(capture) ? s->hot_keys[item_hotkey(capture)] : s->keys[player][capture - ITEM_ACTION];
}

static pad_input *captured_buttons(settings *s)
{
	return is_hotkey_item(capture) ? s->hot_buttons[item_hotkey(capture)]
		: s->buttons[player][capture - ITEM_ACTION];
}

static void bind_key(SDL_Scancode key)
{
	SDL_Scancode keys[BINDINGS_MAX] = { key };

	memcpy(captured_keys(host.options), keys, sizeof(keys));
	memcpy(captured_keys(host.file), keys, sizeof(keys));
	capture = -1;
	save();
}

static void bind_pad(pad_input input)
{
	pad_input inputs[BINDINGS_MAX] = { input };

	memcpy(captured_buttons(host.options), inputs, sizeof(inputs));
	memcpy(captured_buttons(host.file), inputs, sizeof(inputs));
	capture = -1;
	save();
}

static bool capture_event(const SDL_Event *event)
{
	if (event->type == SDL_KEYDOWN && !event->key.repeat)
	{
		if (event->key.keysym.scancode == SDL_SCANCODE_ESCAPE)
			capture = -1;    /* (cancel) */
		else
			bind_key(event->key.keysym.scancode);
	}
	else if (event->type == SDL_CONTROLLERBUTTONDOWN)
		bind_pad((pad_input){ PAD_BUTTON, event->cbutton.button });
	else if (event->type == SDL_CONTROLLERAXISMOTION && event->caxis.value > 16384
			&& (event->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || event->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
		bind_pad((pad_input){ PAD_TRIGGER, event->caxis.axis });
	return true;
}

/* ---- the menu's own input ---- */

void menu_init(const menu_host *menu)
{
	host = *menu;
}

bool menu_is_open(void)
{
	return open;
}

void menu_open(void)
{
	open = true;
	front = false;
	page = PAGE_MAIN;
	selected = 0;
	capture = -1;
	held_direction = NAV_NONE;
	build_items();
}

void menu_open_front(void)
{
	menu_open();
	front = true;
	page = PAGE_FRONT;
	build_items();
	selected = 0;    /* (on START GAME) */
}

void menu_open_training(void)
{
	menu_open_front();
	page = PAGE_TRAINING;
	build_items();
	for (selected = 0; selected < item_count - 1 && items[selected] != ITEM_T_START; selected++)
		;    /* (on START TRAINING: again at once) */
}

/* the replays page, its list read again, on the replay at 'at' (or the last) */
static void replays_page(int at)
{
	replay_count = replay_list(replays, REPLAYS_LISTED);
	page = PAGE_REPLAYS;
	build_items();
	selected = at < item_count - 1 ? at : item_count - 1;
	if (selected > 0 && selected >= replay_count && replay_count > 0)
		selected = replay_count - 1;
}

void menu_open_replays(void)
{
	menu_open_front();
	replays_page(0);
}

void menu_ask_save_replay(void)
{
	open = true;
	front = false;
	page = PAGE_SAVE_REPLAY;
	selected = 0;
	capture = -1;
	held_direction = NAV_NONE;
	build_items();
}

bool menu_in_front(void)
{
	return open && front;
}

bool menu_just_closed(void)
{
	const bool closed = just_closed;

	just_closed = false;
	return closed;
}

bool menu_event(const SDL_Event *event)
{
	if (!open)
		return false;
	if (capture >= 0)
		return capture_event(event);
	if (event->type == SDL_KEYDOWN)
	{
		const SDL_Scancode key = event->key.keysym.scancode;
		int nav = NAV_NONE;

		if (key == SDL_SCANCODE_UP)
			nav = NAV_UP;
		else if (key == SDL_SCANCODE_DOWN)
			nav = NAV_DOWN;
		else if (key == SDL_SCANCODE_LEFT)
			nav = NAV_LEFT;
		else if (key == SDL_SCANCODE_RIGHT)
			nav = NAV_RIGHT;
		else if (event->key.repeat)
			nav = NAV_NONE;    /* (only the arrows repeat) */
		else if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER || key == SDL_SCANCODE_Z
				|| key == SDL_SCANCODE_SPACE)
			nav = NAV_CHOOSE;
		else if (key == SDL_SCANCODE_BACKSPACE || key == SDL_SCANCODE_X)
			nav = NAV_BACK;
		else if (key == SDL_SCANCODE_ESCAPE)
			nav = NAV_CLOSE;
		navigate(nav);
	}
	else if (event->type == SDL_CONTROLLERBUTTONDOWN)
	{
		const int button = event->cbutton.button;

		if (button == SDL_CONTROLLER_BUTTON_A || button == SDL_CONTROLLER_BUTTON_START)
			navigate(NAV_CHOOSE);
		else if (button == SDL_CONTROLLER_BUTTON_B)
			navigate(NAV_BACK);
		else if (button == SDL_CONTROLLER_BUTTON_BACK)
			navigate(NAV_CLOSE);
		/* (the d-pad is read in menu_tick, with the stick, for their repeats) */
	}
	return true;
}

/* the direction the gamepads' d-pads and sticks point */
static int pad_direction(void)
{
	for (int index = 0; index < 2; index++)
	{
		SDL_GameController *const pad = host.pads[index];
		int x, y;

		if (!pad)
			continue;
		x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
		y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) || y < -STICK_DEAD)
			return NAV_UP;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y > STICK_DEAD)
			return NAV_DOWN;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -STICK_DEAD)
			return NAV_LEFT;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > STICK_DEAD)
			return NAV_RIGHT;
	}
	return NAV_NONE;
}

void menu_tick(void)
{
	int direction;

	if (!open)
		return;
	{
		/* the SOUND page's example stops as its entry is left (another entry, page or the menu closed) */
		static int previewed = -1;
		const int now = page == PAGE_SOUND ? items[selected] : -1;

		if (now != previewed)
			ui_sound_preview_stop();
		previewed = now;
	}
	if (updater_state() == UPDATER_AVAILABLE)
		update_found = true;
	if (page == PAGE_FRONT && update_listed != update_found)
	{
		/* (the check has found a newer release while the main menu was up: its entry added, the choice kept) */
		const int was = items[selected];

		build_items();
		for (selected = 0; selected < item_count - 1 && items[selected] != was; selected++)
			;
	}
	if (page == PAGE_UPDATE)
	{
		if (updater_state() == UPDATER_READY && host.install_update)
			host.install_update();    /* (the files put in place and the program closed to start again, or FAILED) */
		if (update_listed != updater_state())
		{
			build_items();
			selected = 0;
		}
	}
	if (capture >= 0)
	{
		if (--capture_frames <= 0)
			capture = -1;    /* (nothing pressed: no change) */
		return;
	}
	direction = pad_direction();
	if (direction != held_direction)
	{
		held_direction = direction;
		held_frames = 0;
		navigate(direction);
	}
	else if (direction != NAV_NONE && ++held_frames >= REPEAT_FIRST && (held_frames - REPEAT_FIRST) % REPEAT_NEXT == 0)
		navigate(direction);
}

/* ---- drawing ---- */

static uint32_t shade(uint32_t colour, int percent)
{
	const uint32_t r = (colour >> 16 & 255u) * (uint32_t)percent / 100u, g = (colour >> 8 & 255u) * (uint32_t)percent / 100u,
		b = (colour & 255u) * (uint32_t)percent / 100u;

	return r << 16 | g << 8 | b;
}

/* a tile's pen at (tx, ty): MAME's layout_8x8x8, as video.c reads the layer (a pixel's low nibble in one byte, its
   high nibble in the next) */
static unsigned tile_pen(const uint8_t *tile, int tx, int ty)
{
	const uint8_t *const pair = &tile[ty * 8 + (tx >> 1) * 2];
	const unsigned shift = (tx & 1) ? 0u : 4u;

	return (unsigned)((pair[1] >> shift & 15u) << 4 | (pair[0] >> shift & 15u));
}

/* character 'ch' at pixel (x, y) of the picture, 'scale' times its size */
static void put_glyph(uint32_t *out, int x, int y, char ch, uint32_t colour, int scale)
{
	static const int pens[16] = { 0, 100, 92, 84, 76, 68, 60, 60, 60, 60, 60, 60, 60, 60, 60, -1 };
	const int code = toupper((unsigned char)ch);
	const uint8_t *tile;

	if (code <= 0x20 || code >= 0x60 || x < 0 || x + 8 * scale > PICTURE_W || y < 0 || y + 8 * scale > PICTURE_H
			|| !rom_set.layer[2])
		return;
	tile = rom_set.layer[2] + code * 64;
	for (int i = 0; i < 8; i++)          /* the picture's x within the character: the tile's row */
		for (int j = 0; j < 8; j++)      /* its y: the tile's column, from the right */
		{
			const unsigned pen = tile_pen(tile, 7 - j, i);
			uint32_t value;

			if (pen == 0)
				continue;
			value = pen >= 16 ? shade(colour, 60) : pens[pen] < 0 ? 0x101018u : shade(colour, pens[pen]);
			for (int dy = 0; dy < scale; dy++)
				for (int dx = 0; dx < scale; dx++)
					out[(y + j * scale + dy) * PICTURE_W + x + i * scale + dx] = value;
		}
}

static void put_char_y(uint32_t *out, int column, int y, char ch, uint32_t colour)
{
	if (column >= 0 && column < COLUMNS)
		put_glyph(out, column * 8, y, ch, colour, 1);
}

static void put_text_y(uint32_t *out, int column, int y, const char *text, uint32_t colour)
{
	for (; *text; text++, column++)
		put_char_y(out, column, y, *text, colour);
}

static void put_right_y(uint32_t *out, int right_column, int y, const char *text, uint32_t colour)
{
	put_text_y(out, right_column - (int)strlen(text) + 1, y, text, colour);
}

/* a line centred to the pixel (an odd number of letters half a letter off the columns) */
static void put_centre_y(uint32_t *out, int y, const char *text, uint32_t colour)
{
	for (int x = (PICTURE_W - (int)strlen(text) * 8) / 2; *text; text++, x += 8)
		put_glyph(out, x, y, *text, colour, 1);
}

static void put_centre(uint32_t *out, int row, const char *text, uint32_t colour)
{
	put_centre_y(out, row * 8, text, colour);
}

static void fill(uint32_t *out, int x0, int y0, int x1, int y1, uint32_t colour)
{
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++)
			out[y * PICTURE_W + x] = colour;
}

/* ---- the notice ---- */

#define NOTICE_FRAMES 100

static char notice[COLUMNS + 1];
static int notice_frames;

void menu_notice(const char *text)
{
	snprintf(notice, sizeof(notice), "%s", text);
	notice_frames = NOTICE_FRAMES;
}

const uint32_t *menu_with_notice(const uint32_t *picture)
{
	static uint32_t out[PICTURE_W * PICTURE_H];
	const int width = (int)strlen(notice), column = (COLUMNS - width) / 2, row = 5;

	if (notice_frames <= 0)
		return picture;
	notice_frames--;
	memcpy(out, picture, sizeof(out));
	/* a dark band under it */
	for (int y = row * 8 - 3; y < row * 8 + 11; y++)
		for (int x = column * 8 - 6; x < (column + width) * 8 + 6; x++)
			if (x >= 0 && x < PICTURE_W && y >= 0 && y < PICTURE_H)
				out[y * PICTURE_W + x] = 0x000000u;
	put_text_y(out, column, row * 8, notice, GOLD);
	return out;
}

void menu_put_meter(uint32_t *picture, const char *const *lines, int count)
{
	int widest = 0;

	for (int line = 0; line < count; line++)
		if ((int)strlen(lines[line]) > widest)
			widest = (int)strlen(lines[line]);
	if (widest > COLUMNS)
		widest = COLUMNS;
	{
		const int left = PICTURE_W - widest * 8 - 2, top = PICTURE_H - count * 10 - 2;

		fill(picture, left - 2, top - 2, PICTURE_W, PICTURE_H, 0x000000u);
		for (int line = 0; line < count; line++)
			for (int at = 0; lines[line][at] && at < widest; at++)
				put_glyph(picture, left + at * 8, top + line * 10, lines[line][at], line ? WHITE : GOLD, 1);
	}
}

void menu_put_speed(uint32_t *picture, int percent)
{
	char text[16];
	int width, left;

	snprintf(text, sizeof(text), "SPEED %d%%", percent);
	width = (int)strlen(text) * 8;
	left = (PICTURE_W - width) / 2;
	fill(picture, left - 2, PICTURE_H - 12, left + width + 2, PICTURE_H, 0x000000u);
	for (int at = 0; text[at]; at++)
		put_glyph(picture, left + at * 8, PICTURE_H - 10, text[at], CYAN, 1);
}

/* ---- the entries' names and values ---- */

static const char *item_label(int item)
{
	if (item >= ITEM_STAT_LINE)
		return stats_label(item - ITEM_STAT_LINE);
	if (item >= ITEM_ACHIEVEMENT)
		return achievements_name(item - ITEM_ACHIEVEMENT);
	if (item >= ITEM_REPLAY && item < ITEM_ACHIEVEMENT)
	{
		/* a replay: its date, "11-18 18:40" */
		static char date[16];

		snprintf(date, sizeof(date), "%.11s", replays[item - ITEM_REPLAY].date + 5);
		return date;
	}
	if (is_hotkey_item(item))
	{
		static const char *const names[] = { "FAST FWD", "SAVE STATE", "LOAD STATE", "PREV SLOT", "NEXT SLOT" };

		return names[item - ITEM_FAST_FORWARD_KEY];
	}
	if (is_cheat(item))
	{
		/* the cheat file's name, made to fit beside its ON / OFF ("Maximum" as "Max") */
		static char name[64];
		const char *const full = cheats_name(item - ITEM_CHEAT), *const longer = strstr(full, "Maximum");

		if (longer)
			snprintf(name, sizeof(name), "%.*sMax%s", (int)(longer - full), full, longer + 7);
		else
			snprintf(name, sizeof(name), "%s", full);
		name[19] = '\0';
		return name;
	}
	if (item == ITEM_UPDATE)
	{
		static char label[40];

		snprintf(label, sizeof(label), "UPDATE TO V%s", updater_latest());
		return label;
	}
	if (item == ITEM_UPDATE_YES && updater_state() == UPDATER_FAILED)
		return "TRY AGAIN";
	return is_action(item) ? action_labels[item - ITEM_ACTION] : item_labels[item];
}

static void item_value(int item, char *out, size_t size)
{
	const settings *const s = host.options;

	out[0] = '\0';
	if (item >= ITEM_STAT_LINE)
	{
		const char *const value = stats_value(item - ITEM_STAT_LINE);

		snprintf(out, size, "%s", value ? value : "");
		return;
	}
	if (item >= ITEM_ACHIEVEMENT)
	{
		snprintf(out, size, "%s", achievements_date(item - ITEM_ACHIEVEMENT) ? "DONE" : "");
		return;
	}
	if (item >= ITEM_REPLAY && item < ITEM_ACHIEVEMENT)
	{
		/* its furthest stage and score (the score as the game shows it: its last digit 0) */
		const replay_info *const r = &replays[item - ITEM_REPLAY];

		if (r->all_clear)
			snprintf(out, size, "ALL %lu0", (unsigned long)r->score);
		else
			snprintf(out, size, "%d-%d %lu0", r->stage / 6 + 1, r->stage % 6 + 1, (unsigned long)r->score);
		return;
	}
	switch (item)
	{
	case ITEM_SLOWDOWN:
		snprintf(out, size, "%s", s->slowdown ? "ON" : "OFF");
		break;
	case ITEM_SKIP_WARNING:
		snprintf(out, size, "%s", s->skip_warning ? "ON" : "OFF");
		break;
	case ITEM_FRONT_AT_START:
		snprintf(out, size, "%s", s->main_menu ? "ON" : "OFF");
		break;
	case ITEM_CHECK_UPDATES:
		snprintf(out, size, "%s", s->check_updates ? "ON" : "OFF");
		break;
	case ITEM_HITBOX:
		snprintf(out, size, "%s", s->hitbox ? "ON" : "OFF");
		break;
	case ITEM_AURA:
		snprintf(out, size, "%s", s->aura ? "ON" : "OFF");
		break;
	case ITEM_BULLETS:
		snprintf(out, size, "%s", s->bullets ? "ON" : "OFF");
		break;
	case ITEM_T_STAGE:
		if (s->training_stage == TRAINING_AREA_7)
			snprintf(out, size, "2-7");    /* (the second loop's hidden Area 7) */
		else
			snprintf(out, size, "%d-%d", s->training_stage / 6 + 1, s->training_stage % 6 + 1);
		break;
	case ITEM_T_POINT:
	{
		const int point = training_place(s->training_stage, s->training_point);

		if (s->training_stage == TRAINING_AREA_7)
			snprintf(out, size, "%s", point == TRAINING_TRUE_LAST ? "HIBACHI (TLB)" : "HACHI");
		else
			snprintf(out, size, "%s", point == TRAINING_BOSS ? "BOSS" : "START");
		break;
	}
	case ITEM_T_SHIP:
		snprintf(out, size, "TYPE-%c", 'A' + s->training_ship);
		break;
	case ITEM_T_STYLE:
		snprintf(out, size, "%s", s->training_laser ? "LASER" : "SHOT");
		break;
	case ITEM_T_POWER:
		if (s->training_power == TRAINING_POWER_ON_PACE)
		{
			/* (what it gives at the place chosen: "ON PACE (2)") */
			const int power = training_on_pace(s->training_stage, s->training_point);

			if (power >= TRAINING_MAX_POWER)
				snprintf(out, size, "ON PACE (MAX)");
			else
				snprintf(out, size, "ON PACE (%d)", power + 1);
		}
		else if (s->training_power >= TRAINING_MAX_POWER)
			snprintf(out, size, "MAX");
		else
			snprintf(out, size, "%d", s->training_power + 1);
		break;
	case ITEM_T_SPEED:
		snprintf(out, size, "%d%%%s", s->training_speed, s->training_speed == 100 ? " (ARCADE)" : "");
		break;
	case ITEM_T_BOMBS:
		snprintf(out, size, "%s", s->training_max_bombs ? "6 (MAX)" : "3");
		break;
	case ITEM_T_RETRY:
		snprintf(out, size, "%s", s->training_retry ? "ON" : "OFF");
		break;
	case ITEM_HIGH_SCORES:
		snprintf(out, size, "%s", s->save_high_scores ? "ON" : "OFF");
		break;
	case ITEM_FREE_PLAY:
		snprintf(out, size, "%s", s->free_play ? "ON" : "OFF");
		break;
	case ITEM_TIMING:
		snprintf(out, size, "%s", s->arcade_timing ? "ARCADE" : "FAST");
		break;
	case ITEM_AUTO_FIRE:
		snprintf(out, size, "%s", s->c_button_full_auto ? "ON" : "OFF");
		break;
	case ITEM_DIFFICULTY:
	{
		static const char *const names[4] = { "NORMAL", "EASY", "HARD", "VERY HARD" };

		snprintf(out, size, "%s", names[cave_board.eeprom.words[DIFFICULTY_WORD] >> 8 & 3u]);
		break;
	}
	case ITEM_FAST_SPEED:
		snprintf(out, size, "%dX", s->fast_forward_speed);
		break;
	case ITEM_VOLUME:
		snprintf(out, size, "%d%%", s->volume);
		break;
	case ITEM_MUSIC_VOLUME:
		snprintf(out, size, "%d%%", s->music_volume);
		break;
	case ITEM_EFFECTS_VOLUME:
		snprintf(out, size, "%d%%", s->effects_volume);
		break;
	case ITEM_VOICES_VOLUME:
		snprintf(out, size, "%d%%", s->voices_volume);
		break;
	case ITEM_SCALE:
		snprintf(out, size, "%dX", s->scale);
		break;
	case ITEM_FULLSCREEN:
		snprintf(out, size, "%s", !s->fullscreen ? "OFF" : s->exclusive ? "EXCLUSIVE" : "BORDERLESS");
		break;
	case ITEM_CONTROLS:
	case ITEM_GRAPHICS:
	case ITEM_GAME:
	case ITEM_CHEATS:
	case ITEM_SOUND:
		snprintf(out, size, ">");
		break;
	case ITEM_DEADZONE:
		snprintf(out, size, "%d%%", s->stick_dead[player]);
		break;
	case ITEM_SCALING:
		snprintf(out, size, "%s", settings_scaling_names[s->scaling]);
		break;
	case ITEM_SMOOTH:
		snprintf(out, size, "%s", s->smooth ? "ON" : "OFF");
		break;
	case ITEM_FILTER:
		snprintf(out, size, "%s", scaler_name(s->filter));
		break;
	case ITEM_SCANLINES:
		snprintf(out, size, "%s", settings_scanline_names[s->scanlines]);
		break;
	case ITEM_CRT:
		snprintf(out, size, "%s", settings_crt_names[s->crt]);
		break;
	case ITEM_STATE_SLOT:
	{
		const int slot = host.state_slot(0);

		snprintf(out, size, "%d%s", slot + 1, !host.state_used(slot) ? " (EMPTY)" : "");
		break;
	}
	case ITEM_VSYNC:
		snprintf(out, size, "%s", s->vsync ? "ON" : "OFF");
		break;
	case ITEM_FPS:
		snprintf(out, size, "%s", s->show_fps ? "ON" : "OFF");
		break;
	case ITEM_RENDERER:
		snprintf(out, size, "%s", settings_renderer_names[s->renderer]);
		break;
	case ITEM_PANELS:
		snprintf(out, size, "%s", s->side_panels ? "ON" : "OFF");
		break;
	case ITEM_ROTATE:
		snprintf(out, size, "%s", s->rotate == ROTATE_NONE ? "OFF" : settings_rotate_names[s->rotate]);
		break;
	default:
		if (is_cheat(item))
			snprintf(out, size, "%s", cheats_on(item - ITEM_CHEAT) ? "ON" : "OFF");
		else if (is_action(item) || is_hotkey_item(item))
		{
			const bool hotkey = is_hotkey_item(item);
			char key[32];

			key_label(hotkey ? s->hot_keys[item_hotkey(item)][0] : s->keys[player][item - ITEM_ACTION][0], key,
				sizeof(key));
			if (strlen(key) > 10)
				key[10] = '\0';
			snprintf(out, size, "%s / %s", key,
				pad_label(hotkey ? s->hot_buttons[item_hotkey(item)][0] : s->buttons[player][item - ITEM_ACTION][0]));
		}
		break;
	}
}

static void put_hints(uint32_t *out, const char *first, const char *second)
{
	const int y = (PANEL_TOP + PANEL_ROWS - 3) * 8;

	put_text_y(out, (COLUMNS - (int)strlen(first)) / 2, y, first, GREY);
	put_text_y(out, (COLUMNS - (int)strlen(second)) / 2, y + 11, second, GREY);
}

/* the first of the cheats (or replays) shown (the list scrolled to keep the choice in it), and how many there are */
static int cheats_first(int *total)
{
	const int rows = page == PAGE_CHEATS ? CHEAT_ROWS : REPLAY_ROWS;
	const int cheats = item_count - 1, chosen = selected < cheats ? selected : cheats - 1;
	int first = chosen - rows / 2;

	if (first > cheats - rows)
		first = cheats - rows;
	if (first < 0)
		first = 0;
	*total = cheats;
	return first;
}

/* ---- the main menu: the title over a dark sky, its entries ---- */

/* the bee (the game's own sprite, from the player's ROM set: a frame of its flight, 32 x 32) at (x, y), head up; its
   colours made here (the game's palette is not set up before it runs): gold, brighter for its
   lower pens, as the bee's own palette goes */
#define BEE_CODE 0x4496u

static void put_bee(uint32_t *out, int x, int y)
{
	if (!rom_set.sprites)
		return;
	for (int row = 0; row < 32; row++)
		for (int column = 0; column < 32; column++)
		{
			const uint32_t pixel = (BEE_CODE << 8) + (uint32_t)((31 - column) * 32 + (31 - row));
			const uint8_t byte = rom_set.sprites[(pixel >> 1) & (rom_set_region_size(CAVE_ROM_SPRITES) - 1u)];
			const unsigned pen = (pixel & 1u) ? (byte & 15u) : (byte >> 4);
			uint32_t colour;

			if (!pen || x + column < 0 || x + column >= PICTURE_W || y + row < 0 || y + row >= PICTURE_H)
				continue;
			if (pen == 1u)
				colour = 0xFFFFFFu;
			else if (pen < 12u)
				colour = shade(GOLD, 110 - (int)pen * 9);
			else
				colour = shade(0xC0E000u, 130 - (int)pen * 8);
			out[(y + row) * PICTURE_W + x + column] = colour;
		}
}

/* a BCD score as the game shows it (its last digit, the continues, left as 0), with commas */
static void score_text(char *out, size_t size, const unsigned char *bcd)
{
	char digits[16];
	int length = 0, at = 0;

	for (int byte = 0; byte < 4; byte++)
	{
		digits[length++] = (char)('0' + (bcd[byte] >> 4));
		digits[length++] = (char)('0' + (bcd[byte] & 15));
	}
	digits[length++] = '0';
	digits[length] = '\0';
	while (at < length - 1 && digits[at] == '0')
		at++;
	{
		char grouped[24];
		int put = 0;

		for (int i = at; i < length; i++)
		{
			grouped[put++] = digits[i];
			if ((length - 1 - i) % 3 == 0 && i < length - 1)
				grouped[put++] = ',';
		}
		grouped[put] = '\0';
		snprintf(out, size, "%s", grouped);
	}
}

static void draw_front(uint32_t *out)
{
	static const char title[] = "DODONPACHI";
	const int scale = 2, title_w = (int)strlen(title) * 8 * scale, title_x = (PICTURE_W - title_w) / 2;
	const unsigned char *const table = hiscore_table();

	for (int y = 0; y < PICTURE_H; y++)
	{
		/* dark red to black (the game's colours), with a few fixed stars */
		const uint32_t red = (uint32_t)(44 - y * 36 / PICTURE_H);

		for (int x = 0; x < PICTURE_W; x++)
		{
			const unsigned hash = (unsigned)(x * 73856093u ^ y * 19349663u);

			out[y * PICTURE_W + x] = hash % 997u == 0u ? 0xA09088u : red << 16 | (red / 4u) << 8 | red / 3u;
		}
	}
	put_centre(out, 2, rom_set.version ? "ARCADE  JAPANESE VERSION" : "ARCADE  INTERNATIONAL VERSION", GREY);
	/* the title between two bees, its shadow under it */
	put_bee(out, title_x - 36, 46);
	put_bee(out, title_x + title_w + 4, 46);
	for (int at = 0; title[at]; at++)
		put_glyph(out, title_x + at * 8 * scale + 2, 56, title[at], 0x300800u, scale);
	for (int at = 0; title[at]; at++)
		put_glyph(out, title_x + at * 8 * scale, 54, title[at], GOLD, scale);
	put_centre_y(out, 80, "R E C O M P", GREY);
	/* the high score kept (the file's #1) */
	if (table)
	{
		char score[24];

		score_text(score, sizeof(score), table);
		put_centre_y(out, 104, "HIGH SCORE", GREY);
		put_centre_y(out, 116, score, WHITE);
	}
	for (int index = 0; index < item_count; index++)
	{
		const bool chosen = index == selected;
		const char *const label = item_label(items[index]);
		const int x = (PICTURE_W - (int)strlen(label) * 8) / 2, y = 156 + index * (item_count > 7 ? 17 : 20);

		if (chosen)
		{
			put_glyph(out, x - 16, y, '>', GOLD, 1);
			put_glyph(out, x + ((int)strlen(label) + 1) * 8, y, '<', GOLD, 1);
		}
		put_centre_y(out, y, label, chosen ? GOLD : items[index] == ITEM_UPDATE ? CYAN : WHITE);
	}
	if (notice_frames > 0)
	{
		notice_frames--;
		put_centre(out, 33, notice, CYAN);
	}
	put_centre(out, 37, "A: CHOOSE", GREY);
#ifdef DDONPACH_VERSION
	{
		/* the version, small in the corner (CMake's project version) */
		char version[24];

		snprintf(version, sizeof(version), "V%s", DDONPACH_VERSION);
		put_text_y(out, COLUMNS - (int)strlen(version), PICTURE_H - 10, version, GREY);
	}
#endif
}

/* the update's page: the versions, and what is asked or how far it is */
static void draw_update(uint32_t *out)
{
	const int state = updater_state();
	char line[64];

	put_centre(out, PANEL_TOP + 1, "UPDATE", GOLD);
#ifdef DDONPACH_VERSION
	snprintf(line, sizeof(line), "V%s  >  V%s", DDONPACH_VERSION, updater_latest());
#else
	snprintf(line, sizeof(line), "V%s", updater_latest());
#endif
	put_centre(out, PANEL_TOP + 3, line, WHITE);
	if (state == UPDATER_AVAILABLE)
	{
		put_centre(out, PANEL_TOP + 6, "DOWNLOAD AND INSTALL IT?", WHITE);
		put_centre(out, PANEL_TOP + 8, "THE PROGRAM STARTS AGAIN", GREY);
		put_centre(out, PANEL_TOP + 10, "SETTINGS, SCORES, REPLAYS", GREY);
		put_centre(out, PANEL_TOP + 12, "AND THE ROM ARE KEPT", GREY);
		put_centre(out, PANEL_TOP + 14, "(NOTES: GITHUB RELEASES)", GREY);
	}
	else if (state == UPDATER_DOWNLOADING || state == UPDATER_READY)
	{
		const int percent = state == UPDATER_READY ? 100 : updater_progress();
		const int left = PANEL_LEFT * 8 + 24, right = (PANEL_RIGHT + 1) * 8 - 24, y = (PANEL_TOP + 9) * 8;

		snprintf(line, sizeof(line), state == UPDATER_READY ? "INSTALLING..." : "DOWNLOADING  %d%%", percent);
		put_centre(out, PANEL_TOP + 6, line, CYAN);
		fill(out, left, y, right, y + 8, 0x3C2028u);
		fill(out, left, y, left + (right - left) * percent / 100, y + 8, GOLD);
	}
	else if (state == UPDATER_FAILED)
	{
		/* (why, in capitals, broken into lines of the panel's width) */
		const char *why = updater_problem();
		int row = PANEL_TOP + 6;

		put_centre(out, row, "IT DID NOT WORK:", GOLD);
		for (row += 2; *why && row < PANEL_TOP + 15; row++)
		{
			size_t length = strlen(why), cut = length;

			if (length > 26)
			{
				for (cut = 26; cut > 0 && why[cut] != ' '; cut--)
					;
				if (cut == 0)
					cut = 26;
			}
			for (size_t at = 0; at < cut; at++)
				line[at] = (char)toupper((unsigned char)why[at]);
			line[cut] = '\0';
			put_centre(out, row, line, WHITE);
			why += cut;
			while (*why == ' ')
				why++;
		}
	}
}

void menu_draw(const uint32_t *game, uint32_t *out)
{
	if (training_pending > 0 && --training_pending == 0)
	{
		/* (the LOADING... frame is on the screen: the game run to the place now, a few seconds) */
		if (host.training())
		{
			front = false;
			close_menu();    /* (the game is at the place: it goes on from there) */
			memcpy(out, game, sizeof(uint32_t) * PICTURE_W * PICTURE_H);
			return;
		}
		training_failed = 240;
	}
	if (page == PAGE_FRONT)
	{
		draw_front(out);
		return;
	}
	const int left = PANEL_LEFT * 8 - 4, right = (PANEL_RIGHT + 1) * 8 + 4;
	const int top = PANEL_TOP * 8 - 4, bottom = (PANEL_TOP + PANEL_ROWS) * 8 + 4;
	int cheats_total = 0, cheats_shown = 0;

	for (int index = 0; index < PICTURE_W * PICTURE_H; index++)
		out[index] = shade(game[index], 30);
	fill(out, left, top, right, bottom, 0x1C0C10u);
	fill(out, left, top, right, top + 2, GOLD);
	fill(out, left, bottom - 2, right, bottom, GOLD);
	fill(out, left, top, left + 2, bottom, GOLD);
	fill(out, right - 2, top, right, bottom, GOLD);

	if (page == PAGE_MAIN)
		put_centre(out, PANEL_TOP + 1, front ? "SETTINGS" : "OPTIONS", GOLD);
	else if (page == PAGE_GRAPHICS)
		put_centre(out, PANEL_TOP + 1, "GRAPHICS", GOLD);
	else if (page == PAGE_GAME)
		put_centre(out, PANEL_TOP + 1, "GAME OPTIONS", GOLD);
	else if (page == PAGE_SOUND)
		put_centre(out, PANEL_TOP + 1, "SOUND", GOLD);
	else if (page == PAGE_TRAINING)
	{
		put_centre(out, PANEL_TOP + 1, "TRAINING", GOLD);
		if (training_pending > 0)
			put_centre(out, PANEL_TOP + 26, "LOADING...", CYAN);
		else if (training_failed > 0)
		{
			training_failed--;
			put_centre(out, PANEL_TOP + 26, "COULD NOT REACH IT", CYAN);
		}
		else
			put_centre(out, PANEL_TOP + 26, "NO HIGH SCORES IN TRAINING", GREY);
	}
	else if (page == PAGE_CHEATS)
	{
		put_centre(out, PANEL_TOP + 1, "CHEATS", GOLD);
		cheats_shown = cheats_first(&cheats_total);
		if (cheats_total == 0)
			put_centre(out, PANEL_TOP + 3, "NO CHEAT FILE", GREY);
		else if (cheats_total > CHEAT_ROWS)
		{
			char shown[32];

			snprintf(shown, sizeof(shown), "%d-%d OF %d", cheats_shown + 1, cheats_shown + CHEAT_ROWS, cheats_total);
			put_centre(out, PANEL_TOP + 2, shown, GREY);
		}
	}
	else if (page == PAGE_ACHIEVEMENTS || page == PAGE_STATS)
	{
		char title[32];

		if (page == PAGE_ACHIEVEMENTS)
			snprintf(title, sizeof(title), "ACHIEVEMENTS  %d/%d", achievements_done(), achievements_count());
		else
			snprintf(title, sizeof(title), "STATISTICS");
		put_centre(out, PANEL_TOP + 1, title, GOLD);
		cheats_shown = cheats_first(&cheats_total);
		if (page == PAGE_ACHIEVEMENTS && selected < achievements_count())
		{
			/* the chosen one: what it asks for, and when it was earned */
			const char *const date = achievements_date(selected);
			char line[48];

			put_centre(out, PANEL_TOP + 2, achievements_text(selected), CYAN);
			snprintf(line, sizeof(line), date ? "EARNED %s" : "NOT YET", date);
			put_centre(out, PANEL_TOP + 3, line, date ? GOLD : GREY);
		}
	}
	else if (page == PAGE_REPLAYS)
	{
		put_centre(out, PANEL_TOP + 1, "REPLAYS", GOLD);
		cheats_shown = cheats_first(&cheats_total);
		if (cheats_total == 0)
		{
			put_centre(out, PANEL_TOP + 3, "NO REPLAYS YET:", GREY);
			put_centre(out, PANEL_TOP + 5, "EACH GAME ASKS AT ITS END", GREY);
		}
		else if (cheats_total > REPLAY_ROWS)
		{
			char shown[32];

			snprintf(shown, sizeof(shown), "%d-%d OF %d", cheats_shown + 1, cheats_shown + REPLAY_ROWS, cheats_total);
			put_centre(out, PANEL_TOP + 2, shown, GREY);
		}
	}
	else if (page == PAGE_REPLAY_MENU || page == PAGE_DELETE)
	{
		/* the replay chosen: its date, ship, how far, its score and length */
		const replay_info *const r = &replays[replay_chosen];
		char line[48];

		put_centre(out, PANEL_TOP + 1, page == PAGE_DELETE ? "DELETE THIS REPLAY?" : "REPLAY", GOLD);
		put_centre(out, PANEL_TOP + 3, r->date, WHITE);
		snprintf(line, sizeof(line), "TYPE-%c %s", 'A' + r->ship, r->laser ? "LASER" : "SHOT");
		put_centre(out, PANEL_TOP + 5, line, WHITE);
		if (r->all_clear)
			snprintf(line, sizeof(line), "ALL CLEAR");
		else
			snprintf(line, sizeof(line), "REACHED %d-%d", r->stage / 6 + 1, r->stage % 6 + 1);
		put_centre(out, PANEL_TOP + 7, line, WHITE);
		snprintf(line, sizeof(line), "SCORE %lu0", (unsigned long)r->score);
		put_centre(out, PANEL_TOP + 9, line, GOLD);
		{
			const unsigned seconds = (unsigned)(r->frames * 17376ull / 1000000ull);

			snprintf(line, sizeof(line), "%u:%02u", seconds / 60u, seconds % 60u);
			put_centre(out, PANEL_TOP + 11, line, GREY);
		}
	}
	else if (page == PAGE_SAVE_REPLAY)
	{
		put_centre(out, PANEL_TOP + 1, "GAME OVER", GOLD);
		put_centre(out, PANEL_TOP + 4, "SAVE THIS GAME'S REPLAY?", WHITE);
		put_centre(out, PANEL_TOP + 6, "(MAIN MENU > REPLAYS)", GREY);
	}
	else if (page == PAGE_UPDATE)
		draw_update(out);
	else
	{
		char title[48];

		snprintf(title, sizeof(title), "CONTROLS  < PLAYER %d >", player + 1);
		put_centre(out, PANEL_TOP + 1, title, GOLD);
		put_centre(out, PANEL_TOP + 2, "KEY / GAMEPAD", GREY);
	}
	{
		/* the entries, in pixel lines: 16 apart, the controls page's 12 (18 of them), the game options' 11 (16 of them,
		   their note above the hints) */
		const bool listed = page == PAGE_CHEATS || page == PAGE_REPLAYS || page == PAGE_ACHIEVEMENTS || page == PAGE_STATS;
		const int step = page == PAGE_GAME ? 11 : page == PAGE_CONTROLS ? 12 : listed ? 13 : 16;
		int y = (PANEL_TOP + ((page == PAGE_CHEATS || page == PAGE_REPLAYS) && cheats_total == 0 ? 8
			: page == PAGE_REPLAY_MENU || page == PAGE_DELETE ? 14 : page == PAGE_SAVE_REPLAY ? 9
			: page == PAGE_UPDATE ? 19 : 4)) * 8;

		for (int index = 0; index < item_count; index++)
		{
			const int item = items[index];
			const bool chosen = index == selected;
			char value[48];

			if (listed && page != PAGE_CHEATS && index < cheats_total
					&& (index < cheats_shown || index >= cheats_shown + REPLAY_ROWS))
				continue;    /* (scrolled out of the list) */
			if (page == PAGE_CHEATS && index < cheats_total
					&& (index < cheats_shown || index >= cheats_shown + CHEAT_ROWS))
				continue;    /* (scrolled out of the list) */
			if ((item == ITEM_QUIT && !(index > 0 && items[index - 1] == ITEM_MAIN_MENU)) || item == ITEM_MAIN_MENU
					|| item == ITEM_BACK || item == ITEM_FAST_FORWARD_KEY || item == ITEM_DEADZONE
					|| item == ITEM_SCALE || item == ITEM_SAVE_STATE || item == ITEM_GAME
					|| item == ITEM_T_START || (item == ITEM_SOUND && index > 0 && items[index - 1] == ITEM_TRAINING_MENU))
				y += page == PAGE_CONTROLS ? 6 : 8;    /* (a gap before them) */
			if (chosen)
				put_char_y(out, PANEL_LEFT + 1, y, '>', GOLD);
			put_text_y(out, PANEL_LEFT + 3, y, item_label(item), chosen ? GOLD
				: item >= ITEM_STAT_LINE && !stats_value(item - ITEM_STAT_LINE) ? CYAN : WHITE);    /* (headings) */
			item_value(item, value, sizeof(value));
			if (chosen && capture >= 0)
				snprintf(value, sizeof(value), "PRESS...");
			put_right_y(out, PANEL_RIGHT - 1, y, value, chosen ? CYAN : GREY);
			y += step;
		}
		if (page == PAGE_GAME)
		{
			/* (what the entries need: the difficulty and the auto-fire are read by the game as it starts) */
			static const char *const note[2] = { "DIFFICULTY, AUTO FIRE:", "ON F3 RESET OR RESTART" };

			for (int line = 0; line < 2; line++)
				put_text_y(out, (COLUMNS - (int)strlen(note[line])) / 2, y + 6 + line * 10, note[line], GOLD);
		}
	}

	if (capture >= 0)
	{
		char seconds[40];

		snprintf(seconds, sizeof(seconds), "PRESS A KEY OR BUTTON  %d", capture_frames / 58 + 1);
		put_centre(out, PANEL_TOP + PANEL_ROWS - 3, seconds, CYAN);
		put_centre(out, PANEL_TOP + PANEL_ROWS - 2, "(ESC: NO CHANGE)", GREY);
	}
	else if (page == PAGE_MAIN)
		put_hints(out, "A: CHOOSE   <>: CHANGE", front ? "B: BACK" : "B / VIEW: CLOSE");
	else if (page == PAGE_CONTROLS)
		put_centre(out, PANEL_TOP + PANEL_ROWS - 1, "A:CHANGE <>:PLAYER B:BACK", GREY);
	else if (page == PAGE_UPDATE)
		put_hints(out, "A: CHOOSE", "B: BACK");
	else
		put_hints(out, "A: CHOOSE   <>: CHANGE", "B: BACK");
}
