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
int  navigate_fwd(Dirview *left, Dirview *center, Dirview *right);
int  navigate_back(Dirview *left, Dirview *center, Dirview *right);
int  rescan_win(Dirview *win);
int  update_win_with_path(Dirview *win, const char *path);

#endif
