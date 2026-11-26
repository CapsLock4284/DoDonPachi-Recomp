/**
 *    Name:        rom_drop.c
 *    Description: the first run's window when there is no game data and no ROM set (rom_drop.h)
 *    Version:     0.1.0 (draft)
 *    Created:     10-11-2026
 *
 *    license:BSD-3-Clause
 *    The font is this program's own (font5x7.h). DonPachi Recomp's, the game's names from its board description.
 */
#include "rom_drop.h"
#include "settings.h"

#include "cave/board.h"
#include "cave/rom_set.h"
#include "window_icon.h"

#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define WINDOW_W    640
#define WINDOW_H    400

#define BACKGROUND  0x0C0C1Cu
#define GOLD        0xF0C040u
#define WHITE       0xF0F0F0u
#define GREY        0x9090A8u
#define RED         0xFF4848u
#define GREEN       0x60E070u

#include "font5x7.h"

static void set_colour(SDL_Renderer *renderer, uint32_t colour)
{
	SDL_SetRenderDrawColor(renderer, colour >> 16 & 255u, colour >> 8 & 255u, colour & 255u, 255);
}

/* 'text' centred at 'y', each of its pixels 'scale' x 'scale' (a character 6 pixels wide, with its space) */
static void text_centred(SDL_Renderer *renderer, int y, const char *text, int scale, uint32_t colour)
{
	const int width = (int)strlen(text) * 6 * scale - scale;
	int x = (WINDOW_W - width) / 2;

	set_colour(renderer, colour);
	for (const char *at = text; *at; at++, x += 6 * scale)
	{
		const glyph *const shape = glyph_of(*at);

		if (!shape)
			continue;
		for (int row = 0; row < 7; row++)
			for (int column = 0; column < 5; column++)
				if (shape->rows[row][column] == '#')
				{
					const SDL_Rect pixel = { x + column * scale, y + row * scale, scale, scale };

					SDL_RenderFillRect(renderer, &pixel);
				}
	}
}

/* 'text' cut to what fits the window at 'scale', with "..." where it is cut */
static void text_fitted(SDL_Renderer *renderer, int y, const char *text, int scale, uint32_t colour)
{
	const size_t most = (size_t)((WINDOW_W - 32) / (6 * scale));
	char line[160];

	if (strlen(text) <= most || most < 4u || most >= sizeof(line))
	{
		text_centred(renderer, y, text, scale, colour);
		return;
	}
	memcpy(line, text, most - 3u);
	memcpy(line + most - 3u, "...", 4u);
	text_centred(renderer, y, line, scale, colour);
}

/* What is wrong with a set that was read but is not whole, as up to four lines ('\n' between them): the files it
   lacks (or has another version of). No file of the set at all: not the DonPachi set */
static int asked;    /* the version the window asks for (the board description's: 0 the parent set) */

/* the version's set's file name, upper case ("DDONPACH.ZIP") */
static const char *zip_name(void)
{
	static char name[48];

	snprintf(name, sizeof(name), "%s.ZIP", rom_set_mame_name(asked));
	for (char *at = name; *at; at++)
		*at = (char)toupper((unsigned char)*at);
	return name;
}

static void describe_missing(char *out, size_t size)
{
	const size_t most = (size_t)((WINDOW_W - 32) / 12);    /* (the letters a line has at scale 2) */
	const char *names[ROM_SET_FILES_MOST];
	bool different[ROM_SET_FILES_MOST];
	const int missing = rom_set_files_missing(names, different, ROM_SET_FILES_MOST);
	char list[2][64] = { "", "" };
	int line = 0, shown = 0;

	if (rom_set_files_found() == 0 || missing == 0)
	{
		snprintf(out, size, "NOT THE %s SET (%s VERSION). TRY ANOTHER.", zip_name(), rom_set_version_name(asked));
		return;
	}
	/* the names on up to two lines, each "NAME" or "NAME (WRONG ONE)", a comma between */
	for (; shown < missing; shown++)
	{
		char name[40];

		snprintf(name, sizeof(name), "%s%s", names[shown], different[shown] ? " (WRONG VERSION)" : "");
		if (list[line][0] && strlen(list[line]) + 2 + strlen(name) > most - 9)    /* (room kept for ", +9 MORE") */
		{
			if (line == 1)
				break;
			strcat(list[line], ",");
			line++;
		}
		if (list[line][0])
			strcat(list[line], ", ");
		strcat(list[line], name);
	}
	if (shown < missing)
		snprintf(list[line] + strlen(list[line]), sizeof(list[line]) - strlen(list[line]), ", +%d MORE",
			missing - shown);
	snprintf(out, size, "THE SET IS MISSING %d OF ITS %d FILES:\n%s%s%s\nA COMPLETE %s FROM MAME HAS THEM ALL.",
		missing, rom_set_files(), list[0], line ? "\n" : "", line ? list[1] : "", zip_name());
}

static bool japan_optional;    /* the first run's second step: the World set read, ddonpachj.zip asked for, skippable */

static void draw(SDL_Renderer *renderer, const char *status, uint32_t status_colour, const char *why)
{
	set_colour(renderer, BACKGROUND);
	SDL_RenderClear(renderer);
	char line[96];

	text_centred(renderer, 70 + 4, "DODONPACHI", 7, 0x301000u);    /* (its shadow) */
	text_centred(renderer, 70, "DODONPACHI", 7, GOLD);
	if (japan_optional)
	{
		text_centred(renderer, 150, "WORLD VERSION READY", 3, GREEN);
		text_centred(renderer, 190, "NOW DROP THE JAPANESE SET TOO (IF YOU HAVE IT)", 2, WHITE);
		text_centred(renderer, 214, "DDONPACHJ.ZIP: MAME'S SET, JAPAN VERSION", 2, GREY);
		text_centred(renderer, 238, "PRESS ENTER OR CLICK TO SKIP: YOU CAN ADD IT LATER.", 2, GREY);
	}
	else
	{
		text_centred(renderer, 170, asked ? "DROP THE JAPANESE ROM SET HERE" : "DROP THE ARCADE ROM SET HERE", 3,
			WHITE);
		snprintf(line, sizeof(line), "%s: MAME'S SET, %s VERSION", zip_name(), rom_set_version_name(asked));
		text_centred(renderer, 210, line, 2, GREY);
		text_centred(renderer, 232, "THE GAME TAKES ITS DATA FROM IT, ONCE.", 2, GREY);
	}
	if (status)
	{
		/* (its lines: the first in the status's colour, the next ones white, a last hint grey) */
		char copy[400];
		int y = 270, line = 0;

		snprintf(copy, sizeof(copy), "%s", status);
		for (char *at = copy, *next; at; at = next, line++, y += 24)
		{
			if ((next = strchr(at, '\n')) != NULL)
				*next++ = '\0';
			text_fitted(renderer, line == 0 && !next ? 290 : y, at, 2,
				line == 0 ? status_colour : next ? WHITE : GREY);
		}
	}
	else if (why)
		text_fitted(renderer, 360, why, 1, 0x606078u);
	SDL_RenderPresent(renderer);
}

bool rom_drop_screen(const char *data_path, const char *why, bool *saved)
{
	return rom_drop_screen_version(data_path, why, saved, 0);
}

/* the dropped file or folder read as the version's set: a clone whole, or over its parent's set loaded */
static const char *read_dropped(const char *path)
{
	const char *problem;
	const bool over = asked != 0 && rom_set.program && rom_set.version == 0;

	if ((problem = rom_set_load_version(path, asked)) == NULL || !over)
		return problem;
	return rom_set_load_clone_over(path, asked);
}

bool rom_drop_japan_too(const char *source, const char *world_data_path, const char *japan_data_path)
{
	/* where ddonpachj's chips can be: the source itself (a merged ddonpach.zip, or a folder with ddonpachj's files or
	   ddonpachj.zip), a merged ddonpach.zip in it (a folder), and the same for the folder the source is in */
	char folder[1024], places[4][1100];
	const char *problem = "no Japanese set";
	bool saved = false;

	snprintf(folder, sizeof(folder), "%s", source);
	{
		char *const slash = strrchr(folder, '/'), *const backslash = strrchr(folder, '\\');
		char *const cut = slash > backslash ? slash : backslash;

		if (cut)
			*cut = '\0';
		else
			snprintf(folder, sizeof(folder), ".");
	}
	snprintf(places[0], sizeof(places[0]), "%s", source);
	snprintf(places[1], sizeof(places[1]), "%s/%s.zip", source, rom_set_mame_name(0));
	snprintf(places[2], sizeof(places[2]), "%s", folder);
	snprintf(places[3], sizeof(places[3]), "%s/%s.zip", folder, rom_set_mame_name(0));
	for (int place = 0; place < 4 && problem; place++)
	{
		/* (the World set as it was before each try: a try that fails may have changed some of it) */
		if (place > 0 && rom_set_load_data_version(world_data_path, 0) != NULL)
			break;
		if (rom_set.program && rom_set.version == 0)
			problem = rom_set_load_clone_over(places[place], 1);
	}
	if (problem == NULL)
	{
		const char *const save_problem = rom_set_save_data(japan_data_path);

		saved = save_problem == NULL;
		if (saved)
			fprintf(stderr, "the Japanese version's data extracted too, into %s\n", japan_data_path);
		else
			fprintf(stderr, "the Japanese version's data was not saved (%s)\n", save_problem);
	}
	else
		fprintf(stderr, "no Japanese set with it (%s): the Japanese version is asked for when chosen\n", problem);
	/* (the World set loaded again, for the game to start with) */
	if (rom_set_load_data_version(world_data_path, 0) != NULL)
		rom_set_load_version(source, 0);
	return saved;
}

/* the window; 'dropped' (if not NULL): the path of the set that was read */
static bool drop_window(const char *data_path, const char *why, bool *saved, int version, char *dropped,
	size_t dropped_size)
{
	SDL_Window *const window = SDL_CreateWindow("DoDonPachi: the ROM set",
		SDL_WINDOWPOS_CENTERED_DISPLAY(settings_monitor_display()),
		SDL_WINDOWPOS_CENTERED_DISPLAY(settings_monitor_display()), WINDOW_W, WINDOW_H, 0);
	SDL_Renderer *renderer;
	char status[400] = "";
	uint32_t status_colour = WHITE;
	bool loaded = false, running = true;

	*saved = false;
	asked = version;
	if (!window)
		return false;
	window_icon_set(window);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (!renderer)
	{
		SDL_DestroyWindow(window);
		return false;
	}
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	if (rom_set_files_found() > 0)
	{
		/* (the set the settings point to was read, but is not whole: what it lacks, from the start) */
		describe_missing(status, sizeof(status));
		status_colour = RED;
	}
	while (running && !loaded)
	{
		SDL_Event event;

		draw(renderer, status[0] ? status : NULL, status_colour, why);
		if (!SDL_WaitEventTimeout(&event, 250))
			continue;
		if (event.type == SDL_QUIT
				|| (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
				|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			running = false;
		else if (event.type == SDL_DROPFILE)
		{
			const char *problem;

			draw(renderer, "READING...", WHITE, NULL);
			if ((problem = read_dropped(event.drop.file)) == NULL)
			{
				const char *const save_problem = rom_set_save_data(data_path);

				if (save_problem)
					fprintf(stderr, "the game's data was not saved: %s\n", save_problem);
				*saved = save_problem == NULL;
				loaded = true;
				if (dropped)
					snprintf(dropped, dropped_size, "%s", event.drop.file);
				draw(renderer, "FOUND IT! STARTING...", GREEN, NULL);
				SDL_Delay(700);
			}
			else
			{
				fprintf(stderr, "%s: %s\n", event.drop.file, problem);
				/* the files the set needs are matched by their CRC32: another game's, or another version's */
				if (strstr(problem, " has no "))
					describe_missing(status, sizeof(status));
				else
					snprintf(status, sizeof(status), "THAT ISN'T A .ZIP OR A FOLDER OF THE SET.");
				status_colour = RED;
			}
			SDL_free(event.drop.file);
		}
	}
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	return loaded;
}

bool rom_drop_screen_version(const char *data_path, const char *why, bool *saved, int version)
{
	return drop_window(data_path, why, saved, version, NULL, 0);
}

bool rom_drop_screen_both(const char *data_path, const char *japan_data_path, const char *why, bool *saved,
	bool *japan_saved)
{
	SDL_Window *window;
	SDL_Renderer *renderer;
	char dropped[1024] = "", status[400] = "";
	uint32_t status_colour = WHITE;
	bool done = false;

	*japan_saved = false;
	/* the World set first, as before (its window: the set dropped, read and saved) */
	if (!drop_window(data_path, why, saved, 0, dropped, sizeof(dropped)))
		return false;
	if (!*saved)
		return true;    /* (no data written: the Japanese one would have nothing to go over next time) */
	/* a merged set (or ddonpachj.zip beside the dropped one): both, at once */
	if ((*japan_saved = rom_drop_japan_too(dropped, data_path, japan_data_path)))
		return true;
	/* else the same window again: ddonpachj.zip, or skip */
	window = SDL_CreateWindow("DoDonPachi: the ROM set", SDL_WINDOWPOS_CENTERED_DISPLAY(settings_monitor_display()),
		SDL_WINDOWPOS_CENTERED_DISPLAY(settings_monitor_display()), WINDOW_W, WINDOW_H, 0);
	if (!window)
		return true;
	window_icon_set(window);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (!renderer)
	{
		SDL_DestroyWindow(window);
		return true;
	}
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	japan_optional = true;
	asked = 1;
	while (!done)
	{
		SDL_Event event;

		draw(renderer, status[0] ? status : NULL, status_colour, NULL);
		if (!SDL_WaitEventTimeout(&event, 250))
			continue;
		if (event.type == SDL_QUIT
				|| (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
				|| event.type == SDL_MOUSEBUTTONDOWN
				|| (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN
					|| event.key.keysym.sym == SDLK_KP_ENTER || event.key.keysym.sym == SDLK_SPACE
					|| event.key.keysym.sym == SDLK_ESCAPE)))
			done = true;    /* (skipped: the World version starts; the Japanese set is asked for when chosen) */
		else if (event.type == SDL_DROPFILE)
		{
			draw(renderer, "READING...", WHITE, NULL);
			if (rom_drop_japan_too(event.drop.file, data_path, japan_data_path))
			{
				*japan_saved = true;
				done = true;
				draw(renderer, "FOUND IT! STARTING...", GREEN, NULL);
				SDL_Delay(700);
			}
			else
			{
				snprintf(status, sizeof(status), "THAT ISN'T DDONPACHJ.ZIP (JAPAN). TRY ANOTHER,\nOR PRESS ENTER TO SKIP.");
				status_colour = RED;
			}
			SDL_free(event.drop.file);
		}
	}
	japan_optional = false;
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	return true;
}
