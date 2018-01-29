/**
 * The backend used by sheriff.c, it's the link between the ncurses UI
 * and the direntry structs that represent the directory structure. It
 * contains all the functions that operate on the struct dirview* dir
 * inside a Dirview struct
 */
#ifndef BACKEND_H
#define BACKEND_H

#include <ncurses.h>
#include "dir.h"

typedef struct {
	WINDOW *win;            /* Ncurses window being managed */
	Direntry *dir;   /* Directory associated with the view */
	int offset;	            /* Offset from the beginning of the screen */
} Dirview;

int navigate_fwd(Dirview *left, Dirview *center, Dirview *right);
int navigate_back(Dirview *left, Dirview *center, Dirview *right);
int associate_dir(Dirview *view, Direntry *direntry);
int update_win_with_path(Dirview *win, char *parent, const Fileentry *leaf);

#endif
