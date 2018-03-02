/**
 * All the functions dealing with PaneCtx structs are defined here.
 */
#ifndef BACKEND_H
#define BACKEND_H

#include <ncurses.h>
#include "dir.h"

#define WIN_NR 5

#define MAXHOSTNLEN 32

typedef struct {
	Direntry *dir;
	int offset;
	int visual;
} PaneCtx;

int  associate_dir(PaneCtx *ctx, Direntry *direntry);
int  init_pane_with_path(PaneCtx *ctx, const char *path);
int  navigate_fwd(PaneCtx *left, PaneCtx *center, PaneCtx *right);
int  navigate_back(PaneCtx *left, PaneCtx *center, PaneCtx *right);
int  panectx_free(PaneCtx *ctx);
int  recheck_offset(PaneCtx *ctx, int nr);
int  rescan_pane(PaneCtx *ctx);

#endif
