/**
 * Functions that sit between a Dirview's WINDOW and its other members
 */
#ifndef UI_H
#define UI_H

#include "backend.h"
#include "tabs.h"

#define TABNAME_MAX 12
#define MAXDATELEN 18
#define HUMANSIZE_LEN 6

typedef struct {
	WINDOW *win;
	PaneCtx *ctx;
} Dirview;

int  check_offset_changed(Dirview *view);
int  render_tree(Dirview *win, int show_sizes);
int  tab_switch(Dirview view[WIN_NR], const TabCtx *ctx);
int  try_highlight(Dirview *view, int idx);
void update_status_bottom(Dirview *win);
void update_status_top(Dirview *win);
int  windows_deinit(Dirview view[WIN_NR]);
int  windows_init(Dirview view[WIN_NR], int w, int h, int pp[3]);

#endif
