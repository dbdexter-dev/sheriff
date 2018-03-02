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

typedef struct tbctx {
	PaneCtx *left, *center, *right;
	struct tbctx *next;
} TabCtx;

int  associate_dir(PaneCtx *ctx, Direntry *direntry);
int  init_pane_with_path(PaneCtx *ctx, const char *path);
int  navigate_fwd(PaneCtx *left, PaneCtx *center, PaneCtx *right);
int  navigate_back(PaneCtx *left, PaneCtx *center, PaneCtx *right);
int  recheck_offset(PaneCtx *ctx, int nr);
int  rescan_pane(PaneCtx *ctx);

int  tabctx_append(TabCtx **ctx, const char *path);
int  tabctx_free(TabCtx **ctx);

#endif
