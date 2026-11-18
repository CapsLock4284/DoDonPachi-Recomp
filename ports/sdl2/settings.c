/**
 *    Name:        settings.c
 *    Description: the window port's settings (settings.h): ddonpach.ini, read and written
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "settings.h"
#include "scaler.h"

#include "cave/machine.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const uint16_t settings_action_bits[ACTIONS] = {
	CAVE_UP, CAVE_DOWN, CAVE_LEFT, CAVE_RIGHT,
	CAVE_BUTTON1, CAVE_BUTTON2, CAVE_BUTTON3, CAVE_START, CAVE_COIN, CAVE_SERVICE
};

static const char *const action_names[ACTIONS] = {
	"up", "down", "left", "right", "shot", "bomb", "button_c", "start", "coin", "service"
};

const char *const settings_scaling_names[VIDEO_SCALINGS] = { "fit", "integer", "stretch" };
const char *const settings_scanline_names[4] = { "off", "light", "medium", "heavy" };
const char *const settings_crt_names[3] = { "off", "light", "full" };
const char *const settings_rotate_names[ROTATIONS] = { "none", "right", "left" };
const char *const settings_renderer_names[RENDERERS] = { "auto", "direct3d", "direct3d11", "direct3d12", "opengl",
	"software" };

static const char *const hotkey_names[HOTKEYS] = { "reset", "service_menu", "fast_forward", "save_state",
	"load_state", "state_slot_previous", "state_slot_next" };

static const char *const action_notes[ACTIONS] = {
	NULL, NULL, NULL, NULL,
	"shot; held, the laser",
	"bomb",
	"button C: the game's own auto-fire, with the test mode's \"C BUTTON FULL-AUTO\" ([game] c_button_full_auto)",
	"start",
	"coin",
	"the service switch: held 2 seconds at the title or in the attract mode, the test mode"
};

/* ---- the defaults ---- */

static void set_keys(settings *s, int player, int action, SDL_Scancode first, SDL_Scancode second)
{
	s->keys[player][action][0] = first;
	s->keys[player][action][1] = second;
}

static void set_button(settings *s, int player, int action, int kind, int index)
{
	s->buttons[player][action][0].kind = kind;
	s->buttons[player][action][0].index = index;
}

void settings_defaults(settings *s)
{
	static const SDL_Scancode keys[2][ACTIONS] = {
		{ SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_Z, SDL_SCANCODE_X,
		  SDL_SCANCODE_C, SDL_SCANCODE_1, SDL_SCANCODE_5, SDL_SCANCODE_F2 },
		{ SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_D, SDL_SCANCODE_G, SDL_SCANCODE_A, SDL_SCANCODE_S,
		  SDL_SCANCODE_Q, SDL_SCANCODE_2, SDL_SCANCODE_6, SDL_SCANCODE_UNKNOWN }
	};
	static const int buttons[ACTIONS] = {
		SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
		SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X,
		SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_INVALID, SDL_CONTROLLER_BUTTON_INVALID
	};

	memset(s, 0, sizeof(*s));
	s->version = 0;
	s->fast_forward_speed = 4;
	s->skip_warning = true;
	s->main_menu = true;
	s->save_high_scores = true;
	s->free_play = true;
	s->arcade_timing = false;
	s->c_button_full_auto = true;
	s->slowdown = true;
	s->scale = 3;
	s->fullscreen = false;
	s->exclusive = false;
	s->scaling = VIDEO_FIT;
	s->smooth = false;
	s->scanlines = 0;
	s->crt = 0;
	s->filter = SCALER_OFF;
	s->vsync = false;
	s->renderer = 0;
	s->show_fps = false;
	s->side_panels = true;
	s->rotate = ROTATE_NONE;
	s->training_power = 4;
	s->training_retry = true;
	s->sound = true;
	s->volume = 100;
	snprintf(s->rom, sizeof(s->rom), "rom");
	snprintf(s->cheats, sizeof(s->cheats), "cheats/ddonpach.xml");
	for (int player = 0; player < 2; player++)
	{
		for (int action = 0; action < ACTIONS; action++)
		{
			set_keys(s, player, action, keys[player][action], SDL_SCANCODE_UNKNOWN);
			if (buttons[action] != SDL_CONTROLLER_BUTTON_INVALID)
				set_button(s, player, action, PAD_BUTTON, buttons[action]);
		}
		s->left_stick[player] = true;
		s->stick_dead[player] = 40;
	}
	s->hot_keys[HOTKEY_RESET][0] = SDL_SCANCODE_F3;
	s->hot_keys[HOTKEY_SERVICE_MENU][0] = SDL_SCANCODE_F4;
	s->hot_keys[HOTKEY_FAST_FORWARD][0] = SDL_SCANCODE_TAB;
	s->hot_buttons[HOTKEY_FAST_FORWARD][0] = (pad_input){ PAD_BUTTON, SDL_CONTROLLER_BUTTON_LEFTSHOULDER };
	s->hot_keys[HOTKEY_SAVE_STATE][0] = SDL_SCANCODE_F5;
	s->hot_keys[HOTKEY_LOAD_STATE][0] = SDL_SCANCODE_F8;
	s->hot_keys[HOTKEY_SLOT_PREVIOUS][0] = SDL_SCANCODE_F6;
	s->hot_keys[HOTKEY_SLOT_NEXT][0] = SDL_SCANCODE_F7;
}

/* ---- reading ---- */

static char *trim(char *text)
{
	char *end;

	while (isspace((unsigned char)*text))
		text++;
	end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1]))
		*--end = '\0';
	return text;
}

static bool parse_bool(const char *value, bool *out)
{
	static const char *const yes[] = { "true", "yes", "on", "1" }, *const no[] = { "false", "no", "off", "0" };

	for (int index = 0; index < 4; index++)
	{
		if (!SDL_strcasecmp(value, yes[index]))
		{
			*out = true;
			return true;
		}
		if (!SDL_strcasecmp(value, no[index]))
		{
			*out = false;
			return true;
		}
	}
	return false;
}

static bool parse_int(const char *value, int low, int high, int *out)
{
	char *end;
	const long number = strtol(value, &end, 10);

	if (end == value || *trim(end) || number < low || number > high)
		return false;
	*out = (int)number;
	return true;
}

/* "Z, Left Shift" or "x, righttrigger": the names one after the other into 'names', at most BINDINGS_MAX */
static int split_names(char *value, char *names[BINDINGS_MAX])
{
	int count = 0;

	if (!*value || !SDL_strcasecmp(value, "none"))
		return 0;
	for (char *name = value; name && count < BINDINGS_MAX;)
	{
		char *const comma = strchr(name, ',');

		if (comma)
			*comma = '\0';
		names[count++] = trim(name);
		name = comma ? comma + 1 : NULL;
	}
	return count;
}

static bool parse_keys(char *value, SDL_Scancode out[BINDINGS_MAX])
{
	char *names[BINDINGS_MAX];
	SDL_Scancode keys[BINDINGS_MAX] = { SDL_SCANCODE_UNKNOWN };
	const int count = split_names(value, names);

	for (int index = 0; index < count; index++)
		if ((keys[index] = SDL_GetScancodeFromName(names[index])) == SDL_SCANCODE_UNKNOWN)
			return false;
	memcpy(out, keys, sizeof(keys));
	return true;
}

static bool parse_buttons(char *value, pad_input out[BINDINGS_MAX])
{
	char *names[BINDINGS_MAX];
	pad_input inputs[BINDINGS_MAX] = { { PAD_NONE, 0 } };
	const int count = split_names(value, names);

	for (int index = 0; index < count; index++)
	{
		const SDL_GameControllerButton button = SDL_GameControllerGetButtonFromString(names[index]);
		const SDL_GameControllerAxis axis = SDL_GameControllerGetAxisFromString(names[index]);

		if (button != SDL_CONTROLLER_BUTTON_INVALID)
			inputs[index] = (pad_input){ PAD_BUTTON, button };
		else if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
			inputs[index] = (pad_input){ PAD_TRIGGER, axis };
		else
			return false;
	}
	memcpy(out, inputs, sizeof(inputs));
	return true;
}

/* one of 'names' (any case): its index, or -1 */
static int name_of(const char *value, const char *const *names, int count)
{
	for (int index = 0; index < count; index++)
		if (!SDL_strcasecmp(value, names[index]))
			return index;
	return -1;
}

static int action_of(const char *name)
{
	for (int action = 0; action < ACTIONS; action++)
		if (!SDL_strcasecmp(name, action_names[action]))
			return action;
	return -1;
}

/* one "name = value" of section 'section': true when it is a setting and its value is right */
static bool take(settings *s, const char *section, const char *name, char *value)
{
	int player, number;

	if (!SDL_strcasecmp(section, "game"))
	{
		if (!SDL_strcasecmp(name, "version"))
		{
			if (!SDL_strcasecmp(value, "world"))
				s->version = 0;
			else if (!SDL_strcasecmp(value, "japan"))
				s->version = 1;
			else
				return false;
			return true;
		}
		if (!SDL_strcasecmp(name, "fast_forward_speed"))
			return parse_int(value, 2, 16, &s->fast_forward_speed);
		if (!SDL_strcasecmp(name, "skip_warning"))
			return parse_bool(value, &s->skip_warning);
		if (!SDL_strcasecmp(name, "main_menu"))
			return parse_bool(value, &s->main_menu);
		if (!SDL_strcasecmp(name, "save_high_scores"))
			return parse_bool(value, &s->save_high_scores);
		if (!SDL_strcasecmp(name, "free_play"))
			return parse_bool(value, &s->free_play);
		if (!SDL_strcasecmp(name, "arcade_timing"))
			return parse_bool(value, &s->arcade_timing);
		if (!SDL_strcasecmp(name, "c_button_full_auto"))
			return parse_bool(value, &s->c_button_full_auto);
		if (!SDL_strcasecmp(name, "slowdown"))
			return parse_bool(value, &s->slowdown);
		return false;
	}
	if (!SDL_strcasecmp(section, "window"))
	{
		if (!SDL_strcasecmp(name, "scale"))
			return parse_int(value, 1, 16, &s->scale);
		if (!SDL_strcasecmp(name, "fullscreen"))
			return parse_bool(value, &s->fullscreen);
		if (!SDL_strcasecmp(name, "fullscreen_type"))
		{
			if (!SDL_strcasecmp(value, "borderless") || !SDL_strcasecmp(value, "exclusive"))
			{
				s->exclusive = !SDL_strcasecmp(value, "exclusive");
				return true;
			}
			return false;
		}
		return false;
	}
	if (!SDL_strcasecmp(section, "video"))
	{
		if (!SDL_strcasecmp(name, "scaling"))
			return (s->scaling = name_of(value, settings_scaling_names, VIDEO_SCALINGS)) >= 0
				|| (s->scaling = VIDEO_FIT, false);
		if (!SDL_strcasecmp(name, "smooth"))
			return parse_bool(value, &s->smooth);
		if (!SDL_strcasecmp(name, "scanlines"))
			return (s->scanlines = name_of(value, settings_scanline_names, 4)) >= 0 || (s->scanlines = 0, false);
		if (!SDL_strcasecmp(name, "filter"))
		{
			for (int f = 0; f < SCALERS; f++)
				if (!SDL_strcasecmp(value, scaler_key(f)))
				{
					s->filter = f;
					return true;
				}
			s->filter = SCALER_OFF;
			return false;
		}
		if (!SDL_strcasecmp(name, "crt"))
			return (s->crt = name_of(value, settings_crt_names, 3)) >= 0 || (s->crt = 0, false);
		if (!SDL_strcasecmp(name, "vsync"))
			return parse_bool(value, &s->vsync);
		if (!SDL_strcasecmp(name, "renderer"))
			return (s->renderer = name_of(value, settings_renderer_names, RENDERERS)) >= 0 || (s->renderer = 0, false);
		if (!SDL_strcasecmp(name, "show_fps"))
			return parse_bool(value, &s->show_fps);
		if (!SDL_strcasecmp(name, "side_panels"))
			return parse_bool(value, &s->side_panels);
		if (!SDL_strcasecmp(name, "rotate"))
			return (s->rotate = name_of(value, settings_rotate_names, ROTATIONS)) >= 0
				|| (s->rotate = ROTATE_NONE, false);
		return false;
	}
	if (!SDL_strcasecmp(section, "sound"))
	{
		if (!SDL_strcasecmp(name, "enabled"))
			return parse_bool(value, &s->sound);
		if (!SDL_strcasecmp(name, "volume"))
			return parse_int(value, 0, 100, &s->volume);
		return false;
	}
	if (!SDL_strcasecmp(section, "training"))
	{
		if (!SDL_strcasecmp(name, "stage"))
			return parse_int(value, 0, 11, &s->training_stage);
		if (!SDL_strcasecmp(name, "place"))
		{
			if (!SDL_strcasecmp(value, "start") || !SDL_strcasecmp(value, "boss"))
			{
				s->training_point = !SDL_strcasecmp(value, "boss");
				return true;
			}
			return false;
		}
		if (!SDL_strcasecmp(name, "ship"))
			return parse_int(value, 0, 2, &s->training_ship);
		if (!SDL_strcasecmp(name, "style"))
		{
			if (!SDL_strcasecmp(value, "shot") || !SDL_strcasecmp(value, "laser"))
			{
				s->training_laser = !SDL_strcasecmp(value, "laser");
				return true;
			}
			return false;
		}
		if (!SDL_strcasecmp(name, "power"))
			return parse_int(value, 0, 4, &s->training_power);
		if (!SDL_strcasecmp(name, "max_bombs"))
			return parse_bool(value, &s->training_max_bombs);
		if (!SDL_strcasecmp(name, "retry_on_miss"))
			return parse_bool(value, &s->training_retry);
		return false;
	}
	if (!SDL_strcasecmp(section, "hotkeys"))
	{
		for (int hotkey = 0; hotkey < HOTKEYS; hotkey++)
		{
			char gamepad[64];

			snprintf(gamepad, sizeof(gamepad), "%s_gamepad", hotkey_names[hotkey]);
			if (!SDL_strcasecmp(name, hotkey_names[hotkey]))
				return parse_keys(value, s->hot_keys[hotkey]);
			if (!SDL_strcasecmp(name, gamepad))
				return parse_buttons(value, s->hot_buttons[hotkey]);
		}
		return false;
	}
	if (!SDL_strcasecmp(section, "files"))
	{
		char *const target = !SDL_strcasecmp(name, "rom") ? s->rom : !SDL_strcasecmp(name, "eeprom") ? s->eeprom
			: !SDL_strcasecmp(name, "cheats") ? s->cheats : NULL;

		if (!target || strlen(value) >= sizeof(s->rom))
			return false;
		strcpy(target, value);
		return true;
	}
	if (!SDL_strncasecmp(section, "player", 6) && (section[6] == '1' || section[6] == '2') && section[7] == '.')
	{
		const int action = action_of(name);

		player = section[6] - '1';
		if (!SDL_strcasecmp(section + 8, "keyboard"))
			return action >= 0 && parse_keys(value, s->keys[player][action]);
		if (!SDL_strcasecmp(section + 8, "gamepad"))
		{
			if (!SDL_strcasecmp(name, "left_stick"))
				return parse_bool(value, &s->left_stick[player]);
			if (!SDL_strcasecmp(name, "stick_deadzone"))
			{
				if (!parse_int(value, 5, 95, &number))
					return false;
				s->stick_dead[player] = number;
				return true;
			}
			return action >= 0 && parse_buttons(value, s->buttons[player][action]);
		}
	}
	return false;
}

const char *settings_load(settings *s, const char *path, bool *written)
{
	static char problems[1024];
	char line[1024], section[64] = "";
	int number = 0;
	FILE *file = fopen(path, "r");

	*written = false;
	problems[0] = '\0';
	if (!file)
	{
		const char *const problem = settings_save(s, path);

		*written = problem == NULL;
		return problem;
	}
	while (fgets(line, sizeof(line), file))
	{
		char *text = trim(line), *equals;

		number++;
		if (!*text || *text == ';' || *text == '#')
			continue;
		if (*text == '[')
		{
			char *const close = strchr(text, ']');

			if (close && close - text - 1 < (int)sizeof(section))
			{
				*close = '\0';
				snprintf(section, sizeof(section), "%s", trim(text + 1));
				continue;
			}
		}
		else if ((equals = strchr(text, '=')) != NULL)
		{
			*equals = '\0';
			if (take(s, section, trim(text), trim(equals + 1)))
				continue;
		}
		if (strlen(problems) < sizeof(problems) - 80)
			snprintf(problems + strlen(problems), sizeof(problems) - strlen(problems),
				"%sline %d is not a setting this version knows, or its value is wrong", problems[0] ? "; " : "", number);
	}
	fclose(file);
	return problems[0] ? problems : NULL;
}

/* ---- writing ---- */

static const char *bool_text(bool value)
{
	return value ? "true" : "false";
}

static void write_keys(FILE *file, const SDL_Scancode keys[BINDINGS_MAX])
{
	int written = 0;

	for (int index = 0; index < BINDINGS_MAX; index++)
		if (keys[index] != SDL_SCANCODE_UNKNOWN)
			fprintf(file, "%s%s", written++ ? ", " : "", SDL_GetScancodeName(keys[index]));
	if (!written)
		fputs("none", file);
}

static void write_buttons(FILE *file, const pad_input inputs[BINDINGS_MAX])
{
	int written = 0;

	for (int index = 0; index < BINDINGS_MAX; index++)
	{
		const char *name = NULL;

		if (inputs[index].kind == PAD_BUTTON)
			name = SDL_GameControllerGetStringForButton((SDL_GameControllerButton)inputs[index].index);
		else if (inputs[index].kind == PAD_TRIGGER)
			name = SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)inputs[index].index);
		if (name)
			fprintf(file, "%s%s", written++ ? ", " : "", name);
	}
	if (!written)
		fputs("none", file);
}

const char *settings_save(const settings *s, const char *path)
{
	FILE *file = fopen(path, "w");

	if (!file)
		return "cannot write the settings file";
	fputs("; DoDonPachi's settings. Edit with any text editor while the game is closed; a line taken out goes back to\n"
		"; its default, and deleting the file writes it again with all the defaults. The command line's options\n"
		"; (--rom, --scale, --no-sound, --eeprom, --cheats) go over these.\n\n", file);

	fputs("[game]\n", file);
	fputs("; the game's version: world, or japan (not yet: the Japanese program is still to be translated)\n", file);
	fprintf(file, "version = %s\n", s->version == 1 ? "japan" : "world");
	fputs("; while the fast forward key or button is held ([hotkeys]): the game's frames a frame, 2 to 16\n", file);
	fprintf(file, "fast_forward_speed = %d\n", s->fast_forward_speed);
	fputs("; true (the default): the WARNING screen the game shows as it starts (31 seconds) is run at once, unseen\n"
		"; (the game runs every frame of it, as on the board); false: shown\n", file);
	fprintf(file, "skip_warning = %s\n", bool_text(s->skip_warning));
	fputs("; true (the default): the main menu as the program starts (START GAME, SETTINGS, QUIT TO DESKTOP); false:\n"
		"; straight into the game, as the board switched on\n", file);
	fprintf(file, "main_menu = %s\n", bool_text(s->main_menu));
	fputs("; true (the default): the high score table kept in ddonpach.hiscore beside the program (the arcade board\n"
		"; forgets it when it is switched off); not kept from a game that had a cheat on or went back to a save state\n",
		file);
	fprintf(file, "save_high_scores = %s\n", bool_text(s->save_high_scores));
	fputs("; true (the default): Start drops in the coins a game needs first (the game has no free play of its own;\n"
		"; its coin sound and credit as on the board); false: coins with the coin key\n", file);
	fprintf(file, "free_play = %s\n", bool_text(s->free_play));
	fputs("; true: the arcade's input latency exactly (a press shows two frames after the game reads it, as on the\n"
		"; board); false (the default): one frame less (the look-ahead). The game runs the same either way\n", file);
	fprintf(file, "arcade_timing = %s\n", bool_text(s->arcade_timing));
	fputs("; true: button C is the game's own auto-fire (the test mode's \"C BUTTON FULL-AUTO\", set in the EEPROM\n"
		"; as the game starts); false: as the test mode has it\n", file);
	fprintf(file, "c_button_full_auto = %s\n", bool_text(s->c_button_full_auto));
	fputs("; the arcade's slowdown: when the screen is busy the board's 68000 can't finish a frame in time and the game\n"
		"; runs slower, as on the real board (true); false: always full speed\n", file);
	fprintf(file, "slowdown = %s\n\n", bool_text(s->slowdown));

	fputs("[window]\n", file);
	fputs("; the window's size, in times the game's 240 x 320\n", file);
	fprintf(file, "scale = %d\n", s->scale);
	fputs("; start in full screen (F11 or Alt+Enter switches while playing)\n", file);
	fprintf(file, "fullscreen = %s\n", bool_text(s->fullscreen));
	fputs("; borderless (a window over the whole screen, at the desktop's resolution: quick to switch away from)\n", file);
	fputs("; or exclusive (the display the game's alone, at the desktop's resolution: may lag a little less)\n", file);
	fprintf(file, "fullscreen_type = %s\n\n", s->exclusive ? "exclusive" : "borderless");

	fputs("[video]\n", file);
	fputs("; fit: as large as the window allows; integer: whole times the game's pixels only (all the same size);\n"
		"; stretch: the whole window, out of shape\n", file);
	fprintf(file, "scaling = %s\n", settings_scaling_names[s->scaling]);
	fputs("; a soft filter instead of sharp pixels\n", file);
	fprintf(file, "smooth = %s\n", bool_text(s->smooth));
	fputs("; off, light, medium or heavy: the arcade monitor's lines (it stands on its side in the cabinet, so they run\n"
		"; up and down the picture)\n", file);
	fprintf(file, "scanlines = %s\n", settings_scanline_names[s->scanlines]);
	fputs("; off, light or full: the arcade monitor's look, a curved tube with its lines, phosphor stripes and glow (it\n"
		"; has its own lines: the scanlines setting is left aside while it is on)\n", file);
	fprintf(file, "crt = %s\n", settings_crt_names[s->crt]);
	fputs("; off, scale2x, scale3x, xbr2x, xbr3x, xbr4x or mmpx: a pixel-art scaler, smoothing the picture's edges while\n"
		"; keeping it sharp (not with the CRT look)\n", file);
	fprintf(file, "filter = %s\n", scaler_key(s->filter));
	fputs("; true: no tearing, the picture waits for the monitor (a little more delay); false: the least delay\n", file);
	fprintf(file, "vsync = %s\n", bool_text(s->vsync));
	fputs("; auto, direct3d (Direct3D 9), direct3d11, direct3d12, opengl or software: how the picture reaches the\n"
		"; screen (the FPS log names the one in use; one the PC lacks falls back to another). auto: direct3d12 on\n"
		"; Windows, the quickest to show (about 1.5 ms after the frame is done; direct3d in a borderless window waits\n"
		"; for Windows' compositor, about 27 ms). Used from the next start\n", file);
	fprintf(file, "renderer = %s\n", settings_renderer_names[s->renderer]);
	fputs("; the FPS meter (F9 too): GAME the game's own frames a second (fewer in the arcade's slowdown), BOARD the\n"
		"; board's frames run a second (57.55), WINDOW the pictures shown, LATE the frames the PC ran late in that\n"
		"; second, WORK the milliseconds a frame takes the PC\n", file);
	fprintf(file, "show_fps = %s\n", bool_text(s->show_fps));
	fputs("; the panels beside the picture on a wide screen: the ship's power and bombs, the board's speed and slowdown\n"
		"; (left); the score chasing the high score table, its graph, this stage against its best (right). As wide as\n"
		"; the screen leaves room for; the game's picture is never changed by them. Not with the picture rotated\n", file);
	fprintf(file, "side_panels = %s\n", bool_text(s->side_panels));
	fputs("; none, right or left: the picture turned for a monitor on its side (TATE)\n", file);
	fprintf(file, "rotate = %s\n", settings_rotate_names[s->rotate]);
	fputc('\n', file);

	fputs("[sound]\n", file);
	fprintf(file, "enabled = %s\n", bool_text(s->sound));
	fputs("; 0 to 100\n", file);
	fprintf(file, "volume = %d\n\n", s->volume);

	fputs("[training]\n", file);
	fputs("; the main menu's TRAINING: the stage, 0-11 (1-1 ... 1-6, 2-1 ... 2-6), and the place in it (start or boss)\n",
		file);
	fprintf(file, "stage = %d\n", s->training_stage);
	fprintf(file, "place = %s\n", s->training_point ? "boss" : "start");
	fputs("; the ship, 0-2 (types A, B, C), and its style (shot or laser)\n", file);
	fprintf(file, "ship = %d\n", s->training_ship);
	fprintf(file, "style = %s\n", s->training_laser ? "laser" : "shot");
	fputs("; the power, 0-4 (levels 1-4, 4 MAX: the shot and the laser both); max_bombs: 6 bombs, else 3\n", file);
	fprintf(file, "power = %d\n", s->training_power);
	fprintf(file, "max_bombs = %s\n", bool_text(s->training_max_bombs));
	fputs("; a miss: back to the place at once\n", file);
	fprintf(file, "retry_on_miss = %s\n\n", bool_text(s->training_retry));

	fputs("[hotkeys]\n", file);
	fputs("; the board's reset (keys, then the gamepad's buttons, either player's)\n", file);
	fputs("reset = ", file);
	write_keys(file, s->hot_keys[HOTKEY_RESET]);
	fputs("\nreset_gamepad = ", file);
	write_buttons(file, s->hot_buttons[HOTKEY_RESET]);
	fputs("\n; the test mode: the service switch held for you until it comes up (at the title or in the attract mode)\n", file);
	fputs("service_menu = ", file);
	write_keys(file, s->hot_keys[HOTKEY_SERVICE_MENU]);
	fputs("\nservice_menu_gamepad = ", file);
	write_buttons(file, s->hot_buttons[HOTKEY_SERVICE_MENU]);
	fputs("\n; held: fast forward ([game] fast_forward_speed), no sound meanwhile\n", file);
	fputs("fast_forward = ", file);
	write_keys(file, s->hot_keys[HOTKEY_FAST_FORWARD]);
	fputs("\nfast_forward_gamepad = ", file);
	write_buttons(file, s->hot_buttons[HOTKEY_FAST_FORWARD]);
	fputs("\n; a save state: the game as it is now, kept in the chosen slot until the program closes (5 slots; saving\n"
		"; again replaces the slot's)\n", file);
	fputs("save_state = ", file);
	write_keys(file, s->hot_keys[HOTKEY_SAVE_STATE]);
	fputs("\nsave_state_gamepad = ", file);
	write_buttons(file, s->hot_buttons[HOTKEY_SAVE_STATE]);
	fputs("\n; back to the chosen slot's save state\n", file);
	fputs("load_state = ", file);
	write_keys(file, s->hot_keys[HOTKEY_LOAD_STATE]);
	fputs("\nload_state_gamepad = ", file);
	write_buttons(file, s->hot_buttons[HOTKEY_LOAD_STATE]);
	fputs("\n; the save states' slot: the one before, the one after\n", file);
	fputs("state_slot_previous = ", file);
	write_keys(file, s->hot_keys[HOTKEY_SLOT_PREVIOUS]);
	fputs("\nstate_slot_previous_gamepad = ", file);
	write_buttons(file, s->hot_buttons[HOTKEY_SLOT_PREVIOUS]);
	fputs("\nstate_slot_next = ", file);
	write_keys(file, s->hot_keys[HOTKEY_SLOT_NEXT]);
	fputs("\nstate_slot_next_gamepad = ", file);
	write_buttons(file, s->hot_buttons[HOTKEY_SLOT_NEXT]);
	fputs("\n\n", file);

	fputs("[files]\n", file);
	fputs("; the ROM set: MAME's ddonpach.zip, or the folder that holds it (from the folder the game is started in).\n"
		"; Read on the first run only: the game's data is kept in ddonpach.dat beside the program from then on\n", file);
	fprintf(file, "rom = %s\n", s->rom);
	fputs("; the test mode's settings (empty: ddonpach.eeprom beside the program)\n", file);
	fprintf(file, "eeprom = %s\n", s->eeprom);
	fputs("; MAME's cheat file, for --cheat\n", file);
	fprintf(file, "cheats = %s\n", s->cheats);

	for (int player = 0; player < 2; player++)
	{
		fprintf(file, "\n[player%d.keyboard]\n", player + 1);
		if (player == 0)
			fputs("; SDL's key names: Up, Down, Left, Right, A-Z, 0-9, F1-F12, Space, Return, Left Shift, Right Ctrl,\n"
				"; Keypad 8 ... Up to 4 keys an action, with commas; none for no key\n", file);
		for (int action = 0; action < ACTIONS; action++)
		{
			if (player == 0 && action_notes[action])
				fprintf(file, "; %s\n", action_notes[action]);
			fprintf(file, "%s = ", action_names[action]);
			write_keys(file, s->keys[player][action]);
			fputc('\n', file);
		}
		fprintf(file, "\n[player%d.gamepad]\n", player + 1);
		if (player == 0)
			fputs("; The first gamepad plugged in is player 1's, the second player 2's. Buttons as on an Xbox pad:\n"
				"; a, b, x, y, back (View), start, guide, leftshoulder, rightshoulder, lefttrigger, righttrigger,\n"
				"; leftstick, rightstick, dpup, dpdown, dpleft, dpright. Up to 4 an action, with commas; none for none.\n"
				"; (leftshoulder is fast forward, in [hotkeys])\n", file);
		for (int action = 0; action < ACTIONS; action++)
		{
			fprintf(file, "%s = ", action_names[action]);
			write_buttons(file, s->buttons[player][action]);
			fputc('\n', file);
		}
		if (player == 0)
			fputs("; the left stick moves as well as the d-pad, past stick_deadzone % of its travel\n", file);
		fprintf(file, "left_stick = %s\n", bool_text(s->left_stick[player]));
		fprintf(file, "stick_deadzone = %d\n", s->stick_dead[player]);
	}
	if (fclose(file) != 0)
		return "cannot write the settings file";
	return NULL;
}

bool settings_pad_pressed(SDL_GameController *pad, pad_input input)
{
	if (input.kind == PAD_BUTTON)
		return SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)input.index) != 0;
	if (input.kind == PAD_TRIGGER)
		return SDL_GameControllerGetAxis(pad, (SDL_GameControllerAxis)input.index) > 16384;
	return false;
}
