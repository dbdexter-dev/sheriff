#ifndef BACKEND_H
#define BACKEND_H

#include <ncurses.h>
#include "dir.h"

typedef struct
{
	WINDOW* win;            /* Ncurses window being managed */
	struct direntry* dir;   /* Directory associated with the view */
	int offset;	            /* Offset from the beginning of the screen */
} Dirview;

int navigate_fwd(Dirview* left, Dirview* center, Dirview* right);
int navigate_back(Dirview* left, Dirview* center, Dirview* right);
int associate_dir(Dirview* view, struct direntry* direntry);

#endif
