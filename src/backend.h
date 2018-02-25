/**
 * All the functions dealing with Dirview structs are defined here.
 */
#ifndef BACKEND_H
#define BACKEND_H

#include <ncurses.h>
#include "dir.h"

#define WIN_NR 5

#define MAXHOSTNLEN 32

typedef struct {
	WINDOW *win;            /* Ncurses window being managed */
	Direntry *dir;          /* Directory associated with the view */
	int offset;	            /* Offset from the beginning of the screen */
	int visual;
} Dirview;

int  associate_dir(Dirview *view, Direntry *direntry);
int  check_offset_changed(Dirview *view);
int  deinit_windows(Dirview views[WIN_NR]);
int  init_windows(Dirview view[WIN_NR], int w, int h, float main_perc);
int  navigate_fwd(Dirview *left, Dirview *center, Dirview *right);
int  navigate_back(Dirview *left, Dirview *center, Dirview *right);
int  try_highlight(Dirview *view, int idx);
void update_status_bottom(Dirview *win);
void update_status_top(Dirview *win);
int  update_win_with_path(Dirview *win, const char *path);

#endif
