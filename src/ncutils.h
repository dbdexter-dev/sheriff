/**
 * All the helper functions that deal directly with an ncurses window, so
 * refreshing it, clearing it, initializing colors, and the like.
 */
#ifndef NCUTILS_H
#define NCUTILS_H

#include "backend.h"

#define PAIR_BLUE_DEF 1
#define PAIR_GREEN_DEF 2
#define PAIR_WHITE_DEF 3
#define PAIR_CYAN_DEF 4
#define PAIR_YELLOW_DEF 5
#define PAIR_RED_DEF 6

#define MAXCMDLEN 128
#define UPD_INTERVAL 250

/* Convenient enum to address a specific view in main_view */
enum windows
{
	TOP = 0,
	LEFT = 1,
	CENTER = 2,
	RIGHT = 3,
	BOT = 4
};

void change_highlight(WINDOW* win, int old_idx, int new_idx);
void dialog(WINDOW *win, char *msg, char *buf);
void init_colors();

#endif
