#ifndef UI_H
#define UI_H

#include "backend.h"

int  check_offset_changed(Dirview *view);
int  refresh_listing(Dirview *win, int show_sizes);
int  try_highlight(Dirview *view, int idx);
void update_status_bottom(Dirview *win);
void update_status_top(Dirview *win);
int  windows_deinit(Dirview views[WIN_NR]);
int  windows_init(Dirview view[WIN_NR], int w, int h, float main_perc);

#endif
