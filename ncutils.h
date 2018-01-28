/**
 * All the helper functions that deal directly with an ncurses window, so
 * refreshing it, clearing it, initializing colors, and the like.
 */
#ifndef NCUTILS_H
#define NCUTILS_H

#include "backend.h"

#define WIN_NR 5

#define PAIR_BLUE_DEF 1
#define PAIR_GREEN_DEF 2
#define PAIR_WHITE_DEF 3
#define PAIR_CYAN_DEF 4
#define PAIR_YELLOW_DEF 5
#define PAIR_RED_DEF 6

#define MAXCMDLEN 128
#define MAXDATELEN 18
#define MAXHOSTNLEN 32
#define HUMANSIZE_LEN 6

/* Convenient enum to address a specific view in main_view */
enum windows
{
	TOP_WIN = 0,
	LEFT_WIN = 1,
	CENTER_WIN = 2,
	RIGHT_WIN = 3,
	BOT_WIN = 4
};

int  check_offset_changed(Dirview* win);
int  deinit_windows(Dirview view[WIN_NR]);
void dialog(Dirview* view, char* msg, char* buf);
void init_colors();
int  init_windows(Dirview view[WIN_NR], int w, int h, float main_perc);
void print_status_bottom(Dirview* win);
void print_status_top(Dirview* win);
int  refresh_listing(Dirview* win, int show_sizes);
int  try_highlight(Dirview* win, int idx);

#endif
