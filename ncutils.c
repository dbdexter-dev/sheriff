#define _GNU_SOURCE

#include <assert.h>
#include <ncurses.h>
#include <time.h>
#include <unistd.h>
#include "backend.h"
#include "ncutils.h"
#include "utils.h"

/* Update window offset if needed, return 1 if we did change offset,
 * 0 otherwise */
int
check_offset_changed(Dirview* win)
{
	int nr;
	nr = getmaxy(win->win);

	if(win->dir->sel_idx - win->offset >= nr)
	{
		win->offset = (win->dir->sel_idx - nr + 1);
		return 1;
	}
	else if (win->dir->sel_idx - win->offset < 0)
	{
		win->offset = win->dir->sel_idx;
		return 1;
	}
	return 0;
}
/* Deinitialize windows, right before exiting. This deallocates all memory
 * dedicated to fileentry_t* arrays and their paths as well */
int
deinit_windows(Dirview view[WIN_NR])
{
	int i;

	assert(view);
	/* Deallocate directory listings. Top and bottom windows
	 * inherit the CENTER_WIN path, so there's no need to free those two */
	for(i=0; i<WIN_NR; i++)
		if(view[i].dir && i != TOP_WIN && i != BOT_WIN)
			assert(!free_listing(&view[i].dir));

	for(i=0; i<WIN_NR; i++)
		delwin(view[i].win);

	return 0;
}

/* Render a dialog prompt in a specified window. Will return the input string
 * through char* input */
void
dialog(Dirview* win, char* msg, char* input)
{
	werase(win->win);
	wattrset(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
	mvwprintw(win->win, 0, 0, msg);

	echo();
	curs_set(1);

	wgetnstr(win->win, input, MAXCMDLEN);
	werase(win->win);

	curs_set(0);
	noecho();
}

/* Initialize color pairs */
void
init_colors(void)
{
	init_pair(PAIR_RED_DEF, COLOR_RED, -1);
	init_pair(PAIR_GREEN_DEF, COLOR_GREEN, -1);
	init_pair(PAIR_BLUE_DEF, COLOR_BLUE, -1);
	init_pair(PAIR_CYAN_DEF, COLOR_CYAN, -1);
	init_pair(PAIR_YELLOW_DEF, COLOR_YELLOW, -1);
	init_pair(PAIR_WHITE_DEF, COLOR_WHITE, -1);
}

/* Initialize the sub-windows that make up the main view */
int
init_windows(Dirview view[WIN_NR], int row, int col, float main_perc)
{
	int i, mc, sc_l, sc_r;

	if(!view)
		return -1;

	/* Calculate center window and sidebars column count */
	mc = col * main_perc;
	sc_r = (col - mc) / 2;
	sc_l = col - mc - sc_r;

	view[TOP_WIN].win = newwin(1, col, 0, 0);
	view[BOT_WIN].win = newwin(1, col, row - 1, 0);
	view[LEFT_WIN].win = newwin(row - 2, sc_l - 1, 1, 0);
	view[CENTER_WIN].win = newwin(row - 2, mc - 1, 1, sc_l);
	view[RIGHT_WIN].win = newwin(row - 2, sc_r, 1, sc_l + mc);

	/* Initialize the view offsets */
	for(i=0; i<WIN_NR; i++)
		view[i].offset = 0;

	return 0;
}

/* Update the bottom status bar. Format: <permissions>  <uid>  <gid>  <last
 * modified> */
void
print_status_bottom(Dirview* win)
{
	char mode[] = "----------";
	char last_modified[MAXDATELEN+1];
	struct tm* mtime;
	const struct fileentry* sel;
	sel = win->dir->tree[win->dir->sel_idx];

	octal_to_str(sel->mode, mode);
	mtime = localtime(&(sel->lastchange));
	strftime(last_modified, MAXDATELEN, "%F %R", mtime);

	assert(win->win);
	werase(win->win);
	wattrset(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	// TODO: mode to string
	mvwprintw(win->win, 0, 0, "%s  %d  %d  %s",
	          mode, sel->uid, sel->gid, last_modified);

	wrefresh(win->win);
}

/* Updates the top status bar. Format: <user>@<host> $PWD/<highlighted entry> */
void
print_status_top(Dirview* win)
{
	char* username;
	char* workdir;
	char hostname[MAXHOSTNLEN+1];

	username = getenv("USER");
	workdir = win->dir->path;
	gethostname(hostname, MAXHOSTNLEN);

	assert(username);
	assert(workdir);
	assert(win->win);

	werase(win->win);
	wattrset(win->win, A_BOLD | COLOR_PAIR(PAIR_BLUE_DEF));
	mvwprintw(win->win, 0, 0, "%s@%s", username, hostname);
	wattron(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	wprintw(win->win, " %s/", workdir);
	wattron(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
	wprintw(win->win, "%s", win->dir->tree[win->dir->sel_idx]->name);

	wrefresh(win->win);
}

/* Render a directory listing on a window. If the directoly listing is NULL,
 * clear the relative window */
int
refresh_listing(Dirview* win, int show_sizes)
{
	int mr, mc, i;
	char* tmpstring;
	char humansize[HUMANSIZE_LEN+1];
	fileentry_t* tmpfile;

	assert(win->win);

	if(!win->dir || !win->dir->tree)
	{
		werase(win->win);
		wrefresh(win->win);
		return 1;
	}

	getmaxyx(win->win, mr, mc);

	/* Go to the top corner */
	werase(win->win);
	wmove(win->win, 0, 0);

	/* Allocate enough space to fit the shortened listing names */
	tmpstring = safealloc(sizeof(*tmpstring) * (mc + 1));

	/* Update window offsets if necessary */
	check_offset_changed(win);

	/* Read up to $mr entries */
	for(i = win->offset; i < win->dir->count && (i - win->offset) < mr; i++)
	{
		tmpfile = win->dir->tree[i];
		/* Change color based on the entry type */
		switch(tmpfile->mode & S_IFMT)
		{
			case 0:
				wattrset(win->win, COLOR_PAIR(PAIR_RED_DEF));
				break;
			case S_IFLNK:
				wattrset(win->win, COLOR_PAIR(PAIR_CYAN_DEF));
				break;
			case S_IFDIR:
				wattrset(win->win, A_BOLD | COLOR_PAIR(PAIR_GREEN_DEF));
				break;
			case S_IFBLK:
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
		/* Chomp string so that it fits in the window */
		if(!show_sizes || tmpfile->size < 0)
		{
			if(strchomp(tmpfile->name, tmpstring, mc))
				die("We do not kno de wey (read: strchomp failed)");
			wprintw(win->win, "%s\n", tmpstring);
		}
		else
		{
			/* Convert byte count to human-readable size */
			tohuman(tmpfile->size, humansize);
			strchomp(tmpfile->name, tmpstring, mc - HUMANSIZE_LEN - 1);

			wprintw(win->win, "%s", tmpstring);
			mvwprintw(win->win, i - win->offset, mc - HUMANSIZE_LEN - 1,
			          "%6s\n", humansize);
		}
		/* Higlight the element marked as selected in the dir tree */
		if(i == win->dir->sel_idx)
			try_highlight(win, i - win->offset);

	}

	free(tmpstring);
	wrefresh(win->win);
	return 0;
}

/* Try to highlight the idxth line, deselecting the previous one. Returns the
 * number number of the line that was actually selected */
int
try_highlight(Dirview* win, int idx)
{
	int savedrow, savedcol;
	int cur_row;
	attr_t attr;

	assert(win);
	assert(win->win);

	getyx(win->win, savedrow, savedcol);
	cur_row = win->dir->sel_idx - win->offset;

	/* Turn off highlighting for the previous line */
	attr = (mvwinch(win->win, cur_row, 0) & (A_COLOR | A_ATTRIBUTES)) & ~A_REVERSE;
	wchgat(win->win, -1, attr, PAIR_NUMBER(attr), NULL);

	idx = try_select(win->dir, idx + win->offset) - win->offset;

	/* Turn on highlighting for this line */
	attr = (mvwinch(win->win, idx, 0) & (A_COLOR | A_ATTRIBUTES)) | A_REVERSE;
	wchgat(win->win, -1, attr, PAIR_NUMBER(attr), NULL);

	/* Restore cursor position */
	wmove(win->win, savedrow, savedcol);
	return idx;
}
