#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "backend.h"
#include "fileops.h"
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
render_tree(Dirview *win, int show_sizes)
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

	getmaxyx(win->win, mr, mc);         /* Get screen bounds */

	werase(win->win);                   /* Go to the top corner */
	wmove(win->win, 0, 0);

	/* Allocate enough space to fit the shortened listing names */
	tmpstring = safealloc(sizeof(*tmpstring) * (mc + 1));

	check_offset_changed(win);          /* Update window offsets if necessary */

	/* Read up to $mr entries */
	for (i = ctx->offset; i < ctx->dir->count && (i - ctx->offset) < mr; i++) {
		tmpfile = ctx->dir->tree[i];

		if (tmpfile->selected) {        /* If visually selected, mark it */
			wattrset(win->win, COLOR_PAIR(PAIR_YELLOW_DEF) | A_BOLD);
		} else {                        /* Change color based on entry type */
			switch (tmpfile->mode & S_IFMT) {
			case 0:                     /* Not a file */
				wattrset(win->win, COLOR_PAIR(PAIR_RED_DEF));
				break;
			case S_IFLNK:
				wattrset(win->win, COLOR_PAIR(PAIR_CYAN_DEF));
				break;
			case S_IFDIR:
				wattrset(win->win, A_BOLD | COLOR_PAIR(PAIR_GREEN_DEF));
				break;
			case S_IFBLK:               /* 4 intentional fallthroughs */
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
		/* Chomp string so that it fits inside the window */
		if (!show_sizes || tmpfile->size < 0) {
			assert(!strchomp(tmpfile->name, tmpstring, mc-1));
			wprintw(win->win, "%s\n", tmpstring);
		} else {
			/* Convert byte count to human-readable size */
			tohuman(tmpfile->size, humansize);
			strchomp(tmpfile->name, tmpstring, mc - HUMANSIZE_LEN - 3);

			wprintw(win->win, "%s", tmpstring);
			mvwprintw(win->win, i - ctx->offset, mc - HUMANSIZE_LEN - 1,
					  "%6s\n", humansize);
		}
		/* Higlight the selected element in the dir listing */
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
tab_switch(Dirview view[WIN_NR], const TabCtx *ctx)
{
	assert(view && ctx);

	/* Correct the backend. Note that both bars refer to the same PaneCtx as the
	 * center view */
	view[LEFT].ctx = ctx->left;
	view[CENTER].ctx = ctx->center;
	view[RIGHT].ctx = ctx->right;
	view[TOP].ctx = ctx->center;
	view[BOT].ctx = ctx->center;

	/* Rescan the directories for possible background changes */
	rescan_pane(ctx->left);
	rescan_pane(ctx->center);
	rescan_pane(ctx->right);

	/* Update the frontend */
	render_tree(view+LEFT, 0);
	render_tree(view+CENTER, 1);
	render_tree(view+RIGHT, 0);
	update_status_top(view+TOP);
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

	assert(win);
	ctx = win->ctx;

	row_nr = ctx->dir->sel_idx - ctx->offset;
	/* Update the dir backend */
	idx = try_select(ctx->dir, idx + ctx->offset, ctx->visual) - ctx->offset;
	/* Update the ncurses frontend */
	change_highlight(win->win, row_nr, idx);
	return idx;
}

/* Update the bottom status bar with all the relevant information */
void
update_status_bottom(Dirview *win)
{
	char last_mod[MAXDATELEN+1];
	char mode[10+1];
	struct tm *mtime;
	const Fileentry *sel;
	Progress *pr;
	int barlen;

	sel = win->ctx->dir->tree[win->ctx->dir->sel_idx];

	/* Gather some info */
	pr = fileop_progress();
	mtime = localtime(&sel->lastchange);
	strftime(last_mod, MAXDATELEN, "%F %R", mtime);
	octal_to_str(sel->mode, mode);

	/* Display it on the screen */
	werase(win->win);
	wattrset(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	mvwprintw(win->win, 0, 0, "%s ", mode);
	wattrset(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
	wprintw(win->win," %d  %d  %s", sel->uid, sel->gid, last_mod);

	pthread_mutex_lock(&pr->mutex);
	if (pr->obj_count > 0) {
		wprintw(win->win, " %s", pr->fname);
		barlen = (pr->obj_done / (float)pr->obj_count) * getmaxx(win->win);
		wmove(win->win, 0, 0);
		wattrset(win->win, A_REVERSE);
		wchgat(win->win, barlen, A_REVERSE, PAIR_GREEN_DEF, NULL);
	}
	pthread_mutex_unlock(&pr->mutex);

	wrefresh(win->win);
}

/* Update the top bar with all the relevant information */
void
update_status_top(Dirview *win)
{
	char *user, *wd, *hi, *tab_fullname;
	char hostn[MAXHOSTNLEN+1];
	char tabname[TABNAME_MAX+1];
	int cur_off, i;
	const TabCtx *tabs;

	/* Gather some info */
	tabs = tabctx_get();
	user = getenv("USER");
	cur_off = getmaxx(win->win);
	gethostname(hostn, MAXHOSTNLEN);
	hi = win->ctx->dir->tree[win->ctx->dir->sel_idx]->name;

	/* If the path is just "/', don't append a slash, otherwise do it */
	if (win->ctx->dir->path[1] == '\0') {
		wd = safealloc(sizeof(*wd) * (strlen(win->ctx->dir->path) + 1));
		strcpy(wd, win->ctx->dir->path);
	} else {
		wd = join_path(win->ctx->dir->path, "");
	}

	assert(user && wd && win->win);

	werase(win->win);

	/* For some reason I can't wattrset the color along with A_BOLD :/ */
	wattrset(win->win, A_BOLD);
	wattron(win->win, COLOR_PAIR(PAIR_CYAN_DEF));
	mvwprintw(win->win, 0, 0, "%s@%s", user, hostn);
	wattron(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	zip_path(wd);
	wprintw(win->win, " %s", wd);
	free(wd);
	wattron(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
	wprintw(win->win, "%s", hi);

	/* Display tab names */
	wattron(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
	for (; tabs != NULL; tabs = tabs->next) {
		if (tabs->center == win->ctx) {
			wattron(win->win, A_REVERSE);
		} else {
			wattroff(win->win, A_REVERSE);
		}
		tab_fullname = tabs->center->dir->path;
		for (i=0; tab_fullname[i] != '\0'; i++)
			;
		for (; tab_fullname[i] != '/' && i>=0; i--)
			;
		i++;
		if (tab_fullname[i] != '\0') {
			strchomp(tab_fullname+i, tabname, TABNAME_MAX);
			cur_off -= strlen(tabname);
			mvwprintw(win->win, 0, cur_off, "%s", tabname);
		} else {
			cur_off -= 1;
			mvwprintw(win->win, 0, cur_off, "/");
		}
		cur_off--;                      /* Add a space between tab names */
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
windows_init(Dirview view[WIN_NR], int row, int col, int pp[3])
{
	int mc, sc_l, sc_r;
	float sum;

	if (!view) {
		return -1;
	}

	sum = pp[0] + pp[1] + pp[2];

	/* Calculate center window and sidebars column count */
	sc_l = col * pp[0]/sum;
	mc = col * pp[1]/sum;
	sc_r = col - sc_l - mc;

	view[TOP].win = newwin(1, col, 0, 0);
	view[BOT].win = newwin(1, col, row - 1, 0);
	view[LEFT].win = newwin(row - 2, sc_l - 1, 1, 0);
	view[CENTER].win = newwin(row - 2, mc - 1, 1, sc_l);
	view[RIGHT].win = newwin(row - 2, sc_r, 1, sc_l + mc);

	/* Check for window updates every 250 ms */
	wtimeout(view[BOT].win, UPD_INTERVAL);
	return 0;
}
