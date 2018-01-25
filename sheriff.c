#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "dir.h"
#include "utils.h"
#include "config.h"

#define HUMANSIZE_LEN 6
#define MAIN_PERC 0.6
#define MAXHOSTNLEN 32
#define MAXDATELEN 18
#define WIN_NR 5

#define PAIR_BLUE_DEF 1
#define PAIR_GREEN_DEF 2
#define PAIR_WHITE_DEF 3
#define PAIR_CYAN_DEF 4
#define PAIR_YELLOW_DEF 5

enum windows
{
	top_win = 0,
	left_win = 1,
	center_win = 2,
	right_win = 3,
	bot_win = 4
};


typedef struct
{
	WINDOW* win;            /* Ncurses window being managed */
	struct direntry* dir;   /* Directory associated with the view */
} Dirview;

static int   deinit_windows(Dirview view[WIN_NR]);
static void  init_colors();
static int   init_windows(Dirview view[WIN_NR], int w, int h, float main_perc);
static void  print_status_bottom(Dirview* win);
static void  print_status_top(Dirview* win);
static void  resize_handler(int sig);
static int   show_listing(Dirview* win, int show_sizes);
static int   try_highlight(Dirview* win, int idx);

/* This is the only variable I can't make non-global
 * since it is needed by the SIGWINCH handler
 */
Dirview main_view[WIN_NR];

/* Deinitialize windows, right before exiting.
 * This deallocates all memory dedicated to fileentry_t* lists
 * and their paths
 */
int
deinit_windows(Dirview view[WIN_NR])
{
	int i;

	assert(view);
	/* Deallocate directory listings. Top and bottom windows
	 * inherit the center_win path, so there's no need to free those two */
	for(i=0; i<WIN_NR; i++)
		if(view[i].dir && i != top_win && i != bot_win)
			free_listing(&view[i].dir);

	for(i=0; i<WIN_NR; i++)
		delwin(view[i].win);

	return 0;
}

/* Initialize the windows that make up the main view */
int
init_windows(Dirview view[WIN_NR], int row, int col, float main_perc)
{
	int main_cols, side_cols;

	if(!view)
		return -1;

	main_cols = col * main_perc;
	side_cols = (col - main_cols) / 2;

	view[top_win].win = newwin(1, col, 0, 0);
	view[bot_win].win = newwin(1, col, row - 1, 0);
	view[left_win].win = newwin(row - 2, side_cols, 1, 0);
	view[center_win].win = newwin(row - 2, main_cols - 2, 1, side_cols + 1);
	view[right_win].win = newwin(row - 2, side_cols, 1, side_cols + main_cols);

	return 0;
}

/* Initialize color pairs */
void
init_colors(void)
{
	init_pair(PAIR_BLUE_DEF, COLOR_BLUE, -1);
	init_pair(PAIR_GREEN_DEF, COLOR_GREEN, -1);
	init_pair(PAIR_WHITE_DEF, COLOR_WHITE, -1);
	init_pair(PAIR_CYAN_DEF, COLOR_CYAN, -1);
	init_pair(PAIR_YELLOW_DEF, COLOR_YELLOW, -1);
}

/* Update the bottom status bar
 * Format: <mode> <uid> <gid> <last_modified> --- <size>
 */
void
print_status_bottom(Dirview* win)
{
	int i, rows, cols;
	char mode[] = "----------";
	char humansize[HUMANSIZE_LEN+1];
	char last_modified[MAXDATELEN+1];
	struct tm* mtime;

	getmaxyx(win->win, rows, cols);


	octal_to_str(win->dir->sel->mode, mode);
	tohuman(win->dir->sel->size, humansize);
	mtime = localtime(&(win->dir->sel->lastchange));
	strftime(last_modified, MAXDATELEN, "%F %R", mtime);

	assert(win->win);
	wclear(win->win);
	wattrset(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	// TODO: mode to string
	mvwprintw(win->win, 0, 0, "%s  %d  %d  %s", mode, win->dir->sel->uid, win->dir->sel->gid, last_modified);
	mvwprintw(win->win, 0, cols - 7, "%6s", humansize);
	wrefresh(win->win);
}

/* Updates the top status bar
 * Format: <user>@<host> $PWD/<selected entry>
 */
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

	wclear(win->win);
	wattrset(win->win, A_BOLD | COLOR_PAIR(PAIR_BLUE_DEF));
	mvwprintw(win->win, 0, 0, "%s@%s", username, hostname);
	wattron(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	wprintw(win->win, " %s/", workdir);
	wattron(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
	wprintw(win->win, "%s", win->dir->sel->name);

	wrefresh(win->win);
}

void
resize_handler(int sig)
{
	int i, nr, nc, main_cols, side_cols;

	/* Re-initialize ncurses with the correct dimensions */
	endwin();
	refresh();
	getmaxyx(stdscr, nr, nc);

	main_cols = nc * MAIN_PERC;
	side_cols = (nc - main_cols) / 2;

	fprintf(stderr, "Resizing to %dx%d, main %d, side %d\n", nc, nr, main_cols, side_cols);

	for(i=0; i<WIN_NR; i++)
		mvwin(main_view[i].win, 0, 0);

	wresize(main_view[top_win].win, 1, nc);
	mvwin(main_view[top_win].win, 0, 0);
	wresize(main_view[bot_win].win, 1, nc);
	mvwin(main_view[bot_win].win, nr - 1, 0);
	wresize(main_view[left_win].win, nr - 2, side_cols);
	mvwin(main_view[left_win].win, 1, 0);
	wresize(main_view[center_win].win, nr - 2, main_cols - 2);
	mvwin(main_view[center_win].win, 1, side_cols + 1);
	wresize(main_view[right_win].win, nr - 2, side_cols);
	mvwin(main_view[right_win].win, 1, side_cols + main_cols);

	clear();

	print_status_top(main_view + top_win);
	show_listing(main_view + left_win, 0);
	show_listing(main_view + center_win, 1);
	show_listing(main_view + right_win, 0);
	print_status_bottom(main_view + bot_win);
}

/* Render a directory listing on a window */
int
show_listing(Dirview* win, int show_sizes)
{
	int maxrows, maxcols, rowcount;
	char* tmpstring;
	char humansize[HUMANSIZE_LEN+1];
	const fileentry_t* tmptree;

	assert(win->win);

	if(!win->dir || !win->dir->tree)
	{
		wclear(win->win);
		wrefresh(win->win);
		return 1;
	}

	/* Allocate enough space to fit the shortened listing names */
	getmaxyx(win->win, maxrows, maxcols);
	tmpstring = safealloc(sizeof(char) * maxcols);

	/* Go to the top corner */
	wmove(win->win, 0, 0);
	wclear(win->win);

	rowcount = 0;
	/* Read up to $maxrows entries */
	for(tmptree = win->dir->tree; (tmptree != NULL) && rowcount < maxrows; tmptree = tmptree->next)
	{
		assert(tmptree);
		/* Change color based on the entry type */
		switch(tmptree->type)
		{
			case DT_LNK:
				wattrset(win->win, COLOR_PAIR(PAIR_CYAN_DEF));
				break;
			case DT_DIR:
				wattrset(win->win, A_BOLD | COLOR_PAIR(PAIR_GREEN_DEF));
				break;
			case DT_BLK:
			case DT_FIFO:
			case DT_SOCK:
			case DT_CHR:
				wattrset(win->win, COLOR_PAIR(PAIR_YELLOW_DEF));
				break;
			case DT_REG:
			default:
				wattrset(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
				break;
		}
		/* Chomp string so that it fits in the window */
		if(!show_sizes)
		{
			strchomp(tmptree->name, tmpstring, maxcols - 1);
			wprintw(win->win, "%s\n", tmpstring);
		}
		else
		{
			tohuman(tmptree->size, humansize);
			strchomp(tmptree->name, tmpstring, maxcols - 1 - HUMANSIZE_LEN - 1);
			/* TODO: print file size if we're asked to
			 * So, chomp string to maxcols - 1 - (space reserved to size) */
			wprintw(win->win, "%s", tmpstring);
			mvwprintw(win->win, rowcount, maxcols - HUMANSIZE_LEN - 1, "%6s", humansize);
			wprintw(win->win, "\n");
		}
		/* TODO: makes the first row disappear */
		if(rowcount == win->dir->sel_idx)
			try_highlight(win, rowcount);

		rowcount++;
	}

	free(tmpstring);
	wrefresh(win->win);

	return 0;
}

int
try_highlight(Dirview* win, int idx)
{
	int i, savedrow, savedcol;
	attr_t attr;

	assert(win);
	assert(win->win);

	/* Save current cursor position */
	getyx(win->win, savedrow, savedcol);

	/* Turn off highlighting for the previous line */
	attr = (mvwinch(win->win, win->dir->sel_idx, 0) & (A_COLOR | A_ATTRIBUTES)) & ~A_REVERSE;
	wchgat(win->win, -1, attr, PAIR_NUMBER(attr), NULL);

	/* Move to the specified line */
	if(idx > win->dir->tree_size - 1)
		win->dir->sel_idx = win->dir->tree_size - 1;
	else if (idx < 0)
		win->dir->sel_idx = 0;
	else
		win->dir->sel_idx = idx;

	/* Turn on highlighting for this line */
	attr = (mvwinch(win->win, win->dir->sel_idx, 0) & (A_COLOR | A_ATTRIBUTES)) | A_REVERSE;
	wchgat(win->win, -1, attr, PAIR_NUMBER(attr), NULL);

	/* Update the pointer to the currently selected entry */
	for(i=0, win->dir->sel = win->dir->tree; i<win->dir->sel_idx; i++, win->dir->sel = win->dir->sel->next);

	/* Restore cursor position */
	wmove(win->win, savedrow, savedcol);
	/* Return the highlighted line number */
	return win->dir->sel_idx;
}

int
main(int argc, char* argv[])
{
	int max_row, max_col;
	int i, tree_needs_update;
	char ch;
	char tmpcat[512];
	int cur_highlight = 0;

	/* Initialize ncurses */
	signal(SIGWINCH, resize_handler);     /* Register the resize handler */
	initscr();
	cbreak();                             /* Quasi-raw input */
	keypad(stdscr, TRUE);
	curs_set(0);                          /* Hide cursor */
	noecho();                             /* Don't echo keys pressed */
	use_default_colors();                 /* Enable default 16 colors */
	start_color();

	getmaxyx(stdscr, max_row, max_col);

	/* Initialize the two main directory listings */
	init_listing(&(main_view[left_win].dir), "../");
	init_listing(&(main_view[center_win].dir), "./");

	/* Associate status bars to the main direntry */
	main_view[top_win].dir = main_view[center_win].dir;
	main_view[bot_win].dir = main_view[center_win].dir;

	/* Initialize colorschemes */
	init_colors();
	/* Initialize windows */
	init_windows(main_view, max_row, max_col, MAIN_PERC);
	refresh();

	show_listing(main_view + left_win, 0);
	show_listing(main_view + center_win, 1);
	print_status_top(main_view + top_win);
	print_status_bottom(main_view + bot_win);

	tree_needs_update = 0;
	while((ch = wgetch(main_view[center_win].win)) != 'q')
	{
		switch(ch)
		{
			case 'j':
				cur_highlight = try_highlight(main_view + center_win, ++cur_highlight);
				tree_needs_update = 1;
				break;
			case 'k':
				cur_highlight = try_highlight(main_view + center_win, --cur_highlight);
				tree_needs_update = 1;
				break;
			case 'h':
				break;
			case 'l':
				break;
			default:
				break;
		}
		if(tree_needs_update)
		{
			free_listing(&(main_view[right_win].dir));
			if(main_view[center_win].dir->sel->type == DT_DIR)
			{
				snprintf(tmpcat, 512, "%s/%s", main_view[center_win].dir->path, main_view[center_win].dir->sel->name);
				init_listing(&(main_view[right_win].dir), tmpcat);
			}
			show_listing(main_view + right_win, 0);
			print_status_bottom(main_view + bot_win);
			print_status_top(main_view + top_win);
			tree_needs_update = 0;
		}
	}
	/* Terminate ncurses session */
	deinit_windows(main_view);
	endwin();
	return 0;
}

