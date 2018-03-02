/**
 * Functions that sit between a Dirview's WINDOW and its other members
 */
#ifndef UI_H
#define UI_H

#include "backend.h"

typedef struct {
	WINDOW *win;
	PaneCtx *ctx;
} Dirview;

int  check_offset_changed(Dirview *view);
int  refresh_listing(Dirview *win, int show_sizes);
int  tab_switch(Dirview view[WIN_NR], const TabCtx *ctx, const TabCtx *head);
int  try_highlight(Dirview *view, int idx);
void update_status_bottom(Dirview *win);
void update_status_top(Dirview *win, const TabCtx *ctx);
int  windows_deinit(Dirview view[WIN_NR]);
int  windows_init(Dirview view[WIN_NR], int w, int h, float main_perc);

#endif
