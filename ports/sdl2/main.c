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
 *          mode, after the warning screen), F9 the FPS meter, F11 or Alt+Enter full screen, Escape or the gamepad's
 *          View button the options menu (QUIT TO DESKTOP there).
 *    Fast forward: held, LB on the gamepad or Tab ([hotkeys] fast_forward; [game] fast_forward_speed, 4 by default).
 *    Save states: F5 keeps the game as it is now, F8 goes back to it, in the chosen slot of 5 (F6 / F7), in memory
 *          until the program closes.
 *    The picture: each frame shown as the board shows it, as its VBLANK comes (the look-ahead: look_ahead); [game]
 *    arcade_timing true draws it as the game's vblank interrupt returns, a frame later (as MAME).
 *    Free play ([game] free_play, on by default): Start drops in the coins a game needs (cave/free_play.h).
 *    High scores: kept in ddonpach.hiscore beside the program (hiscore.h; the game options' SAVE HIGH SCORES, or
 *          [game] save_high_scores); not saved from a game that had a cheat on or went back to a save state.
 *    The start's WARNING screen (31 seconds) is run at once, unseen ([game] skip_warning, on by default).
 *    Replays: each arcade game recorded (replay.h); at its game over the question whether to keep it (replays/ beside the
 *          program), played back from the main menu's REPLAYS (fast forward works; STOP REPLAY in the options).
 *    Main menu: as the program starts (START GAME, SETTINGS, QUIT TO DESKTOP; [game] main_menu), and from the options'
 *          MAIN MENU (the game reset); the game waits at its start while it is up, and runs from there with START GAME.
 */
#include "achievements.h"
#include "crash_log.h"
#include "game_watch.h"
#include "stats.h"
#include "crt.h"
#include "hiscore.h"
#include "hitbox.h"
#include "widgets.h"
#include "menu.h"
#include "paths.h"
#include "replay.h"
#include "rom_drop.h"
#include "training.h"
#include "updater.h"
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
#include <time.h>

#ifndef DDONPACH_VERSION
#define DDONPACH_VERSION "dev"
#endif

/* the settings: ddonpach.ini, then the command line (settings.h) */
static settings options, file_options;    /* (in use: the file's and the command line's; the file's alone) */
static char settings_path[1024];
static SDL_Window *window;
static int running = 1;
static bool fast_forwarding;
static bool training_on;     /* a training game (training.h): the main menu's TRAINING, until a reset or the main
                                menu */
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
#define WARNING_FRAMES_LEAST 1500     /* (the warning is never over sooner) */
#define WARNING_FRAMES_AT_ONCE 600    /* (a frame of the window's at most: it answers meanwhile) */

static bool warning_showing(void)
{
	static bool over;              /* (since the last reset: the flag is clear again in a game, 2,400 frames from the
	                                  reset a game started at once would be run unseen too) */
	const uint64_t frames = program_frames_since_reset();

	if (frames == 0u || reset_asked)
	{
		over = false;              /* (a reset, or one asked for: it comes with the next frame, the count from it;
		                              a save state put back keeps its own count, never 0) */
		return true;
	}
	/* (the flag looked at from WARNING_FRAMES_LEAST on: after a reset the work RAM still holds the last run's, set,
	   until the game's start clears it) */
	if (!over && (frames >= WARNING_FRAMES_MOST
			|| (frames >= WARNING_FRAMES_LEAST && *cave_work_ram_byte(0x1017A5u) & 0x80u)))
		over = true;
	return !over;
}

/* A hotkey pressed (not held from before): the reset, or the test mode (the service switch held for the player); the
   save states (fast forward is held: the main loop asks hotkey_down) */
static void replay_end(void);

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
			else if (replay_playing())
				replay_end();    /* (a replay playing: stopped, the main menu's replays) */
			else
			{
				program_reset();
				reset_asked = true;
				service_held = 0;
				free_play_reset();
				hiscore_reset();
				widgets_reset();
				training_on = false;
				state_loaded = false;
				crash_log_reset(options.slowdown, rom_set.version, options.save_high_scores ? hiscore_table() : NULL);
				replay_reset();
				game_watch_reset();
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
			else if (replay_playing())
				menu_notice("NO SAVE STATES IN A REPLAY");
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
	/* (and the stages' best scores, the side panels': widgets.h) */
	snprintf(path, sizeof(path), "%s%s", paths_folder(), rom_set.version ? "ddonpach_jpn.stages" : "ddonpach.stages");
	if ((problem = widgets_open(path)) != NULL)
		fprintf(stderr, "%s: %s\n", path, problem);
	{
		static bool loaded;

		if (!loaded)
		{
			/* (the achievements and statistics: once, as the program starts) */
			snprintf(path, sizeof(path), "%sddonpach.achievements", paths_folder());
			achievements_load(path);
			snprintf(path, sizeof(path), "%sddonpach.stats", paths_folder());
			stats_load(path);
			loaded = true;
		}
	}
	widgets_reset();
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

/* the PC's time in each part of a frame, for the FPS meter's log (counter ticks, added up over its second) */
enum { SPENT_RUN, SPENT_LOOK_AHEAD, SPENT_DRAW, SPENT_SHOW, SPENT_PRESENT, SPENT_PARTS };
static Uint64 spent[SPENT_PARTS], spent_most[SPENT_PARTS];

static void spend(int part, Uint64 since)
{
	const Uint64 ticks = SDL_GetPerformanceCounter() - since;

	spent[part] += ticks;
	if (ticks > spent_most[part])
		spent_most[part] = ticks;
}

/* the picture from the board's memories, with the sprites latched last */
static void draw_picture(void)
{
	const Uint64 began = SDL_GetPerformanceCounter();

	video_draw(frame);
	video_rotated(frame, picture);
	spend(SPENT_DRAW, began);
}

/* the window's picture shown (with vsync, the wait for the monitor is here) */
static void present(void)
{
	const Uint64 began = SDL_GetPerformanceCounter();

	SDL_RenderPresent(renderer);
	spend(SPENT_PRESENT, began);
}

/* ---- the picture in the window ([video]: scaling, smoothing, the pixel-art filter, scanlines or the CRT look,
   rotation) ---- */

/* the side panels (widgets.h): on as the settings have them, and never with the picture turned (a monitor on its
   side has no room beside it). Their width, in the game's pixels, is worked out from the screen's shape as each
   picture is shown: WIDGET_W at least, wider to fill a wider screen. The game's picture is never changed by them:
   it stays the arcade's 240 x 320, at the same scale, between them */
static uint32_t panel_pixels[2][WIDGET_MAX_W * WIDGET_H];
static SDL_Texture *panel_textures[2];
static int panel_width = WIDGET_W;

static bool panels_shown(void)
{
	return options.side_panels && options.rotate == ROTATE_NONE;
}

/* the picture's size on the screen, in the game's pixels: 240 x 320, or 320 x 240 turned for a monitor on its side
   (with the panels and their gaps beside it, when they are shown) */
static int shown_width(void)
{
	if (panels_shown())
		return VIDEO_HEIGHT + 2 * (panel_width + WIDGET_GAP);
	return options.rotate == ROTATE_NONE ? VIDEO_HEIGHT : VIDEO_WIDTH;
}

/* the panels' width for a screen of out_w x out_h: the scale is the picture's (the most the height allows, or the
   width with the narrowest panels), then the panels take the width left over at that scale */
static int panel_width_for(int out_w, int out_h)
{
	const float least = (float)(VIDEO_HEIGHT + 2 * (WIDGET_W + WIDGET_GAP));
	float scale = (float)out_h / (float)VIDEO_WIDTH;
	int width;

	if (options.scaling == VIDEO_STRETCH)
		return WIDGET_W;    /* (stretched to the whole window anyway) */
	if ((float)out_w / least < scale)
		scale = (float)out_w / least;
	if (options.scaling == VIDEO_INTEGER && scale >= 1.0f)
		scale = (float)(int)scale;
	if (scale <= 0.0f)
		return WIDGET_W;
	width = ((int)((float)out_w / scale) - VIDEO_HEIGHT) / 2 - WIDGET_GAP;
	return width < WIDGET_W ? WIDGET_W : width > WIDGET_MAX_W ? WIDGET_MAX_W : width;
}

/* the panels drawn at (x, y), the picture's top left with them, at the picture's scale; the picture's own place and
   width returned */
static void draw_panels(float *x, float y, float *w, float scale_x, float scale_y)
{
	const SDL_Rect source = { 0, 0, panel_width, WIDGET_H };
	const SDL_FRect left = { *x, y, (float)panel_width * scale_x, WIDGET_H * scale_y };
	const SDL_FRect right = { *x + *w - (float)panel_width * scale_x, y, (float)panel_width * scale_x, WIDGET_H * scale_y };

	widgets_draw(panel_pixels[0], panel_pixels[1], panel_width);
	for (int side = 0; side < 2; side++)
	{
		SDL_UpdateTexture(panel_textures[side], &source, panel_pixels[side], panel_width * (int)sizeof(uint32_t));
		SDL_SetTextureScaleMode(panel_textures[side], options.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
		SDL_RenderCopyF(renderer, panel_textures[side], &source, side ? &right : &left);
	}
	*x += (float)(panel_width + WIDGET_GAP) * scale_x;
	*w = VIDEO_HEIGHT * scale_x;
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
	if (panels_shown())
		panel_width = panel_width_for(out_w, out_h);
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
	if (panels_shown() && panel_textures[0] && panel_textures[1])
		draw_panels(&x, y, &w, scale_x, scale_y);
	else if (panels_shown())
	{
		x += (float)(panel_width + WIDGET_GAP) * scale_x;    /* (no textures for them: their place left black) */
		w = VIDEO_HEIGHT * scale_x;
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
		present();
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
	present();
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
/* Drawn only when it will be shown: the look-ahead's picture, or with arcade_timing the last frame's of the window's
   frame (not the frames fast forward or the warning's skip run unseen, nor the real frame's the look-ahead's
   replaces: the drawing is the most of a frame's work) */
static bool picture_wanted = true;

static void after_vblank(void)
{
	if (picture_wanted)
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

	if (options.arcade_timing)
		return;    /* (the picture drawn as the last frame's interrupt returned) */
	if (reset_asked || !(state = program_state_save()))
	{
		draw_picture();    /* (no look-ahead: the board as it is at the VBLANK) */
		return;
	}
	cave_sound_quiet = true;
	picture_wanted = true;
	program_run_frame();    /* (its interrupt draws the picture: after_vblank) */
	picture_wanted = false;
	cave_sound_quiet = false;
	program_state_load(state);
	program_state_free(state);    /* (freed once it is put back, at the next frame) */
}

/* ---- the FPS meter (F9, or the graphics options' FPS METER: [video] show_fps) ---- */

/* Counted over about a second, then shown: the game's own frames (the board's less its frames of slowdown), the
   board's frames run (57.55 a second at the board's pace; fewer: the PC can't keep up), the pictures shown, the
   frames the PC came to its frame's time late (its work and the picture's took longer than a frame), and the
   milliseconds a frame's work and picture take the PC, on average and at most */
static struct
{
	Uint64 since;                   /* the counter when this second began */
	unsigned game_frames, board_frames, windows, late;
	Uint64 work, work_most;         /* (counter ticks) */
	char lines[3][32];
} meter;

static void meter_board_frame(void)
{
	meter.board_frames++;
	if (!program_frame_lagged())
		meter.game_frames++;
}

static void meter_log_second(double seconds);

/* At the frame's end, before its wait: 'began' when its work began, 'next' when the next frame is due */
static void meter_frame_end(Uint64 began, Uint64 next)
{
	const Uint64 frequency = SDL_GetPerformanceFrequency(), now = SDL_GetPerformanceCounter();
	const Uint64 work = now - began;

	meter.windows++;
	meter.work += work;
	if (work > meter.work_most)
		meter.work_most = work;
	if (now > next)
		meter.late++;
	if (!meter.since)
		meter.since = now;
	else if (now - meter.since >= frequency)
	{
		const double seconds = (double)(now - meter.since) / (double)frequency;
		const double ms = 1000.0 / (double)frequency;

		snprintf(meter.lines[0], sizeof(meter.lines[0]), "GAME %.1f FPS", meter.game_frames / seconds);
		snprintf(meter.lines[1], sizeof(meter.lines[1]), "BOARD %.1f  WINDOW %.1f", meter.board_frames / seconds,
			meter.windows / seconds);
		snprintf(meter.lines[2], sizeof(meter.lines[2]), "LATE %u  WORK %.1f/%.1fMS", meter.late,
			meter.windows ? (double)meter.work * ms / meter.windows : 0.0, (double)meter.work_most * ms);
#ifdef METER_LOG
		fprintf(stderr, "%s | %s | %s\n", meter.lines[0], meter.lines[1], meter.lines[2]);
#endif
		if (options.show_fps)
			meter_log_second(seconds);
		memset(spent, 0, sizeof(spent));
		memset(spent_most, 0, sizeof(spent_most));
		meter.since = now;
		meter.game_frames = meter.board_frames = meter.windows = meter.late = 0;
		meter.work = meter.work_most = 0;
	}
}

/* The log: while the meter is on, its second written to ddonpach_fps.log beside the program (started again each
   run), with the settings and the renderer at its top, and the PC's time in each part of a frame: RUN the game's
   frames (with their sound), LOOK the look-ahead's frame (with its picture), DRAW the pictures drawn, SHOW the
   picture put in the window (the filter, the CRT look, PRESENT: with vsync the wait for the monitor) */
static FILE *meter_log;

static void meter_log_open(void)
{
	char path[1024];
	SDL_RendererInfo info;
	SDL_DisplayMode mode;
	int w = 0, h = 0, out_w = 0, out_h = 0;
	const time_t now = time(NULL);
	char when[64];

	snprintf(path, sizeof(path), "%sddonpach_fps.log", paths_folder());
	if (!(meter_log = fopen(path, "w")))
		return;
	strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", localtime(&now));
	fprintf(meter_log, "DoDonPachi FPS log, %s\n", when);
	if (renderer && SDL_GetRendererInfo(renderer, &info) == 0)
		fprintf(meter_log, "renderer %s%s%s\n", info.name, info.flags & SDL_RENDERER_ACCELERATED ? " accelerated" : "",
			info.flags & SDL_RENDERER_PRESENTVSYNC ? " vsync" : "");
	fprintf(meter_log, "renderer setting %s\n", settings_renderer_names[options.renderer]);
	if (window)
	{
		SDL_GetWindowSize(window, &w, &h);
		if (SDL_GetWindowDisplayMode(window, &mode) == 0)
			fprintf(meter_log, "display %dx%d at %d Hz\n", mode.w, mode.h, mode.refresh_rate);
	}
	if (renderer)
		SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
	fprintf(meter_log, "window %dx%d (drawn %dx%d)%s\n", w, h, out_w, out_h, options.fullscreen ? " full screen" : "");
	fprintf(meter_log, "settings: vsync %s, crt %s, filter %s, scaling %s, smooth %s, scanlines %s, rotate %s, "
		"input timing %s, slowdown %s, sound %s\n", options.vsync ? "on" : "off", settings_crt_names[options.crt],
		scaler_key(options.filter), settings_scaling_names[options.scaling], options.smooth ? "on" : "off",
		settings_scanline_names[options.scanlines & 3], settings_rotate_names[options.rotate],
		options.arcade_timing ? "arcade" : "fast", options.slowdown ? "on" : "off", audio ? "on" : "off");
	fputs("each second: GAME the game's frames, BOARD the board's (57.55), WINDOW the pictures shown, LATE the frames"
		" the PC ran late, WORK a frame's ms (average / most); then each part's ms a frame (average / most)\n",
		meter_log);
	fflush(meter_log);
}

static void meter_log_second(double seconds)
{
	const double ms = 1000.0 / (double)SDL_GetPerformanceFrequency();
	static const char *const names[SPENT_PARTS] = { "run", "look", "draw", "show", "present" };
	const unsigned windows = meter.windows ? meter.windows : 1u;
	char when[16];
	const time_t now = time(NULL);

	if (!meter_log)
		meter_log_open();
	if (!meter_log)
		return;
	strftime(when, sizeof(when), "%H:%M:%S", localtime(&now));
	fprintf(meter_log, "%s  game %5.1f  board %5.1f  window %5.1f  late %2u  work %5.2f/%5.2f |", when,
		meter.game_frames / seconds, meter.board_frames / seconds, meter.windows / seconds, meter.late,
		(double)meter.work * ms / windows, (double)meter.work_most * ms);
	for (int part = 0; part < SPENT_PARTS; part++)
		fprintf(meter_log, "  %s %5.2f/%5.2f", names[part], (double)spent[part] * ms / windows,
			(double)spent_most[part] * ms);
	fputc('\n', meter_log);
	fflush(meter_log);
}

/* The picture with the hitboxes and the aura over it, when they are on (hitbox.h): those of the frame whose sprites
   the picture has, two frames back (three with arcade timing: the picture drawn as the next frame's interrupt
   returns) */
static const uint32_t *with_overlays(const uint32_t *pixels)
{
	static uint32_t marked[VIDEO_WIDTH * VIDEO_HEIGHT];

	if (!options.hitbox && !options.aura)
		return pixels;
	memcpy(marked, pixels, sizeof(marked));
	hitbox_draw(marked, options.arcade_timing ? 3 : 2, options.hitbox, options.aura);
	return marked;
}

/* The picture with the meter over its corner, when it is on */
static const uint32_t *with_meter(const uint32_t *pixels)
{
	static uint32_t out[VIDEO_WIDTH * VIDEO_HEIGHT];
	const char *const lines[3] = { meter.lines[0], meter.lines[1], meter.lines[2] };

	if (!options.show_fps || !meter.lines[0][0])
		return pixels;
	memcpy(out, pixels, sizeof(out));
	menu_put_meter(out, lines, 3);
	return out;
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
	crash_log_not_replayable("a save state was put back");
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
	widgets_reset();
	training_on = false;
	state_loaded = false;
	crash_log_reset(options.slowdown, rom_set.version, options.save_high_scores ? hiscore_table() : NULL);
	replay_reset();
	game_watch_reset();
	menu_notice("RESET");
}

/* The options' MAIN MENU: the board reset and kept at its start (it is not run while the main menu is up), the side
   panels' game forgotten; START GAME runs it from there, as the board switched on */
static bool ask_replay, replay_over;    /* (a game over: the question; a replay at its end: stopped) */
static bool replay_has_table;           /* the replay playing: its high score table put back after the boot */
static uint16_t own_eeprom[CAVE_EEPROM_WORDS];    /* the player's own, while a replay plays with its own */

/* a replay playing stopped: the player's EEPROM, slowdown and high score file back (the caller resets the board) */
static void replay_restore(void)
{
	if (!replay_playing())
		return;
	replay_stop();
	memcpy(cave_board.eeprom.words, own_eeprom, sizeof(own_eeprom));
	program_set_slowdown(options.slowdown);
	hiscores_open();    /* (the file's table, and its saving, back) */
}

static void menu_main_menu(void)
{
	replay_restore();
	program_reset();
	reset_asked = true;
	service_held = 0;
	free_play_reset();
	hiscore_reset();
	widgets_reset();
	training_on = false;
	state_loaded = false;
	crash_log_reset(options.slowdown, rom_set.version, options.save_high_scores ? hiscore_table() : NULL);
	replay_reset();
	game_watch_reset();
	memset(picture, 0, sizeof(picture));
}

/* ---- training (training.h): the game run to a place of the player's choosing, a save state there ---- */

static program_state *training_state;    /* the place: RETRY TRAINING, and a miss with retry_on_miss, go back to it */
static int training_lives_seen;           /* player 1's lives as last seen (a miss: fewer) */
static int training_retry_frames;         /* a miss seen: frames until the game goes back to the place */

/* the game as it is, shown and heard from here (a state put back, or the place reached) */
static void training_shown(void)
{
	free_play_reset();
	video_latch_sprites();
	draw_picture();
	if (audio)
		audio_restart();
	cave_sound_drop();
	training_lives_seen = training_lives();
	training_retry_frames = 0;
}

static bool menu_training(void)
{
	const training_setup setup = { options.training_stage, options.training_point, options.training_ship,
		options.training_laser, options.training_power, options.training_max_bombs };
	const char *problem;

	menu_main_menu();    /* (the board reset: training_prepare runs it from there) */
	problem = training_prepare(&setup);
	if (problem)
	{
		fprintf(stderr, "training, %s: %s\n", training_name(&setup), problem);
		menu_main_menu();
		return false;
	}
	program_state_free(training_state);
	training_state = program_state_save();
	training_on = true;
	state_loaded = true;    /* (no high score or stage best from a training game) */
	widgets_game_begun();
	crash_log_not_replayable("a training game (the game run to its place unrecorded)");
	reset_asked = false;    /* (the reset came with the way there) */
	training_shown();
	menu_notice(training_name(&setup));
	return true;
}

static bool menu_in_training(void)
{
	return training_on;
}

static void menu_retry_training(void)
{
	if (!training_state)
		return;
	program_state_load(training_state);
	hiscore_state_loaded();
	training_shown();
	menu_notice("RETRY");
}

/* ---- replays (replay.h): each arcade game recorded; saved at its end if the player says yes; played back ---- */


static void menu_keep_replay(bool keep)
{
	const char *const name = replay_keep(keep);

	if (keep)
		menu_notice(name ? "REPLAY SAVED" : "THE REPLAY COULD NOT BE SAVED");
}

/* A replay played: the board reset to its start (its EEPROM, slowdown and high score table; the player's own kept to
   put back), the frames before its game run at once, unseen; then it plays, a frame at a time */
static bool menu_play_replay(const char *file)
{
	replay_start start;
	const char *problem;
	uint16_t in0, in1;

	memcpy(own_eeprom, cave_board.eeprom.words, sizeof(own_eeprom));
	if ((problem = replay_play(file, &start)) != NULL)
	{
		fprintf(stderr, "replay %s: %s\n", file, problem);
		menu_notice("THE REPLAY COULD NOT BE PLAYED");
		return false;
	}
	program_reset();
	reset_asked = true;
	service_held = 0;
	free_play_reset();
	widgets_reset();
	training_on = false;
	state_loaded = false;
	memcpy(cave_board.eeprom.words, start.eeprom, sizeof(start.eeprom));
	program_set_slowdown(start.slowdown);
	replay_has_table = start.has_table;
	hiscore_use_table(start.has_table ? start.table : NULL);
	hiscore_reset();
	crash_log_reset(start.slowdown, rom_set.version, start.has_table ? start.table : NULL);
	replay_reset();
	game_watch_reset();
	while (replay_position() < start.game_begins && replay_next(&cave_board.in0, &cave_board.in1))
	{
		crash_log_frame();
		program_run_frame();
		cave_sound_drop();
		widgets_frame(false);
		hiscore_frame(replay_has_table, false);
		replay_after_playback_frame();
		reset_asked = false;
	}
	(void)in0;
	(void)in1;
	video_latch_sprites();
	draw_picture();
	if (audio)
		audio_restart();
	return true;
}

static bool menu_in_replay(void)
{
	return replay_playing();
}

/* A replay stopped (its end, STOP REPLAY, a reset): the player's EEPROM, slowdown and high score file back, the board
   reset, the main menu's replays page up */
static void replay_end(void)
{
	if (!replay_playing())
		return;
	menu_main_menu();    /* (replay_restore first) */
	menu_open_replays();
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

/* ---- the update (updater.h) ---- */

/* the releases' repository on GitHub, and the end of the package's name for this system (tools/make_release) */
#ifndef UPDATE_REPOSITORY
#define UPDATE_REPOSITORY "CapsLock4284/DoDonPachi-Recomp"
#endif
#ifdef _WIN32
#define UPDATE_PACKAGE "-win64.zip"
#else
#define UPDATE_PACKAGE "-linux-x64.tar.gz"
#endif

static bool update_restart;             /* installed: the new program started as this one closes */

/* the update downloaded: its files put in place and the program closed to start again (or the menu says why not) */
static void menu_install_update(void)
{
	stats_save();
	if (updater_install())
	{
		update_restart = true;
		running = 0;
	}
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
		crash_log_start(log_path, paths_folder());
		/* (the replays: their folder beside the program) */
#ifdef _WIN32
		snprintf(log_path, sizeof(log_path), "%sreplays\\", paths_folder());
#else
		snprintf(log_path, sizeof(log_path), "%sreplays/", paths_folder());
#endif
		replay_folder(log_path);
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
	{
		/* the files an update left (.old) deleted; the newest release looked up (on its thread: the game goes on) */
		char *const base = SDL_GetBasePath();

		if (base)
		{
			updater_cleanup(base);
#ifdef DDONPACH_VERSION
			if (options.check_updates)
				updater_check(base, UPDATE_REPOSITORY, UPDATE_PACKAGE, DDONPACH_VERSION);
#endif
			SDL_free(base);
		}
	}
	/* (the run's inputs kept from its first frame) */
	crash_log_reset(options.slowdown, rom_set.version, options.save_high_scores ? hiscore_table() : NULL);
	replay_reset();
	game_watch_reset();
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
	{
		/* the one asked for (SDL falls back to another if it can't make it). auto: on Windows Direct3D 12, which
		   PresentMon showed reaching the screen on its own ("independent flip", 1.4 ms after the present on the
		   owner's PC); SDL's own choice there, Direct3D 9, goes through the compositor (27 ms) */
		const char *driver = options.renderer > 0 ? settings_renderer_names[options.renderer] : NULL;
#ifdef _WIN32
		if (!driver)
			driver = "direct3d12";
#endif
		if (driver)
			SDL_SetHint(SDL_HINT_RENDER_DRIVER, driver);
	}
	renderer = window ? SDL_CreateRenderer(window, -1,
		options.renderer == RENDERERS - 1 ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED) : NULL;
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
	for (int side = 0; side < 2; side++)
		if (!(panel_textures[side] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
				WIDGET_MAX_W, WIDGET_H)))
			fprintf(stderr, "side panels: %s\n", SDL_GetError());    /* (their place left black then) */
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
		menu.main_menu = menu_main_menu;
		menu.training = menu_training;
		menu.in_training = menu_in_training;
		menu.retry_training = menu_retry_training;
		menu.keep_replay = menu_keep_replay;
		menu.play_replay = menu_play_replay;
		menu.in_replay = menu_in_replay;
		menu.stop_replay = replay_end;
		menu.install_update = menu_install_update;
		menu_init(&menu);
		if (options.main_menu)
			menu_open_front();    /* (the main menu first: the game waits at its start until START GAME) */
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
		const Uint64 frame_began = SDL_GetPerformanceCounter();

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
					&& event.key.keysym.sym == SDLK_F9)
				options.show_fps = !options.show_fps;    /* (the FPS meter: for this run; the menu's is kept) */
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
		if (program_slowdown() != options.slowdown && !replay_playing())
		{
			program_set_slowdown(options.slowdown);    /* (changed in the menu) */
			crash_log_not_replayable("the slowdown setting changed");
		}
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
				const Uint64 run_began = SDL_GetPerformanceCounter();

				if (replay_playing())
				{
					/* (a replay: its inputs; over at its end) */
					if (!replay_next(&cave_board.in0, &cave_board.in1))
					{
						replay_over = true;
						break;
					}
				}
				else
					read_keys();
				picture_wanted = options.arcade_timing && count == frames - 1;
				crash_log_frame();
				program_run_frame();
				meter_board_frame();
				widgets_frame(!state_loaded && !cheats_any_on() && options.slowdown && !replay_playing());
				game_watch_frame(!state_loaded && !cheats_any_on() && options.slowdown && !replay_playing());
				achievements_frame(true);
				stats_frame();
				if (!replay_playing())
					cheats_apply();    /* (a replay is its inputs alone) */
				if (fast || skip)
					cave_sound_drop();
				else
					audio_queue();
				video_latch_sprites();    /* (VBLANK: the 013 takes the sprites) */
				hitbox_record();
				reset_asked = false;      /* (done: the frame began from the reset) */
				if (cheats_any_on())
				{
					state_loaded = true;    /* (an assist: the game doesn't count) */
					crash_log_not_replayable("a cheat was on");
				}
				if (replay_playing())
				{
					hiscore_frame(replay_has_table, false);    /* (the table as the recording had it put back) */
					if (!replay_after_playback_frame())
						menu_notice("THE REPLAY IS OUT OF STEP");
				}
				else
				{
					hiscore_frame(options.save_high_scores, !state_loaded);
					if (state_loaded && !cheats_any_on() && hiscore_between_games())
					{
						state_loaded = false;      /* (a new game: its scores are kept again) */
						hiscore_game_not_kept();    /* (the table as the file has it: none of that game's scores) */
					}
					if (replay_after_frame())
					{
						ask_replay = true;    /* (a game over: its replay kept or not) */
						break;
					}
				}
				spend(SPENT_RUN, run_began);
			}
			if (!widgets_toast_busy())
			{
				/* an achievement earned: shown over the right panel (or as a notice, the panels off) */
				const int more = achievements_waiting() - 1;
				const char *const name = achievements_next_shown();

				if (name && panels_shown())
					widgets_toast(name, more);
				else if (name)
				{
					char text[48];

					snprintf(text, sizeof(text), "ACHIEVEMENT: %s", name);
					menu_notice(text);
				}
			}
			if (replay_over)
			{
				replay_over = false;
				replay_end();
				menu_notice("END OF THE REPLAY");
			}
			else if (ask_replay)
			{
				ask_replay = false;
				menu_ask_save_replay();
			}
			if (training_on && options.training_retry)
			{
				/* RETRY ON MISS: a life lost, the place again a second later (the miss seen first) */
				const int lives = training_lives();

				if (lives < training_lives_seen && training_retry_frames == 0)
					training_retry_frames = 58;
				training_lives_seen = lives;
				if (training_retry_frames > 0 && --training_retry_frames == 0)
					menu_retry_training();
			}
			{
				const Uint64 look_began = SDL_GetPerformanceCounter();

				look_ahead();    /* (the picture the board shows now) */
				spend(SPENT_LOOK_AHEAD, look_began);
			}
		}
		{
			const Uint64 show_began = SDL_GetPerformanceCounter();

			show_picture(with_meter(menu_with_notice(with_overlays(picture))));
			spend(SPENT_SHOW, show_began);
		}
pace:
		meter_frame_end(frame_began, next);

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
	stats_save();
	crash_log_finish();
	eeprom_keep(true);
	for (int slot = 0; slot < STATE_SLOTS; slot++)
		program_state_free(slot_states[slot]);
	if (audio)
		SDL_CloseAudioDevice(audio);
	scaler_quit();
	SDL_Quit();
	if (update_restart && !updater_restart(argv))
		fprintf(stderr, "updated: the new program could not be started, start it again\n");
	return 0;
}
