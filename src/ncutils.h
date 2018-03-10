/**
 * All the helper functions that deal directly with an ncurses window, so
 * refreshing it, clearing it, initializing colors, and the like.
 */
#ifndef NCUTILS_H
#define NCUTILS_H

#include "backend.h"

#define MAXCMDLEN 128
#define UPD_INTERVAL 100

/* Convenient enum to address a specific view in main_view */
enum windows {
	TOP = 0,
	LEFT = 1,
	CENTER = 2,
	RIGHT = 3,
	BOT = 4
};

/* Color pairs */
enum color_pairs {
	PAIR_BLUE_DEF,
	PAIR_GREEN_DEF,
	PAIR_WHITE_DEF,
	PAIR_CYAN_DEF,
	PAIR_YELLOW_DEF,
	PAIR_RED_DEF
};

void change_highlight(WINDOW* win, int old_idx, int new_idx);
void dialog(WINDOW *win, char *buf, const char *msg, ...);
void init_colors();

#endif
