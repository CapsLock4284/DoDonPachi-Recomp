/**
 *    Name:        main.c
 *    Description: DoDonPachi in a window, on SDL2: the translated program, its picture, its sound, the keys and
 *                 gamepads
 *    Version:     0.1.0 (draft)
 *    Created:     16-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's window port (ports/sdl2/main.c), cut down to what does not depend on that game's memory: the
 *    window and its video settings (scaling, the CRT look, the pixel-art scalers, rotation), the sound, the controls,
 *    the EEPROM, the save states, fast forward, free play, the high scores, MAME's cheat files, and the options menu
 *    (menu.h: in the game's own font, its options, game options, graphics, controls and cheats pages). Not yet: the
 *    main menu, training, replays, achievements, statistics, the side panels, the hitbox.
 *
 *    Usage: ddonpach [--settings PATH] [--rom PATH] [--scale N] [--no-sound] [--eeprom PATH] [--cheats FILE]
 *                    [--cheat NAME ...] [--list-cheats]
 *      --settings PATH the settings file (default: ddonpach.ini beside the program (paths.h); settings.h): the options
 *                      below and the keys and gamepad buttons of both players. Written with the defaults when it is
 *                      not there; the options below go over it for the run
 *      --rom    the ROM set: MAME's ddonpach.zip, or a folder holding it or its files (default: rom). Read on the
 *               first run only: its contents are kept in ddonpach.dat beside the program, read from then on; given
 *               on the command line, the set is read and extracted again
 *      --scale  the window's size, in times the picture's 240 x 320 (default 3)
 *      --no-sound      plays no sound
 *      --eeprom PATH   where the EEPROM is kept: the test mode's settings (default: ddonpach.eeprom beside the
 *                      program). 128 bytes, as MAME's nvram/ddonpach/eeprom, which can be copied there. A new one
 *                      starts from the ROM set's
 *      --cheat NAME    turns on the cheat of the cheat file whose name holds NAME, as many times as there are cheats;
 *                      NAME=VALUE picks a value where the cheat asks for one
 *      --cheats FILE   the cheat file, MAME's format (default: cheats/ddonpach.xml)
 *      --list-cheats   lists the cheat file's cheats
 *
 *    Controls (the defaults: ddonpach.ini changes them): keys arrows, Z shot (held: the laser), X bomb, C auto-fire,
 *          1 start,
 *          5 coin, F2 the service switch (held 2 seconds at the title or in the attract mode: the test mode); player 2
 *          R F D G, A S, 2, 6.
 *          Gamepads (the first plugged in is player 1's, the second player 2's; the Xbox layout's names): d-pad or
 *          left stick move, A shot, B bomb, X auto-fire, Start start. Keys and gamepads work together.
 *          C (X on the gamepad) is the game's own auto-fire: [game] c_button_full_auto sets the test mode's "C BUTTON
 *          FULL-AUTO" (on by default).
 *          F3 resets the game, F4 the test mode (the service switch held for you; at the title or in the attract
 *          mode, after the warning screen), F11 or Alt+Enter full screen, Escape or the gamepad's View button the
 *          options menu (QUIT TO DESKTOP there).
 *    Fast forward: held, LB on the gamepad or Tab ([hotkeys] fast_forward; [game] fast_forward_speed, 4 by default).
 *    Save states: F5 keeps the game as it is now, F8 goes back to it, in the chosen slot of 5 (F6 / F7), in memory
 *          until the program closes.
 *    The picture: each frame shown as the board shows it, as its VBLANK comes (the look-ahead: look_ahead); [game]
 *    arcade_timing true draws it as the game's vblank interrupt returns, a frame later (as MAME).
 *    Free play ([game] free_play, on by default): Start drops in the coins a game needs (cave/free_play.h).
 *    High scores: kept in ddonpach.hiscore beside the program (hiscore.h; the game options' SAVE HIGH SCORES, or
 *          [game] save_high_scores); not saved from a game that had a cheat on or went back to a save state.
 *    The start's WARNING screen (31 seconds) is run at once, unseen ([game] skip_warning, on by default).
 */
#include "crash_log.h"
#include "crt.h"
#include "hiscore.h"
#include "menu.h"
#include "paths.h"
#include "rom_drop.h"
#include "scaler.h"
#include "settings.h"
#include "window_icon.h"

#include "cave/boards.h"
#include "cave/cheats.h"
#include "cave/free_play.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"
#include "cave/video.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DDONPACH_VERSION
#define DDONPACH_VERSION "dev"
#endif

/* the settings: ddonpach.ini, then the command line (settings.h) */
static settings options, file_options;    /* (in use: the file's and the command line's; the file's alone) */
static char settings_path[1024];
static SDL_Window *window;
static int running = 1;
static bool fast_forwarding;
static bool state_loaded;    /* this game went back to a save state, or had a cheat on: its scores are not kept
                                (until the next game: hiscore.h) */
static bool reset_asked;    /* a reset asked for this frame (the look-ahead's state would undo it: look_ahead) */

/* ---- the gamepads ---- */

static SDL_GameController *pads[2];    /* player 1's, player 2's: the first two plugged in */

/* A gamepad plugged in (SDL also tells of those there at the start): the first free player's */
static void pad_added(int device)
{
	SDL_GameController *pad;

	for (int player = 0; player < 2; player++)
		if (pads[player] && SDL_JoystickGetDeviceInstanceID(device)
				== SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[player])))
			return;    /* (already open) */
	for (int player = 0; player < 2; player++)
		if (!pads[player])
		{
			if ((pad = SDL_GameControllerOpen(device)) == NULL)
				fprintf(stderr, "gamepad %d: %s\n", device, SDL_GetError());
			pads[player] = pad;
			return;
		}
}

static void pad_removed(SDL_JoystickID instance)
{
	for (int player = 0; player < 2; player++)
		if (pads[player] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[player])) == instance)
		{
			SDL_GameControllerClose(pads[player]);
			pads[player] = NULL;
		}
}

static uint16_t pad_inputs(int player)
{
	SDL_GameController *const pad = pads[player];
	const int dead = options.stick_dead[player] * 32767 / 100;
	uint16_t pressed = 0;
	int x, y;

	if (!pad)
		return 0;
	for (int action = 0; action < ACTIONS; action++)
		for (int binding = 0; binding < BINDINGS_MAX; binding++)
			if (settings_pad_pressed(pad, options.buttons[player][action][binding]))
				pressed |= settings_action_bits[action];
	if (!options.left_stick[player])
		return pressed;
	x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
	y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
	if (x < -dead)
		pressed |= CAVE_LEFT;
	else if (x > dead)
		pressed |= CAVE_RIGHT;
	if (y < -dead)
		pressed |= CAVE_UP;
	else if (y > dead)
		pressed |= CAVE_DOWN;
	return pressed;
}

/* ---- the hotkeys: what the port does itself ([hotkeys]) ---- */

#define SERVICE_MENU_FRAMES 150    /* the service switch held for the player: DoDonPachi goes to its test mode once
                                      the switch has been held 120 frames (its counter at $10129C, up to $78), at the
                                      title or in the attract mode (not on the warning screen, nor at the reset) */

static unsigned service_held;      /* frames the service switch is still held for the player */
static bool hotkey_was_down[HOTKEYS];

static void state_action(bool save);
static void state_slot_hotkey(int step);

static bool hotkey_down(int hotkey)
{
	const Uint8 *const keys = SDL_GetKeyboardState(NULL);

	for (int binding = 0; binding < BINDINGS_MAX; binding++)
	{
		const SDL_Scancode key = options.hot_keys[hotkey][binding];

		if (key != SDL_SCANCODE_UNKNOWN && keys[key])
			return true;
		for (int player = 0; player < 2; player++)
			if (pads[player] && settings_pad_pressed(pads[player], options.hot_buttons[hotkey][binding]))
				return true;
	}
	return false;
}

/* The board's start: its self-test, then the WARNING screen, typed out and held, about 1,809 frames (31 seconds)
   after each reset before the attract mode's first picture (the SCORE RANKING). With [game] skip_warning its frames
   are run unseen, at once (all of them, as the board runs them: the game the same, about 0.15 s). Over when the
   game's between-games flag ($1017A5 bit 7) is first set; at most WARNING_FRAMES_MOST */
#define WARNING_FRAMES_MOST 2400
#define WARNING_FRAMES_AT_ONCE 600    /* (a frame of the window's at most: it answers meanwhile) */

static bool warning_showing(void)
{
	return program_frames_since_reset() < WARNING_FRAMES_MOST && !(*cave_work_ram_byte(0x1017A5u) & 0x80u);
}

/* A hotkey pressed (not held from before): the reset, or the test mode (the service switch held for the player); the
   save states (fast forward is held: the main loop asks hotkey_down) */
static void hotkeys(void)
{
	for (int hotkey = HOTKEY_RESET; hotkey <= HOTKEY_SERVICE_MENU; hotkey++)
	{
		const bool down = hotkey_down(hotkey);

		if (down && !hotkey_was_down[hotkey])
		{
			if (hotkey == HOTKEY_SERVICE_MENU)
			{
				service_held = SERVICE_MENU_FRAMES;    /* (no reset: the game looks at the switch after its warning) */
				menu_notice("THE TEST SWITCH HELD");
			}
			else
			{
				program_reset();
				reset_asked = true;
				service_held = 0;
				free_play_reset();
				hiscore_reset();
				state_loaded = false;
				menu_notice("RESET");
			}
		}
		hotkey_was_down[hotkey] = down;
	}
	for (int hotkey = HOTKEY_SAVE_STATE; hotkey <= HOTKEY_SLOT_NEXT; hotkey++)
	{
		const bool down = hotkey_down(hotkey);

		if (down && !hotkey_was_down[hotkey])
		{
			if (hotkey == HOTKEY_SLOT_PREVIOUS || hotkey == HOTKEY_SLOT_NEXT)
				state_slot_hotkey(hotkey == HOTKEY_SLOT_NEXT ? 1 : -1);
			else
				state_action(hotkey == HOTKEY_SAVE_STATE);
		}
		hotkey_was_down[hotkey] = down;
	}
}

/* what the player holds on their keys and gamepad: the board's input bits, 1 = held */
static uint16_t held_inputs(int player)
{
	const Uint8 *const keys = SDL_GetKeyboardState(NULL);
	uint16_t pressed = pad_inputs(player);

	for (int action = 0; action < ACTIONS; action++)
		for (int binding = 0; binding < BINDINGS_MAX; binding++)
			if (options.keys[player][action][binding] != SDL_SCANCODE_UNKNOWN
					&& keys[options.keys[player][action][binding]])
				pressed |= settings_action_bits[action];
	return pressed;
}

/* what the game does not see until it is let go: what was held as the menu closed (the A that chose RESUME is no
   shot) */
static uint16_t ignored[2];

static void ignore_held_inputs(void)
{
	ignored[0] = held_inputs(0);
	ignored[1] = held_inputs(1);
}

/* The inputs from the keys and the gamepads held: active low */
static void read_keys(void)
{
	uint16_t pressed[2];

	for (int player = 0; player < 2; player++)
	{
		const uint16_t held = held_inputs(player);

		ignored[player] &= held;    /* (let go: seen again) */
		pressed[player] = (uint16_t)(held & ~ignored[player]);
	}
	if (service_held)
	{
		pressed[0] |= CAVE_SERVICE;
		service_held--;
	}
	cave_board.in0 = (uint16_t)~pressed[0];
	cave_board.in1 = (uint16_t)~pressed[1];
	if (options.free_play)
		free_play_inputs();    /* (a Start with too few credits: the coins first) */
}

static void show_problem(const char *problem)
{
	fprintf(stderr, "The game does not start: %s\n", problem);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DoDonPachi", problem, NULL);
}

/* ---- the game's data: extracted from the ROM set on the first run, read from then on (no .zip needed) ---- */

/* beside the program: each version's set's contents (never in the repository) */
static const char *const data_files[2] = { "ddonpach.dat", "ddonpach_jpn.dat" };

static bool data_extracted;

/* The version's extracted data beside the program; without it (the first run, or a damaged file), the ROM set ('rom'
   in the settings), then its contents saved as the data for next time. 'rom_given': the command line's --rom, read
   (and extracted) whatever data there is */
static const char *game_data_open(int version, bool rom_given)
{
	char path[1024];
	const char *data_problem = "--rom given", *rom_problem, *save_problem;

	snprintf(path, sizeof(path), "%s%s", paths_folder(), data_files[version]);
	if (!rom_given && (data_problem = rom_set_load_data_version(path, version)) == NULL)
		return NULL;
	if ((rom_problem = rom_set_load_version(options.rom, version)) != NULL)
	{
		/* no data and no set: a window asks for the set to be dropped onto it */
		bool saved;

		fprintf(stderr, "%s (%s): the ROM set is asked for\n", rom_problem, data_problem);
		if (!rom_drop_screen_version(path, version ? "NO DDONPACH_JPN.DAT, AND NO DDONPACHJ.ZIP IN THE 'ROM' FOLDER"
				: "NO DDONPACH.DAT, AND NO DDONPACH.ZIP IN THE 'ROM' FOLDER", &saved, version))
			return "";    /* (the window closed: no game, no message) */
		data_extracted = saved;
		return NULL;
	}
	if ((save_problem = rom_set_save_data(path)) != NULL)
		fprintf(stderr, "the game's data was not saved: %s (the ROM set is read again next time)\n", save_problem);
	else
	{
		data_extracted = true;
		fprintf(stderr, "the game's data extracted from %s into %s\n", options.rom, path);
	}
	return NULL;
}

/* ---- the sound: the board's samples (cave/sound.h), queued to SDL's audio device a frame at a time ---- */

#define AUDIO_AHEAD_MS     50u     /* silence queued at the start: what the device plays from while frames come */
#define AUDIO_MOST_MS      200u    /* past this much queued (the device's clock slower than the frame's), it starts
                                      again from AUDIO_AHEAD_MS: a skip, rarely */

static SDL_AudioDeviceID audio;

static Uint32 audio_bytes(unsigned milliseconds)
{
	return (Uint32)(CAVE_SOUND_RATE * milliseconds / 1000u * sizeof(int16_t));
}

static void audio_open(void)
{
	SDL_AudioSpec want, have;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		fprintf(stderr, "no sound: %s\n", SDL_GetError());
		return;
	}
	SDL_zero(want);
	want.freq = (int)CAVE_SOUND_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = 512;
	audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);    /* (SDL converts to what the device takes) */
	if (!audio)
	{
		fprintf(stderr, "no sound: %s\n", SDL_GetError());
		return;
	}
	SDL_PauseAudioDevice(audio, 0);
}

/* silence, so the device has something to play while the frames come */
static void audio_restart(void)
{
	static const int16_t silence[CAVE_SOUND_RATE * AUDIO_AHEAD_MS / 1000u];

	SDL_ClearQueuedAudio(audio);
	SDL_QueueAudio(audio, silence, (Uint32)sizeof(silence));
}

/* the frame's samples to the device */
static void audio_queue(void)
{
	int16_t samples[2048];
	unsigned count;

	if (!audio)
	{
		cave_sound_drop();
		return;
	}
	if (SDL_GetQueuedAudioSize(audio) > audio_bytes(AUDIO_MOST_MS))
		audio_restart();
	while ((count = cave_sound_take(samples, 2048u)) > 0)
	{
		if (options.volume < 100)
			for (unsigned index = 0; index < count; index++)
				samples[index] = (int16_t)(samples[index] * options.volume / 100);
		SDL_QueueAudio(audio, samples, (Uint32)(count * sizeof(int16_t)));
	}
}

/* ---- the EEPROM's file: loaded before the first frame, saved soon after the game changes it, and at the end ---- */

#define EEPROM_CHECK_FRAMES 60u    /* how often it is checked for changes: about a second */

static char eeprom_path[1024];
static uint16_t eeprom_saved[CAVE_EEPROM_WORDS];

static void eeprom_keep(bool always);

/* The test mode's "C BUTTON FULL-AUTO" (configuration item 3: the board's own auto-fire on button C), as DonPachi's:
   words 1 and 4 as the test mode writes them when that is the only setting changed from the ROM set's EEPROM (word
   4's bit 2 the setting; word 1 from FFFB to FFFF). Set before the game's first frame, as it reads the EEPROM as it
   starts */
static void eeprom_c_button_full_auto(bool on)
{
	/* (word 1's bit 2 with word 4's: FFFB off, FFFF on, as the test mode writes them both ways) */
	if (on)
	{
		cave_board.eeprom.words[1] |= 0x0004u;
		cave_board.eeprom.words[4] |= 0x0004u;
	}
	else
	{
		cave_board.eeprom.words[1] &= (uint16_t)~0x0004u;
		cave_board.eeprom.words[4] &= (uint16_t)~0x0004u;
	}
}

/* each version's own high score table beside the program (hiscore.h) */
static const char *const hiscore_files[2] = { "ddonpach.hiscore", "ddonpach_jpn.hiscore" };

static void hiscores_open(void)
{
	char path[1024];
	const char *problem;

	snprintf(path, sizeof(path), "%s%s", paths_folder(), hiscore_files[rom_set.version ? 1 : 0]);
	if ((problem = hiscore_open(path)) != NULL)
		fprintf(stderr, "%s: %s\n", path, problem);
}

/* each version's own EEPROM beside the program (the settings' or --eeprom's: the World one's) */
static const char *const eeprom_files[2] = { "ddonpach.eeprom", "ddonpach_jpn.eeprom" };

static void eeprom_open(const char *path)
{
	if (path && rom_set.version == 0)
		snprintf(eeprom_path, sizeof(eeprom_path), "%s", path);
	else
		snprintf(eeprom_path, sizeof(eeprom_path), "%s%s", paths_folder(), eeprom_files[rom_set.version ? 1 : 0]);
	switch (cave_eeprom_load(eeprom_path))
	{
	case 1:
		break;    /* (the player's own settings) */
	case 0:
		eeprom_keep(true);    /* (the ROM set's, written for next time) */
		break;
	default:
		fprintf(stderr, "%s is not an EEPROM of 128 bytes: the ROM set's is used (and saved over it)\n", eeprom_path);
		break;
	}
	memcpy(eeprom_saved, cave_board.eeprom.words, sizeof(eeprom_saved));
	if (options.c_button_full_auto)
	{
		eeprom_c_button_full_auto(true);
		eeprom_keep(false);    /* (saved if it was not set) */
	}
}

static void eeprom_keep(bool always)
{
	const char *problem;

	if (!always && !memcmp(eeprom_saved, cave_board.eeprom.words, sizeof(eeprom_saved)))
		return;
	if ((problem = cave_eeprom_save(eeprom_path)) != NULL)
		fprintf(stderr, "%s: %s\n", eeprom_path, problem);
	else
		memcpy(eeprom_saved, cave_board.eeprom.words, sizeof(eeprom_saved));
}

/* ---- the picture ---- */

static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
static SDL_Renderer *renderer;
static SDL_Texture *texture;

/* the picture from the board's memories, with the sprites latched last */
static void draw_picture(void)
{
	video_draw(frame);
	video_rotated(frame, picture);
}

/* ---- the picture in the window ([video]: scaling, smoothing, the pixel-art filter, scanlines or the CRT look,
   rotation) ---- */

/* the picture's size on the screen, in the game's pixels: 240 x 320, or 320 x 240 turned for a monitor on its side */
static int shown_width(void)
{
	return options.rotate == ROTATE_NONE ? VIDEO_HEIGHT : VIDEO_WIDTH;
}

static int shown_height(void)
{
	return options.rotate == ROTATE_NONE ? VIDEO_WIDTH : VIDEO_HEIGHT;
}

/* a point of the unturned picture's rectangle, turned about the centre (cx, cy) as the picture is */
static void turn(float x, float y, float cx, float cy, float *out_x, float *out_y)
{
	if (options.rotate == ROTATE_RIGHT)
	{
		*out_x = cx - (y - cy);
		*out_y = cy + (x - cx);
	}
	else if (options.rotate == ROTATE_LEFT)
	{
		*out_x = cx + (y - cy);
		*out_y = cy - (x - cx);
	}
	else
	{
		*out_x = x;
		*out_y = y;
	}
}

/* the picture made larger by the settings' pixel-art scaler (scaler.h), in a texture of its own; NULL when it is
   off (or there is no memory for it: the plain picture then). 'shown_w': the picture's width on the screen: where
   that is less than the copy's, the copy is shrunk smoothly whatever SMOOTHING says (sharp, it would lose lines) */
static SDL_Texture *filtered_picture(const uint32_t *pixels, float shown_w)
{
	static SDL_Texture *filtered;
	static uint32_t *filtered_pixels;
	static int filtered_factor;
	const int factor = scaler_factor(options.filter);

	if (factor <= 1)
		return NULL;
	if (factor != filtered_factor)
	{
		if (filtered)
			SDL_DestroyTexture(filtered);
		free(filtered_pixels);
		filtered_factor = 0;
		filtered_pixels = malloc((size_t)VIDEO_WIDTH * VIDEO_HEIGHT * (size_t)(factor * factor) * sizeof(uint32_t));
		filtered = filtered_pixels ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
			VIDEO_HEIGHT * factor, VIDEO_WIDTH * factor) : NULL;
		if (!filtered)
		{
			fprintf(stderr, "filter %s: %s\n", scaler_key(options.filter), filtered_pixels ? SDL_GetError() : "no memory");
			free(filtered_pixels);
			filtered_pixels = NULL;
			options.filter = SCALER_OFF;    /* (for this run: the plain picture) */
			return NULL;
		}
		filtered_factor = factor;
	}
	scaler_run(options.filter, pixels, VIDEO_HEIGHT, VIDEO_WIDTH, filtered_pixels);
	SDL_UpdateTexture(filtered, NULL, filtered_pixels, VIDEO_HEIGHT * factor * (int)sizeof(uint32_t));
	SDL_SetTextureScaleMode(filtered, options.smooth || shown_w < (float)(VIDEO_HEIGHT * factor) - 0.5f
		? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	return filtered;
}

static void show_picture(const uint32_t *pixels)
{
	static const Uint8 darkness[4] = { 0, 70, 120, 170 };    /* (the scanlines' dark half: off, light, medium, heavy) */
	int out_w, out_h;
	float scale_x, scale_y, w, h, x, y, cx, cy;
	SDL_FRect unturned;

	SDL_UpdateTexture(texture, NULL, pixels, VIDEO_HEIGHT * (int)sizeof(uint32_t));
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
	scale_x = (float)out_w / (float)shown_width();
	scale_y = (float)out_h / (float)shown_height();
	if (options.scaling != VIDEO_STRETCH)
	{
		scale_x = scale_y = scale_x < scale_y ? scale_x : scale_y;
		if (options.scaling == VIDEO_INTEGER && scale_x >= 1.0f)
			scale_x = scale_y = (float)(int)scale_x;
	}
	w = (float)shown_width() * scale_x;
	h = (float)shown_height() * scale_y;
	x = ((float)out_w - w) / 2.0f;
	y = ((float)out_h - h) / 2.0f;
	if (options.scaling == VIDEO_INTEGER)
	{
		x = (float)(int)x;    /* (whole pixels: every game pixel the same size) */
		y = (float)(int)y;
	}
	cx = x + w / 2.0f;
	cy = y + h / 2.0f;
	/* the texture's rectangle before it is turned: 240 x 320 in its own shape */
	unturned.w = options.rotate == ROTATE_NONE ? w : h;
	unturned.h = options.rotate == ROTATE_NONE ? h : w;
	unturned.x = cx - unturned.w / 2.0f;
	unturned.y = cy - unturned.h / 2.0f;
	/* the arcade monitor's look (crt.c), when it is on and the renderer can: its own lines, not the scanlines */
	if (options.crt != CRT_OFF && crt_draw(renderer, texture, pixels, unturned,
			options.rotate == ROTATE_RIGHT ? 90 : options.rotate == ROTATE_LEFT ? 270 : 0, options.crt))
	{
		SDL_RenderPresent(renderer);
		return;
	}
	{
		/* the picture, or its pixel-art scaler's larger copy (the same size on the screen) */
		SDL_Texture *const filtered = filtered_picture(pixels, unturned.w);

		SDL_RenderCopyExF(renderer, filtered ? filtered : texture, NULL, &unturned,
			options.rotate == ROTATE_RIGHT ? 90.0 : options.rotate == ROTATE_LEFT ? 270.0 : 0.0, NULL, SDL_FLIP_NONE);
	}

	/* the scanlines: the arcade monitor's lines are the board's rows, the picture's columns (the monitor stands on
	   its side in the cabinet): the dark half of each, turned with the picture; none when a line is under 2 pixels */
	if (options.scanlines > 0 && unturned.w / VIDEO_HEIGHT >= 2.0f)
	{
		static SDL_FRect lines[VIDEO_HEIGHT];
		const float band = unturned.w / VIDEO_HEIGHT;

		for (int line = 0; line < VIDEO_HEIGHT; line++)
		{
			const float x0 = unturned.x + band * (float)line + band * 0.5f, x1 = unturned.x + band * (float)(line + 1);
			float ax, ay, bx, by;

			turn(x0, unturned.y, cx, cy, &ax, &ay);
			turn(x1, unturned.y + unturned.h, cx, cy, &bx, &by);
			lines[line].x = ax < bx ? ax : bx;
			lines[line].y = ay < by ? ay : by;
			lines[line].w = ax < bx ? bx - ax : ax - bx;
			lines[line].h = ay < by ? by - ay : ay - by;
		}
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, darkness[options.scanlines & 3]);
		SDL_RenderFillRectsF(renderer, lines, VIDEO_HEIGHT);
	}
	SDL_RenderPresent(renderer);
}

/* ---- the window ---- */

/* full screen as the settings have it: borderless (a window over the desktop) or exclusive */
static Uint32 fullscreen_flag(void)
{
	return options.exclusive ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP;
}

static void set_fullscreen(void)
{
	const Uint32 now = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;

	if (now == fullscreen_flag())
		return;
	if (now)
		SDL_SetWindowFullscreen(window, 0);    /* (from one kind to the other: out of it first) */
	if (options.exclusive)
	{
		/* the desktop's own mode: no change of resolution or refresh rate */
		SDL_DisplayMode desktop;
		const int display = SDL_GetWindowDisplayIndex(window);

		if (display >= 0 && SDL_GetDesktopDisplayMode(display, &desktop) == 0)
			SDL_SetWindowDisplayMode(window, &desktop);
	}
	SDL_SetWindowFullscreen(window, fullscreen_flag());
}

/* the title: the game, its version, and the fast forward while it is held */
static void window_title(void)
{
	char title[96];

	snprintf(title, sizeof(title), "DoDonPachi%s%s", rom_set.version ? " (Japan)" : "",
		fast_forwarding ? "  [fast forward]" : "");
	SDL_SetWindowTitle(window, title);
}

/* As the vblank interrupt returns: the picture drawn there, as the board shows it (the sprites latched as VBLANK
   began, the layers' scroll and the palette as the interrupt has just set them) */
static void after_vblank(void)
{
	draw_picture();
}

/* ---- the look-ahead picture: no frame of lag the board does not have ---- */


/* The board shows a frame as its VBLANK comes: the sprites latched then, the layers' scroll and the palette as the
   vblank interrupt sets them just after (its first 2 ms). program_run_frame returns at that VBLANK, before the
   interrupt, so a picture drawn as the interrupt returns (after_vblank) comes a frame later, after the next frame's
   work. Here the program runs on through that next frame with the state saved first, its interrupt drawing the
   picture the board shows now, and the state is put back: the game goes on exactly as it would without it (the next
   frame is run again, for real, with the inputs then; checked: the same game at every frame over 30,000 VBLANKs, and
   the same picture as the frame after draws at each). Its sound is not kept (cave_sound_quiet). Costs the game's
   work twice a frame: a millisecond or less. Without a state (no memory), the picture of arcade timing */
static void look_ahead(void)
{
	program_state *state;

	if (options.arcade_timing || reset_asked || !(state = program_state_save()))
		return;
	cave_sound_quiet = true;
	program_run_frame();    /* (its interrupt draws the picture: after_vblank) */
	cave_sound_quiet = false;
	program_state_load(state);
	program_state_free(state);    /* (freed once it is put back, at the next frame) */
}

/* ---- the save states (cave/program.h): STATE_SLOTS of them, in memory, until the program closes ---- */

static program_state *slot_states[STATE_SLOTS];
static int state_slot;             /* the slot save and load use */

static void state_action(bool save)
{
	char text[32];

	if (save)
	{
		program_state *const state = program_state_save();

		if (!state)
		{
			menu_notice("NO MEMORY FOR A SAVE STATE");
			return;
		}
		program_state_free(slot_states[state_slot]);
		slot_states[state_slot] = state;
		snprintf(text, sizeof(text), "STATE SAVED (SLOT %d)", state_slot + 1);
		menu_notice(text);
		return;
	}
	if (!slot_states[state_slot])
	{
		snprintf(text, sizeof(text), "SLOT %d IS EMPTY", state_slot + 1);
		menu_notice(text);
		return;
	}
	program_state_load(slot_states[state_slot]);
	free_play_reset();
	hiscore_state_loaded();    /* (the file's table back: no high score from this game) */
	state_loaded = true;
	video_latch_sprites();
	draw_picture();
	if (audio)
		audio_restart();
	cave_sound_drop();
	snprintf(text, sizeof(text), "STATE LOADED (SLOT %d)", state_slot + 1);
	menu_notice(text);
}

/* F6 / F7: the slot before or after, told on the screen */
static void state_slot_hotkey(int step)
{
	char text[32];

	state_slot = ((state_slot + step) % STATE_SLOTS + STATE_SLOTS) % STATE_SLOTS;
	snprintf(text, sizeof(text), "STATE SLOT %d%s", state_slot + 1, slot_states[state_slot] ? "" : " (EMPTY)");
	menu_notice(text);
}

/* ---- what the options menu does (menu.h's menu_host) ---- */

static void menu_reset(void)
{
	program_reset();
	reset_asked = true;
	service_held = 0;
	free_play_reset();
	hiscore_reset();
	state_loaded = false;
	menu_notice("RESET");
}

static void menu_test_mode(void)
{
	service_held = SERVICE_MENU_FRAMES;    /* (no reset: the game looks at the switch after its warning) */
	menu_notice("THE TEST SWITCH HELD");
}

static void menu_apply_window(void)
{
	if (options.fullscreen)
		set_fullscreen();
	else
	{
		SDL_SetWindowFullscreen(window, 0);
		SDL_SetWindowSize(window, shown_width() * options.scale, shown_height() * options.scale);
	}
}

/* the smoothing and vsync, as the settings have them */
static void menu_apply_video(void)
{
	SDL_SetTextureScaleMode(texture, options.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	if (SDL_RenderSetVSync(renderer, options.vsync ? 1 : 0) != 0)
		fprintf(stderr, "vsync: %s\n", SDL_GetError());
}

/* the auto-fire into the EEPROM now (both ways: off as the test mode clears it); the game reads it at its reset */
static void menu_apply_auto_fire(void)
{
	eeprom_c_button_full_auto(options.c_button_full_auto);
	eeprom_keep(false);
}

static void menu_quit(void)
{
	running = 0;
}

static int menu_state_slot(int step)
{
	if (step)
		state_slot = ((state_slot + step) % STATE_SLOTS + STATE_SLOTS) % STATE_SLOTS;
	return state_slot;
}

static bool menu_state_used(int slot)
{
	return slot_states[slot] != NULL;
}

#define CHEATS_MAX 16

int main(int argc, char **argv)
{
	const char *cheat_names[CHEATS_MAX];
	bool rom_given = false;
	int cheat_count = 0, list_cheats = 0;
	bool settings_written;
	const char *problem;
	Uint64 next, period;

	{
		/* ddonpach.log beside the program (Windows: stderr; a crash written there) */
		char log_path[1100];

		snprintf(log_path, sizeof(log_path), "%sddonpach.log", paths_folder());
		crash_log_start(log_path);
	}
	cave_board_describe(&cave_board_ddonpach);
	/* the settings file, then the command line over it */
	settings_defaults(&file_options);
	settings_path[0] = '\0';
	for (int index = 1; index + 1 < argc; index++)
		if (!strcmp(argv[index], "--settings"))
			snprintf(settings_path, sizeof(settings_path), "%s", argv[index + 1]);
	if (!settings_path[0])
		snprintf(settings_path, sizeof(settings_path), "%sddonpach.ini", paths_folder());
	problem = settings_load(&file_options, settings_path, &settings_written);
	options = file_options;    /* (the command line goes over it, for this run only) */
	if (problem)
	{
		char message[1400];

		snprintf(message, sizeof(message), "%s: %s.\nThose settings keep their defaults.", settings_path, problem);
		fprintf(stderr, "%s\n", message);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "DoDonPachi settings", message, NULL);
	}
	for (int index = 1; index < argc; index++)
	{
		const char *const value = index + 1 < argc ? argv[index + 1] : NULL;

		if (!strcmp(argv[index], "--settings") && value)
			index++;
		else if (!strcmp(argv[index], "--rom") && value)
		{
			snprintf(options.rom, sizeof(options.rom), "%s", argv[++index]);
			rom_given = true;    /* (the set read again, and extracted again) */
		}
		else if (!strcmp(argv[index], "--cheat") && value && cheat_count < CHEATS_MAX)
			cheat_names[cheat_count++] = argv[++index];
		else if (!strcmp(argv[index], "--cheats") && value)
			snprintf(options.cheats, sizeof(options.cheats), "%s", argv[++index]);
		else if (!strcmp(argv[index], "--list-cheats"))
			list_cheats = 1;
		else if (!strcmp(argv[index], "--no-sound"))
			options.sound = false;
		else if (!strcmp(argv[index], "--eeprom") && value)
			snprintf(options.eeprom, sizeof(options.eeprom), "%s", argv[++index]);
		else if (!strcmp(argv[index], "--scale") && value)
			options.scale = atoi(argv[++index]);
	}
	if (options.scale < 1)
		options.scale = 1;
	{
		/* (the ROM set and the cheat file as the settings name them: from the working folder, else beside the
		   program, as a game started from a desktop's file manager or a launcher may not be started in its folder) */
		char found[sizeof(options.rom)];

		paths_find(options.rom, found, sizeof(found));
		snprintf(options.rom, sizeof(options.rom), "%s", found);
		paths_find(options.cheats, found, sizeof(found));
		snprintf(options.cheats, sizeof(options.cheats), "%s", found);
	}
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0)
	{
		fprintf(stderr, "SDL: %s\n", SDL_GetError());
		return 1;
	}
	if (options.version != 0 && !program_has_version(options.version))
	{
		fprintf(stderr, "no Japanese program in this build: the World version\n");
		options.version = 0;
	}
	if ((problem = game_data_open(options.version, rom_given)) != NULL || (problem = program_start()) != NULL)
	{
		if (problem[0])
			show_problem(problem);
		SDL_Quit();
		return 1;
	}
	if (list_cheats)
	{
		printf("the cheats of %s:\n", options.cheats);
		cheats_list(options.cheats);
		SDL_Quit();
		return 0;
	}
	eeprom_open(options.eeprom[0] ? options.eeprom : NULL);
	hiscores_open();
	cheats_load(options.cheats);
	for (int cheat = 0; cheat < cheat_count; cheat++)
		if ((problem = cheats_enable(options.cheats, cheat_names[cheat])) != NULL)
		{
			show_problem(problem);
			SDL_Quit();
			return 1;
		}
	window = SDL_CreateWindow("DoDonPachi", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		shown_width() * options.scale, shown_height() * options.scale,
		SDL_WINDOW_RESIZABLE | (options.fullscreen && !options.exclusive ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	if (window)
	{
		window_icon_set(window);
		window_title();
	}
	if (window && options.fullscreen && options.exclusive)
		set_fullscreen();    /* (exclusive: at the desktop's mode, not the window's size) */
	renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
	if (!renderer)
		renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : NULL;
	texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		VIDEO_HEIGHT, VIDEO_WIDTH) : NULL;
	if (!texture)
	{
		show_problem(SDL_GetError());
		SDL_Quit();
		return 1;
	}
	SDL_SetTextureScaleMode(texture, options.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	if (SDL_RenderSetVSync(renderer, options.vsync ? 1 : 0) != 0)
		fprintf(stderr, "vsync: %s\n", SDL_GetError());
	program_after_vblank = after_vblank;
	{
		menu_host menu = { 0 };

		menu.options = &options;
		menu.file = &file_options;
		menu.path = settings_path;
		menu.pads = pads;
		menu.reset = menu_reset;
		menu.test_mode = menu_test_mode;
		menu.apply_window = menu_apply_window;
		menu.apply_video = menu_apply_video;
		menu.apply_auto_fire = menu_apply_auto_fire;
		menu.quit = menu_quit;
		menu.state = state_action;
		menu.state_slot = menu_state_slot;
		menu.state_used = menu_state_used;
		menu_init(&menu);
	}
	if (data_extracted)
		menu_notice("ROM DATA EXTRACTED");
	if (options.sound)
		audio_open();
	if (audio)
		audio_restart();
	SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

	program_set_slowdown(options.slowdown);    /* (the arcade's slowdown, as the settings have it) */
	period = SDL_GetPerformanceFrequency() * CAVE_FRAME_USEC / 1000000u;
	next = SDL_GetPerformanceCounter() + period;
	while (running)
	{
		SDL_Event event;

		if (program_frames() % EEPROM_CHECK_FRAMES == 0)
			eeprom_keep(false);
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_RENDER_TARGETS_RESET || event.type == SDL_RENDER_DEVICE_RESET)
				crt_reset();    /* (its textures lost: made again) */
			if (event.type == SDL_QUIT)
				running = 0;
			else if (event.type == SDL_CONTROLLERDEVICEADDED)
				pad_added(event.cdevice.which);
			else if (event.type == SDL_CONTROLLERDEVICEREMOVED)
				pad_removed(event.cdevice.which);
			else if (event.type == SDL_KEYDOWN && !event.key.repeat
					&& (event.key.keysym.sym == SDLK_F11
						|| (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT))))
			{
				options.fullscreen = !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP);
				if (options.fullscreen)
					set_fullscreen();
				else
					SDL_SetWindowFullscreen(window, 0);
			}
			else if (menu_is_open())
				menu_event(&event);
			else if ((event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym == SDLK_ESCAPE)
					|| (event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK))
				menu_open();    /* (the options menu: the game paused while it is up) */
		}
		if (menu_just_closed())
		{
			ignore_held_inputs();
			if (audio)
				audio_restart();
		}
		if (!running)
			break;
		if (menu_is_open())
		{
			/* (the game paused: the menu over its last picture, at the board's pace) */
			static uint32_t menu_picture[VIDEO_WIDTH * VIDEO_HEIGHT];

			menu_tick();
			menu_draw(picture, menu_picture);
			show_picture(menu_picture);
			goto pace;
		}
		if (program_slowdown() != options.slowdown)
			program_set_slowdown(options.slowdown);    /* (changed in the menu) */
		hotkeys();
		{
			/* fast forward, while its key or button is held: fast_forward_speed of the game's frames in this one,
			   the last one shown, no sound */
			const bool fast = hotkey_down(HOTKEY_FAST_FORWARD);
			const bool skip = options.skip_warning && warning_showing();
			const int frames = skip ? WARNING_FRAMES_AT_ONCE : fast ? options.fast_forward_speed : 1;

			if (fast != fast_forwarding)
			{
				fast_forwarding = fast;
				window_title();
				if (!fast && audio)
					audio_restart();
			}
			for (int count = 0; count < frames; count++)
			{
				if (skip && !warning_showing())
					break;    /* (the warning over: the attract mode's first picture from the next frame on) */
				read_keys();
				program_run_frame();
				cheats_apply();
				if (fast || skip)
					cave_sound_drop();
				else
					audio_queue();
				video_latch_sprites();    /* (VBLANK: the 013 takes the sprites) */
				reset_asked = false;      /* (done: the frame began from the reset) */
				if (cheats_any_on())
					state_loaded = true;    /* (an assist: the game doesn't count) */
				hiscore_frame(options.save_high_scores, !state_loaded);
				if (state_loaded && !cheats_any_on() && hiscore_between_games())
				{
					state_loaded = false;      /* (a new game: its scores are kept again) */
					hiscore_game_not_kept();    /* (the table as the file has it: none of that game's scores) */
				}
			}
			look_ahead();    /* (the picture the board shows now) */
		}
		show_picture(menu_with_notice(picture));
pace:

		/* (the board's 57.55 frames a second: SDL_Delay may sleep a whole tick of the system's timer longer than
		   asked, up to 15 ms on Windows, so it sleeps only to 2 ms before the frame's time, and the rest is waited
		   out on the counter) */
		{
			const Uint64 frequency = SDL_GetPerformanceFrequency(), margin = frequency * 2u / 1000u;
			Uint64 now = SDL_GetPerformanceCounter();

			if (now + margin < next)
				SDL_Delay((Uint32)((next - now - margin) * 1000u / frequency));
			while ((now = SDL_GetPerformanceCounter()) < next)
				;
			next = (now > next + 4u * period) ? now + period : next + period;
		}
	}
	eeprom_keep(true);
	for (int slot = 0; slot < STATE_SLOTS; slot++)
		program_state_free(slot_states[slot]);
	if (audio)
		SDL_CloseAudioDevice(audio);
	scaler_quit();
	SDL_Quit();
	return 0;
}
