#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "backend.h"
#include "dir.h"
#include "utils.h"
#include "ncutils.h"

/* Associate a Direntry struct with a Dirview */
int
associate_dir(Dirview *view, Direntry *direntry)
{
	assert(view);
	assert(direntry);

	view->dir = direntry;
	return 0;
}

/* Update window offset if needed, return 1 if we did change offset,
 * 0 otherwise */
int
check_offset_changed(Dirview *win)
{
	int nr;
	nr = getmaxy(win->win);

	if (win->dir->sel_idx - win->offset >= nr) {
		win->offset = (win->dir->sel_idx - nr + 1);
		return 1;
	} else if (win->dir->sel_idx - win->offset < 0) {
		win->offset = win->dir->sel_idx;
		return 1;
	}
	return 0;
}

/* Deinitialize windows, right before exiting. This deallocates all memory
 * dedicated to Fileentry* arrays and their paths as well */
int
deinit_windows(Dirview view[WIN_NR])
{
	int i;

	assert(view);
	/* Deallocate directory listings. Top and bottom windows
	 * inherit the CENTER path, so there's no need to free those two */
	for (i=0; i<WIN_NR; i++) {
		if (view[i].dir && i != TOP && i != BOT) {
			assert(!free_listing(&view[i].dir));
		}
		delwin(view[i].win);
	}

	return 0;
}

/* Initialize the sub-windows that make up the main view */
int
init_windows(Dirview view[WIN_NR], int row, int col, float main_perc)
{
	int i, mc, sc_l, sc_r;

	if (!view) {
		return -1;
	}

	/* Calculate center window and sidebars column count */
	mc = col  *main_perc;
	sc_r = (col - mc) / 2;
	sc_l = col - mc - sc_r;

	view[TOP].win = newwin(1, col, 0, 0);
	view[BOT].win = newwin(1, col, row - 1, 0);
	view[LEFT].win = newwin(row - 2, sc_l - 1, 1, 0);
	view[CENTER].win = newwin(row - 2, mc - 1, 1, sc_l);
	view[RIGHT].win = newwin(row - 2, sc_r, 1, sc_l + mc);

	/* Initialize the view offsets */
	for (i=0; i<WIN_NR; i++) {
		view[i].offset = 0;
	}

	return 0;
}

/* Navigate back out of a directory, updating the Direntries */
int
navigate_back(Dirview *left, Dirview *center, Dirview *right)
{
	Direntry *tmpdir;
	char *fullpath;

	/* Rotate right by one the allocated directories */
	tmpdir = right->dir;
	right->dir = center->dir;
	center->dir = left->dir;
	left->dir = tmpdir;

	right->offset = center->offset;
	center->offset = left->offset;
	left->offset = 0;

	assert(center->dir->path);
	fullpath = join_path(center->dir->path, "../");

	/* Init left pane with parent directory contents */
	update_win_with_path(left, fullpath);
	free(fullpath);

	return 0;
}

/* Navigate forward in the directory listing */
int
navigate_fwd(Dirview *left, Dirview *center, Dirview *right)
{
	Fileentry* centersel;
	Direntry *tmpdir;
	char *fullpath;

	centersel = center->dir->tree[center->dir->sel_idx];
	if (!S_ISDIR(centersel->mode)) {
		return -1;
	}

	/* Rotate left by one the allocated directories */
	tmpdir = left->dir;
	left->dir = center->dir;
	center->dir = right->dir;
	right->dir = tmpdir;

	left->offset = center->offset;
	center->offset = right->offset;
	right->offset = 0;

	assert(center->dir->path);
	centersel = center->dir->tree[center->dir->sel_idx];
	fullpath = join_path(center->dir->path, centersel->name);

	/* Init right pane with child directory contents */
	update_win_with_path(right, fullpath);
	free(fullpath);

	return 0;
}

/* Try to highlight the idxth line, deselecting the previous one. Returns the
 * number of the line that was actually selected */
int
try_highlight(Dirview *win, int idx)
{
	int row_nr;

	assert(win);

	row_nr = win->dir->sel_idx - win->offset;
	/* Update the dir backend */
	idx = try_select(win->dir, idx + win->offset, win->visual) - win->offset;
	/* Update the ncurses frontend */
	change_highlight(win->win, row_nr, idx);
	return idx;
}


void
update_status_bottom(Dirview *win)
{
	char last_mod[MAXDATELEN+1];
	struct tm *mtime;
	const Fileentry *sel;

	sel = win->dir->tree[win->dir->sel_idx];

	mtime = localtime(&sel->lastchange);
	strftime(last_mod, MAXDATELEN, "%F %R", mtime);
	print_status_bottom(win->win, sel->mode, mtime, sel->uid, sel->gid);
}

void
update_status_top(Dirview *win)
{
	char *user, *wd, *hi;
	char hostn[MAXHOSTNLEN+1];

	user = getenv("USER");
	wd = win->dir->path;
	gethostname(hostn, MAXHOSTNLEN);
	hi = win->dir->tree[win->dir->sel_idx]->name;

	assert(user && wd && win->win);
	print_status_top(win->win, user, wd, hostn, hi);
	return;
}

/* Update a window with a given path, which can also be NULL. In that case, the
 * window passed as an argument is initialized empty. XXX this funcion used to
 * be a bit more complex, now it's just here in case that complexity arises in
 * the future */
int
update_win_with_path(Dirview *win, const char *path)
{
	init_listing(&(win->dir), path);
	return 0;
}
