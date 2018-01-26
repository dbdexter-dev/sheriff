#define _GNU_SOURCE

#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "backend.h"
#include "dir.h"
#include "utils.h"

#define HUMANSIZE_LEN 6
#define MAIN_PERC 0.6
#define MAXHOSTNLEN 32
#define MAXDATELEN 18
#define MAXCMDLEN 128
#define MAXSEARCHLEN MAXCMDLEN
#define WIN_NR 5

#define PAIR_BLUE_DEF 1
#define PAIR_GREEN_DEF 2
#define PAIR_WHITE_DEF 3
#define PAIR_CYAN_DEF 4
#define PAIR_YELLOW_DEF 5
#define PAIR_RED_DEF 6

#define LENGTH(X) sizeof(X) / sizeof(X[0])

/* Convenient enum to address a specific view in main_view */
enum windows
{
	TOP_WIN = 0,
	LEFT_WIN = 1,
	CENTER_WIN = 2,
	RIGHT_WIN = 3,
	BOT_WIN = 4
};

typedef union
{
	int i;
	void* v;
} Arg;

static int   deinit_windows(Dirview view[WIN_NR]);
static void  dialog(Dirview* view, char* msg, char* buf);
static void  init_colors();
static int   init_windows(Dirview view[WIN_NR], int w, int h, float main_perc);
static void  print_status_bottom(Dirview* win);
static void  print_status_top(Dirview* win);
static void  resize_handler(int sig);
static int   refresh_listing(Dirview* win, int show_sizes);
static int   try_highlight(Dirview* win, int idx);

static void  enter_directory(const Arg* arg);
static void  exit_directory(const Arg* arg);
static void  filesearch(const Arg* arg);
static void  open_with(const Arg* arg);
static void  rel_highlight(const Arg* arg);

/* This is the only variable I can't make non-global
 * since it is needed by the SIGWINCH handler and the various
 * keybind handlers
 */
Dirview main_view[WIN_NR];

/* Keybind handlers {{{*/
void
enter_directory(const Arg* arg)
{
	int is_file;
	Arg open_arg;

	/* If type < 0, it's not a file, but a message (e.g. "inaccessible"): bail out*/
	if(main_view[CENTER_WIN].dir->tree[main_view[CENTER_WIN].dir->sel_idx]->mode == 0)
		return;

	is_file = navigate_fwd(main_view + LEFT_WIN, main_view + CENTER_WIN, main_view + RIGHT_WIN);
	/* If current element is a file, open a dialog */
	if(is_file)
	{

		open_arg.v = (void*) main_view[CENTER_WIN].dir;
		open_with(&open_arg);
	}
	/* Normal directory, proceed as planned */
	else
	{
		associate_dir(main_view + TOP_WIN, main_view[CENTER_WIN].dir);
		associate_dir(main_view + BOT_WIN, main_view[CENTER_WIN].dir);
		main_view[CENTER_WIN].offset = 0;
		refresh_listing(main_view + LEFT_WIN, 0);
		refresh_listing(main_view + CENTER_WIN, 1);
		refresh_listing(main_view + RIGHT_WIN, 0);
	}
}

void
exit_directory(const Arg* arg)
{
	navigate_back(main_view + LEFT_WIN, main_view + CENTER_WIN, main_view + RIGHT_WIN);
	associate_dir(main_view + TOP_WIN, main_view[CENTER_WIN].dir);
	associate_dir(main_view + BOT_WIN, main_view[CENTER_WIN].dir);
	refresh_listing(main_view + LEFT_WIN, 0);
	refresh_listing(main_view + CENTER_WIN, 1);
	refresh_listing(main_view + RIGHT_WIN, 0);
}

/* File search */
void
filesearch(const Arg* arg)
{
	int i, found;
	char fname[MAXSEARCHLEN+1];
	const struct direntry* dir = main_view[CENTER_WIN].dir;
	echo();

	dialog(main_view + BOT_WIN, arg->i > 0 ? "/" : "?", fname);
	found = 0;

	if(arg->i > 0)
	{
		for(i=main_view[CENTER_WIN].dir->sel_idx; i<dir->tree_size && !found; i++)
			if(strcasestr(dir->tree[i]->name, fname))
			{
				main_view[CENTER_WIN].dir->sel_idx = i;
				found = 1;
			}
	}
	else
	{
		for(i=main_view[CENTER_WIN].dir->sel_idx; i>=0 && !found; i--)
			if(strcasestr(dir->tree[i]->name, fname))
			{
				main_view[CENTER_WIN].dir->sel_idx = i;
				found = 1;
			}
	}
	refresh_listing(main_view + CENTER_WIN, 1);
	noecho();
}

void
open_with(const Arg* arg)
{
	struct direntry* dir = arg->v;
	char* fname;
	char cmd[MAXCMDLEN+1];
	pid_t pid;

	fname = safealloc(sizeof(char) * (strlen(dir->path) + strlen(dir->tree[dir->sel_idx]->name) + 1 + 1));
	sprintf(fname, "%s/%s", dir->path, dir->tree[dir->sel_idx]->name);

	/* Ask the user for a command to open the file with */
	dialog(main_view + BOT_WIN,  "open_with: ", cmd);

	/* Leave curses mode */
	def_prog_mode();
	endwin();

	/* Spawn the requested command */
	if(!(pid = fork()))
	{
		execlp(cmd, cmd, fname, NULL);
		exit(0);
	}
	else
		waitpid(pid, NULL, 0);

	/* Restore curses mode */
	reset_prog_mode();
	free(fname);
}

void
rel_highlight(const Arg* arg)
{
	int cur_pos, prev_offset, mr;

	prev_offset = main_view[CENTER_WIN].offset;
	mr = getmaxy(main_view[CENTER_WIN].win);

	/* Handle possible offsets in the window */
	cur_pos = main_view[CENTER_WIN].dir->sel_idx - main_view[CENTER_WIN].offset;
	try_highlight(main_view + CENTER_WIN, cur_pos + arg->i);

	/* If the selected element is a directory, update the right pane
	 * Otherwise, free it so that refresh_listing will show a blank pane */
	if(S_ISDIR(main_view[CENTER_WIN].dir->tree[main_view[CENTER_WIN].dir->sel_idx]->mode))
		update_win_with_path(main_view + RIGHT_WIN,
		                     main_view[CENTER_WIN].dir->path,
		                     main_view[CENTER_WIN].dir->tree[main_view[CENTER_WIN].dir->sel_idx]);
	else
		assert(!free_listing(&main_view[RIGHT_WIN].dir));

	/* TODO update only if needed */
	refresh_listing(main_view + RIGHT_WIN, 0);

	/* Calculate the offsets since they might have changed */
	if(main_view[CENTER_WIN].dir->sel_idx - main_view[CENTER_WIN].offset >= mr)
		main_view[CENTER_WIN].offset = (main_view[CENTER_WIN].dir->sel_idx - mr + 1);
	else if (main_view[CENTER_WIN].dir->sel_idx - main_view[CENTER_WIN].offset < 0)
		main_view[CENTER_WIN].offset = main_view[CENTER_WIN].dir->sel_idx;

	if(main_view[CENTER_WIN].offset != prev_offset)
		refresh_listing(main_view + CENTER_WIN, 1);

	wrefresh(main_view[CENTER_WIN].win);
}
/*}}}*/
#include "config.h"

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
	 * inherit the CENTER_WIN path, so there's no need to free those two */
	for(i=0; i<WIN_NR; i++)
		if(view[i].dir && i != TOP_WIN && i != BOT_WIN)
			assert(!free_listing(&view[i].dir));

	for(i=0; i<WIN_NR; i++)
		delwin(view[i].win);

	return 0;
}

/* Show a dialog prompt in a specified window */
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

/* Initialize the windows that make up the main view */
int
init_windows(Dirview view[WIN_NR], int row, int col, float main_perc)
{
	int i, mc, sc_l, sc_r;

	if(!view)
		return -1;

	/* Calculate main area and sidebars column count */
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

/* Initialize color pairs */
void
init_colors(void)
{
	init_pair(PAIR_BLUE_DEF, COLOR_BLUE, -1);
	init_pair(PAIR_GREEN_DEF, COLOR_GREEN, -1);
	init_pair(PAIR_WHITE_DEF, COLOR_WHITE, -1);
	init_pair(PAIR_CYAN_DEF, COLOR_CYAN, -1);
	init_pair(PAIR_YELLOW_DEF, COLOR_YELLOW, -1);
	init_pair(PAIR_RED_DEF, COLOR_RED, -1);
}

/* Update the bottom status bar
 * Format: <mode> <uid> <gid> <last_modified> --- <size>
 */
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

	werase(win->win);
	wattrset(win->win, A_BOLD | COLOR_PAIR(PAIR_BLUE_DEF));
	mvwprintw(win->win, 0, 0, "%s@%s", username, hostname);
	wattron(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	wprintw(win->win, " %s/", workdir);
	wattron(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
	wprintw(win->win, "%s", win->dir->tree[win->dir->sel_idx]->name);

	wrefresh(win->win);
}

void
resize_handler(int sig)
{
	int i, nr, nc, mc, sc_l, sc_r;

	/* Re-initialize ncurses with the correct dimensions */
	endwin();
	refresh();
	erase();
	getmaxyx(stdscr, nr, nc);

	mc = nc * MAIN_PERC;
	sc_l = (nc - mc) / 2;
	sc_r = nc - mc - sc_l;

	for(i=0; i<WIN_NR; i++)
		mvwin(main_view[i].win, 0, 0);

	wresize(main_view[TOP_WIN].win, 1, nc);
	mvwin(main_view[TOP_WIN].win, 0, 0);

	wresize(main_view[BOT_WIN].win, 1, nc);
	mvwin(main_view[BOT_WIN].win, nr - 1, 0);

	wresize(main_view[LEFT_WIN].win, nr - 2, sc_l - 1);
	mvwin(main_view[LEFT_WIN].win, 1, 0);

	wresize(main_view[CENTER_WIN].win, nr - 2, mc - 1);
	mvwin(main_view[CENTER_WIN].win, 1, sc_l);

	wresize(main_view[RIGHT_WIN].win, nr - 2, sc_r - 1);
	mvwin(main_view[RIGHT_WIN].win, 1, sc_l + mc);

	print_status_top(main_view + TOP_WIN);
	refresh_listing(main_view + LEFT_WIN, 0);
	refresh_listing(main_view + CENTER_WIN, 1);
	refresh_listing(main_view + RIGHT_WIN, 0);
	print_status_bottom(main_view + BOT_WIN);
}

/* Render a directory listing on a window */
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

	/* Allocate enough space to fit the shortened listing names */
	getmaxyx(win->win, mr, mc);

	/* Go to the top corner */
	werase(win->win);
	wmove(win->win, 0, 0);

	tmpstring = safealloc(sizeof(char) * mc);

	/* Read up to $mr entries */
	if(win->dir->sel_idx - win->offset >= mr)
		win->offset = (win->dir->sel_idx - mr + 1);
	else if (win->dir->sel_idx - win->offset < 0)
		win->offset = win->dir->sel_idx;

	for(i = win->offset; i < win->dir->tree_size && (i - win->offset) < mr; i++)
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
			assert(!strchomp(tmpfile->name, tmpstring, mc - 1));
			wprintw(win->win, "%s\n", tmpstring);
		}
		else
		{
			/* Convert byte count to human-readable size */
			tohuman(tmpfile->size, humansize);
			strchomp(tmpfile->name, tmpstring, mc - 1 - HUMANSIZE_LEN - 1);

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

/* Highlight the idxth line, deselecting the previous one */
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

int
main(int argc, char* argv[])
{
	int i, max_row, max_col;
	wchar_t ch;

	/* Initialize ncurses */
	signal(SIGWINCH, resize_handler);     /* Register the resize handler */
	initscr();                            /* Initialize ncurses sesion */
	noecho();                             /* Don't echo keys pressed */
	cbreak();                             /* Quasi-raw input */
	curs_set(0);                          /* Hide cursor */
	use_default_colors();                 /* Enable default 16 colors */
	start_color();

	getmaxyx(stdscr, max_row, max_col);

	/* Initialize the two main directory listings */
	init_listing(&(main_view[LEFT_WIN].dir), "../");
	init_listing(&(main_view[CENTER_WIN].dir), "./");
	init_listing(&(main_view[RIGHT_WIN].dir), "./");


	/* Associate status bars to the main direntry */
	main_view[TOP_WIN].dir = main_view[CENTER_WIN].dir;
	main_view[BOT_WIN].dir = main_view[CENTER_WIN].dir;

	init_colors();                                          /* Initialize colorschemes */
	init_windows(main_view, max_row, max_col, MAIN_PERC);   /* Initialize windows */
	keypad(main_view[BOT_WIN].win, TRUE);                   /* Enable keypad input from where we'll get it */

	refresh_listing(main_view + LEFT_WIN, 0);
	refresh_listing(main_view + CENTER_WIN, 1);
	refresh_listing(main_view + RIGHT_WIN, 0);
	print_status_top(main_view + TOP_WIN);
	print_status_bottom(main_view + BOT_WIN);

	/* Main control loop */
	while((ch = wgetch(main_view[BOT_WIN].win)) != 'q')
	{
		/* Call the function associated with the key pressed */
		for(i=0; i<LENGTH(keys); i++)
			if(ch == keys[i].key)
				keys[i].funct(&keys[i].arg);

		print_status_top(main_view + TOP_WIN);
		print_status_bottom(main_view + BOT_WIN);
	}
	/* Terminate ncurses session */
	deinit_windows(main_view);
	endwin();
	return 0;
}

