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
#include "hiscore.h"
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
#define CHEAT_ROWS   16                 /* the cheats page's: as many shown at a time, scrolled with the choice */

enum { PAGE_MAIN, PAGE_CONTROLS, PAGE_GRAPHICS, PAGE_GAME, PAGE_CHEATS, PAGE_FRONT };
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
	ITEM_FAST_FORWARD_KEY = 90,   /* the hotkeys (both players': [hotkeys]): fast forward, save state, load state */
	ITEM_SAVE_STATE_KEY, ITEM_LOAD_STATE_KEY, ITEM_SLOT_PREVIOUS_KEY, ITEM_SLOT_NEXT_KEY,
	ITEM_ACTION = 100,            /* + an action: a player's key and button for it */
	ITEM_CHEAT = 200              /* + a cheat of the cheat file (cave/cheats.h) */
};

static const char *const item_labels[] = {
	"RESUME", "RESET", "TEST MODE", "FAST FWD SPEED", "VOLUME", "WINDOW SIZE", "FULL SCREEN", "CONTROLS",
	"QUIT TO DESKTOP", "RESET TO DEFAULTS", "BACK", "GRAPHICS", "SCALING", "SMOOTHING", "SCANLINES", "VSYNC",
	"ROTATE", "STICK DEADZONE", "SAVE STATE", "LOAD STATE", "GAME OPTIONS", "CHEATS", "SLOWDOWN", "CRT",
	"STATE SLOT", "FILTER", "C BUTTON AUTO FIRE", "INPUT TIMING", "FREE PLAY", "SAVE HIGH SCORES", "SKIP WARNING",
	"FPS METER", "RENDERER", "SIDE PANELS", "START GAME", "SETTINGS", "MAIN MENU", "MAIN MENU AT START"
};

static const char *const action_labels[ACTIONS] = {
	"UP", "DOWN", "LEFT", "RIGHT", "SHOT", "BOMB", "AUTO FIRE", "START", "COIN", "SERVICE"
};

static menu_host host;
static bool open, just_closed;
static bool front;                      /* the main menu is up, or its SETTINGS (the game not started) */
static int page, selected, player;
static int items[128], item_count;
static int capture = -1;                /* the action waiting for its key or button, or -1 */
static int capture_frames;
static int held_direction, held_frames; /* the gamepads' d-pad and stick, for their repeats */

static bool is_action(int item)
{
	return item >= ITEM_ACTION && item < ITEM_ACTION + ACTIONS;
}

static bool is_cheat(int item)
{
	return item >= ITEM_CHEAT;
}

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
		static const int front_items[] = { ITEM_START_GAME, ITEM_SETTINGS, ITEM_QUIT };

		add_items(front_items, sizeof(front_items) / sizeof(front_items[0]));
	}
	else if (page == PAGE_MAIN && front)
	{
		/* (the main menu's SETTINGS: what can be set before a game) */
		static const int settings_items[] = { ITEM_VOLUME, ITEM_GAME, ITEM_GRAPHICS, ITEM_CONTROLS, ITEM_BACK };

		add_items(settings_items, sizeof(settings_items) / sizeof(settings_items[0]));
	}
	else if (page == PAGE_MAIN)
	{
		static const int main_items[] = { ITEM_RESUME, ITEM_RESET, ITEM_VOLUME, ITEM_SAVE_STATE, ITEM_LOAD_STATE,
			ITEM_STATE_SLOT, ITEM_GAME, ITEM_GRAPHICS, ITEM_CONTROLS, ITEM_MAIN_MENU, ITEM_QUIT };

		add_items(main_items, sizeof(main_items) / sizeof(main_items[0]));
	}
	else if (page == PAGE_GAME)
	{
		static const int game_items[] = { ITEM_TIMING, ITEM_FREE_PLAY, ITEM_HIGH_SCORES, ITEM_SKIP_WARNING,
			ITEM_FRONT_AT_START, ITEM_SLOWDOWN, ITEM_AUTO_FIRE, ITEM_FAST_SPEED, ITEM_TEST_MODE, ITEM_CHEATS, ITEM_BACK };

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
		in_use->volume = wrap && in_use->volume >= 100 ? 0 : clamp(in_use->volume + step * 10, 0, 100);
		file->volume = in_use->volume;
		break;
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
static void back(void)
{
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
		: page == PAGE_CHEATS ? ITEM_CHEATS : ITEM_CONTROLS;

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
		if (page != PAGE_MAIN || front)
			back();
		else
			close_menu();
		break;
	case NAV_CLOSE:
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

/* ---- the entries' names and values ---- */

static const char *item_label(int item)
{
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
	return is_action(item) ? action_labels[item - ITEM_ACTION] : item_labels[item];
}

static void item_value(int item, char *out, size_t size)
{
	const settings *const s = host.options;

	out[0] = '\0';
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
	case ITEM_FAST_SPEED:
		snprintf(out, size, "%dX", s->fast_forward_speed);
		break;
	case ITEM_VOLUME:
		snprintf(out, size, "%d%%", s->volume);
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

/* the first of the cheats shown (the list scrolled to keep the choice in it), and how many there are */
static int cheats_first(int *total)
{
	const int cheats = item_count - 1, chosen = selected < cheats ? selected : cheats - 1;
	int first = chosen - CHEAT_ROWS / 2;

	if (first > cheats - CHEAT_ROWS)
		first = cheats - CHEAT_ROWS;
	if (first < 0)
		first = 0;
	*total = cheats;
	return first;
}

/* ---- the main menu: the title over a dark sky, its entries ---- */

/* the bee (the game's own sprite, from the player's ROM set: a frame of its flight, 32 x 32) at (x, y), turned as the
   picture is; its colours made here (the game's palette is not set up before it runs): gold, brighter for its
   lower pens, as the bee's own palette goes */
#define BEE_CODE 0x4496u

static void put_bee(uint32_t *out, int x, int y)
{
	if (!rom_set.sprites)
		return;
	for (int row = 0; row < 32; row++)
		for (int column = 0; column < 32; column++)
		{
			const uint32_t pixel = (BEE_CODE << 8) + (uint32_t)((31 - row) * 32 + column);
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
		const int x = (PICTURE_W - (int)strlen(label) * 8) / 2, y = 156 + index * 20;

		if (chosen)
		{
			put_glyph(out, x - 16, y, '>', GOLD, 1);
			put_glyph(out, x + ((int)strlen(label) + 1) * 8, y, '<', GOLD, 1);
		}
		put_centre_y(out, y, label, chosen ? GOLD : WHITE);
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

void menu_draw(const uint32_t *game, uint32_t *out)
{
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
	else
	{
		char title[48];

		snprintf(title, sizeof(title), "CONTROLS  < PLAYER %d >", player + 1);
		put_centre(out, PANEL_TOP + 1, title, GOLD);
		put_centre(out, PANEL_TOP + 2, "KEY / GAMEPAD", GREY);
	}
	{
		/* the entries, in pixel lines: 16 apart, the controls page's 12 (it has 18 of them) */
		const int step = page == PAGE_CONTROLS ? 12 : page == PAGE_CHEATS ? 13 : 16;
		int y = (PANEL_TOP + (page == PAGE_CHEATS && cheats_total == 0 ? 6 : 4)) * 8;

		for (int index = 0; index < item_count; index++)
		{
			const int item = items[index];
			const bool chosen = index == selected;
			char value[48];

			if (page == PAGE_CHEATS && index < cheats_total
					&& (index < cheats_shown || index >= cheats_shown + CHEAT_ROWS))
				continue;    /* (scrolled out of the list) */
			if ((item == ITEM_QUIT && !(index > 0 && items[index - 1] == ITEM_MAIN_MENU)) || item == ITEM_MAIN_MENU
					|| item == ITEM_BACK || item == ITEM_FAST_FORWARD_KEY || item == ITEM_DEADZONE
					|| item == ITEM_SCALE || item == ITEM_SAVE_STATE || item == ITEM_GAME || item == ITEM_CHEATS)
				y += page == PAGE_CONTROLS ? 6 : 8;    /* (a gap before them) */
			if (chosen)
				put_char_y(out, PANEL_LEFT + 1, y, '>', GOLD);
			put_text_y(out, PANEL_LEFT + 3, y, item_label(item), chosen ? GOLD : WHITE);
			item_value(item, value, sizeof(value));
			if (chosen && capture >= 0)
				snprintf(value, sizeof(value), "PRESS...");
			put_right_y(out, PANEL_RIGHT - 1, y, value, chosen ? CYAN : GREY);
			y += step;
		}
		if (page == PAGE_GAME)
		{
			/* (what the chosen entry needs: the auto-fire is read by the game as it starts) */
			static const char *const note[2] = { "AUTO FIRE CHANGES ON", "F3 RESET OR RESTART" };

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
	else
		put_hints(out, "A: CHOOSE   <>: CHANGE", "B: BACK");
}
