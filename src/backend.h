/**
 * All the functions dealing with PaneCtx structs are defined here. PaneCtx
 * holds the backend info for a specific pane, so the listing it refers to, the
 * current offset from which to print the entries, and whether visual mode is
 * active for that pane.
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
int  free_pane(PaneCtx *ctx);
void init_pane_with_path(PaneCtx *ctx, const char *path);
int  navigate_fwd(PaneCtx *left, PaneCtx *center, PaneCtx *right);
int  navigate_back(PaneCtx *left, PaneCtx *center, PaneCtx *right);
int  recheck_offset(PaneCtx *ctx, int nr);
int  rescan_pane(PaneCtx *ctx);

#endif
