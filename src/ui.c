#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "backend.h"
#include "ncutils.h"
#include "ui.h"
#include "utils.h"

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

/* Render a directory listing on a window. If the directoly listing is NULL,
 * clear the relative window */
int
refresh_listing(Dirview *win, int show_sizes)
{
	int mr, mc, i;
	char *tmpstring;
	char humansize[HUMANSIZE_LEN+1];
	Fileentry* tmpfile;

	assert(win->win);

	if (!win->dir || !win->dir->path) {
		werase(win->win);
		wrefresh(win->win);
		return 1;
	}

	assert(win->dir->tree);

	getmaxyx(win->win, mr, mc);

	/* Go to the top corner */
	werase(win->win);
	wmove(win->win, 0, 0);

	/* Allocate enough space to fit the shortened listing names */
	tmpstring = safealloc(sizeof(*tmpstring) * (mc + 1));

	/* Update window offsets if necessary */
	check_offset_changed(win);

	/* Read up to $mr entries */
	for (i = win->offset; i < win->dir->count && (i - win->offset) < mr; i++) {
		tmpfile = win->dir->tree[i];

		if (tmpfile->selected) {
			wattrset(win->win, COLOR_PAIR(PAIR_YELLOW_DEF) | A_BOLD);
		} else {
			/* Change color based on the entry type */
			switch (tmpfile->mode & S_IFMT) {
			case 0:                 /* Not a file */
				wattrset(win->win, COLOR_PAIR(PAIR_RED_DEF));
				break;
			case S_IFLNK:
				wattrset(win->win, COLOR_PAIR(PAIR_CYAN_DEF));
				break;
			case S_IFDIR:
				wattrset(win->win, A_BOLD | COLOR_PAIR(PAIR_GREEN_DEF));
				break;
			case S_IFBLK:           /* All intentional fallthroughs */
			case S_IFIFO:
			case S_IFSOCK:
			case S_IFCHR:
				wattrset(win->win, COLOR_PAIR(PAIR_YELLOW_DEF));
				break;
			case S_IFREG:
			default:
				wattrset(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
				break;
			}
		}
		/* Chomp string so that it fits in the window */
		if (!show_sizes || tmpfile->size < 0) {
			if (strchomp(tmpfile->name, tmpstring, mc))
				die("We do not kno de wey (read: strchomp failed)");
			wprintw(win->win, "%s\n", tmpstring);
		} else {
			/* Convert byte count to human-readable size */
			tohuman(tmpfile->size, humansize);
			strchomp(tmpfile->name, tmpstring, mc - HUMANSIZE_LEN - 1);

			wprintw(win->win, "%s", tmpstring);
			mvwprintw(win->win, i - win->offset, mc - HUMANSIZE_LEN - 1,
			          "%6s\n", humansize);
		}
		/* Higlight the element marked as selected in the dir tree */
		if (i == win->dir->sel_idx) {
			try_highlight(win, i - win->offset);
		}
	}

	free(tmpstring);
	wrefresh(win->win);
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

/* Deinitialize windows, right before exiting. This deallocates all memory
 * dedicated to Fileentry* arrays and their paths as well */
int
windows_deinit(Dirview view[WIN_NR])
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
windows_init(Dirview view[WIN_NR], int row, int col, float main_perc)
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

