#include <assert.h>
#include <stdlib.h>
#include <string.h>
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
	return recheck_offset(win->ctx, getmaxy(win->win));
}

/* Render a directory listing on a window. If the directoly listing is NULL,
 * clear the relative window */
int
refresh_listing(Dirview *win, int show_sizes)
{
	int mr, mc, i;
	char *tmpstring;
	char humansize[HUMANSIZE_LEN+1];
	PaneCtx *ctx;
	Fileentry* tmpfile;

	ctx = win->ctx;
	assert(win->win);

	if (!win->ctx->dir || !win->ctx->dir->path) {
		werase(win->win);
		wrefresh(win->win);
		return 1;
	}

	assert(win->ctx->dir->tree);

	getmaxyx(win->win, mr, mc);

	/* Go to the top corner */
	werase(win->win);
	wmove(win->win, 0, 0);

	/* Allocate enough space to fit the shortened listing names */
	tmpstring = safealloc(sizeof(*tmpstring) * (mc + 1));

	/* Update window offsets if necessary */
	check_offset_changed(win);

	/* Read up to $mr entries */
	for (i = ctx->offset; i < ctx->dir->count && (i - ctx->offset) < mr; i++) {
		tmpfile = ctx->dir->tree[i];

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
			mvwprintw(win->win, i - ctx->offset, mc - HUMANSIZE_LEN - 1,
			          "%6s\n", humansize);
		}
		/* Higlight the element marked as selected in the dir tree */
		if (i == ctx->dir->sel_idx) {
			try_highlight(win, i - ctx->offset);
		}
	}

	free(tmpstring);
	wrefresh(win->win);
	return 0;
}

/* Switch tab context and refresh the views */
int
tab_switch(Dirview view[WIN_NR], const TabCtx *ctx, const TabCtx *head)
{
	assert(view && ctx);

	view[LEFT].ctx = ctx->left;
	view[CENTER].ctx = ctx->center;
	view[RIGHT].ctx = ctx->right;

	view[TOP].ctx = ctx->center;
	view[BOT].ctx = ctx->center;

	/* Rescan the directories for possible background changes */
	rescan_pane(ctx->left);
	rescan_pane(ctx->center);
	rescan_pane(ctx->right);

	refresh_listing(view+LEFT, 0);
	refresh_listing(view+CENTER, 1);
	refresh_listing(view+RIGHT, 0);

	update_status_top(view+TOP, head);
	update_status_bottom(view+BOT);

	return 0;
}

/* Try to highlight the idxth line, deselecting the previous one. Returns the
 * number of the line that was actually selected */
int
try_highlight(Dirview *win, int idx)
{
	int row_nr;
	PaneCtx *ctx;

	ctx = win->ctx;

	assert(win);

	row_nr = ctx->dir->sel_idx - ctx->offset;
	/* Update the dir backend */
	idx = try_select(ctx->dir, idx + ctx->offset, ctx->visual) - ctx->offset;
	/* Update the ncurses frontend */
	change_highlight(win->win, row_nr, idx);
	return idx;
}


void
update_status_bottom(Dirview *win)
{
	char last_mod[MAXDATELEN+1];
	char mode[10+1];
	struct tm *mtime;
	const Fileentry *sel;

	sel = win->ctx->dir->tree[win->ctx->dir->sel_idx];

	mtime = localtime(&sel->lastchange);
	strftime(last_mod, MAXDATELEN, "%F %R", mtime);
	octal_to_str(sel->mode, mode);

	werase(win->win);
	wattrset(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	mvwprintw(win->win, 0, 0, "%s ", mode);
	wattrset(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
	wprintw(win->win," %d  %d  %s", sel->uid, sel->gid, last_mod);

	wrefresh(win->win);
}

void
update_status_top(Dirview *win, const TabCtx *tabs)
{
	char *user, *wd, *hi, *tab_fullname;
	char hostn[MAXHOSTNLEN+1];
	char tabname[TABNAME_MAX+1];
	int cur_off;

	user = getenv("USER");
	cur_off = getmaxx(win->win) - 1;
	wd = win->ctx->dir->path;
	gethostname(hostn, MAXHOSTNLEN);
	hi = win->ctx->dir->tree[win->ctx->dir->sel_idx]->name;

	assert(user && wd && win->win);

	werase(win->win);

	/* For some reason I can't wattrset the color along with A_BOLD :/ */
	wattrset(win->win, A_BOLD);
	wattron(win->win, COLOR_PAIR(PAIR_BLUE_DEF));
	mvwprintw(win->win, 0, 0, "%s@%s", user, hostn);
	wattron(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	wprintw(win->win, " %s/", wd);
	wattron(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
	wprintw(win->win, "%s", hi);
	wattrset(win->win, COLOR_PAIR(PAIR_CYAN_DEF));
	for (; tabs != NULL; tabs = tabs->next) {
		if (tabs->center == win->ctx) {
			wattron(win->win, A_REVERSE);
		} else {
			wattroff(win->win, A_REVERSE);
		}
		tab_fullname = tabs->center->dir->path + strlen(tabs->center->dir->path) - 1;
		/* TODO this is not that safe since we might go past the beginning of
		 * the string */
		for (; *tab_fullname != '/'; tab_fullname--)
			;
		tab_fullname++;

		strchomp(tab_fullname, tabname, TABNAME_MAX);
		cur_off -= strlen(tabname) + 1;
		mvwprintw(win->win, 0, cur_off, "%s", tabname);
	}

	wrefresh(win->win);
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
		delwin(view[i].win);
	}

	return 0;
}

/* Initialize the sub-windows that make up the main view */
int
windows_init(Dirview view[WIN_NR], int row, int col, float main_perc)
{
	int mc, sc_l, sc_r;

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

	/* Check for window updates every 250 ms */
	wtimeout(view[BOT].win, UPD_INTERVAL);
	return 0;
}
