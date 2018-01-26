#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	unsigned int i;
	void* v;
} Arg;

static int   deinit_windows(Dirview view[WIN_NR]);
static void  dialog_open_with(Dirview* view, char* fname);
static void  init_colors();
static int   init_windows(Dirview view[WIN_NR], int w, int h, float main_perc);
static void  print_status_bottom(Dirview* win);
static void  print_status_top(Dirview* win);
static void  resize_handler(int sig);
static int   refresh_listing(Dirview* win, int show_sizes);
static int   try_highlight(Dirview* win, int idx);

static void  enter_directory(const Arg* arg);
static void  exit_directory(const Arg* arg);
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
		refresh_listing(main_view + LEFT_WIN, 0);
		refresh_listing(main_view + CENTER_WIN, 1);
	}
}

void
exit_directory(const Arg* arg)
{
	navigate_back(main_view + LEFT_WIN, main_view + CENTER_WIN, main_view + RIGHT_WIN);
	associate_dir(main_view + TOP_WIN, main_view[CENTER_WIN].dir);
	associate_dir(main_view + BOT_WIN, main_view[CENTER_WIN].dir);
	try_highlight(main_view + CENTER_WIN, main_view[CENTER_WIN].dir->sel_idx);
	refresh_listing(main_view + LEFT_WIN, 0);
	refresh_listing(main_view + CENTER_WIN, 1);
}

void
open_with(const Arg* arg)
{
	struct direntry* dir = arg->v;
	char* fname;

	fname = safealloc(sizeof(char) * (strlen(dir->path) + strlen(dir->sel->name) + 1 + 1));
	sprintf(fname, "%s/%s", dir->path, dir->sel->name);

	dialog_open_with(main_view + BOT_WIN, fname);
	free(fname);
}

void
rel_highlight(const Arg* arg)
{
	int cur_pos, prev_offset;

	prev_offset = main_view[CENTER_WIN].offset;

	cur_pos = main_view[CENTER_WIN].dir->sel_idx - main_view[CENTER_WIN].offset;
	try_highlight(main_view + CENTER_WIN, cur_pos + arg->i);

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

/* Show a dialog asking for a command to open a file with */
void
dialog_open_with(Dirview* win, char* fname)
{
	pid_t pid;
	char command[MAXCMDLEN+1];

	/* Ask for the command to open the file with */
	werase(win->win);
	wattrset(win->win, COLOR_PAIR(PAIR_WHITE_DEF));
	mvwprintw(win->win, 0, 0, "open_with: ");
	echo();
	curs_set(1);
	wgetnstr(win->win, command, MAXCMDLEN);

	/* Leave curses mode */
	def_prog_mode();
	endwin();

	/* Spawn the requested command */
	if(!(pid = fork()))
	{
		execlp(command, command, fname, NULL);
		exit(0);
	}
	else
		waitpid(pid, NULL, 0);

	/* Restore curses mode */
	reset_prog_mode();
	noecho();
	curs_set(0);
	werase(win->win);
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

	octal_to_str(win->dir->sel->mode, mode);
	mtime = localtime(&(win->dir->sel->lastchange));
	strftime(last_modified, MAXDATELEN, "%F %R", mtime);

	assert(win->win);
	werase(win->win);
	wattrset(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	// TODO: mode to string
	mvwprintw(win->win, 0, 0, "%s  %d  %d  %s",
	          mode, win->dir->sel->uid, win->dir->sel->gid, last_modified);

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
	wprintw(win->win, "%s", win->dir->sel->name);

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
	int mr, mc, rowcount;
	char* tmpstring;
	char humansize[HUMANSIZE_LEN+1];
	fileentry_t* tmptree;

	assert(win->win);

	if(!win->dir || !win->dir->tree)
	{
		werase(win->win);
		wrefresh(win->win);
		return 1;
	}

	/* Allocate enough space to fit the shortened listing names */
	getmaxyx(win->win, mr, mc);
	tmpstring = safealloc(sizeof(char) * mc);

	/* Go to the top corner */
	werase(win->win);
	wmove(win->win, 0, 0);

	/* Skip the first $offset entries as they're off screen */
	tmptree = win->dir->tree;
	for(rowcount = 0; (tmptree != NULL) && rowcount < win->offset; rowcount++)
		tmptree = tmptree->next;
	/* Read up to $mr entries */
	for(rowcount = 0; tmptree != NULL && rowcount < mr; rowcount++, tmptree = tmptree->next)
	{
		assert(tmptree);
		/* Change color based on the entry type */
		switch(tmptree->type)
		{
			case -1:
				wattrset(win->win, COLOR_PAIR(PAIR_RED_DEF));
				break;
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
		if(!show_sizes || tmptree->size < 0)
		{
			strchomp(tmptree->name, tmpstring, mc - 1);
			wprintw(win->win, "%s\n", tmpstring);
		}
		else
		{
			/* Convert byte count to human-readable size */
			tohuman(tmptree->size, humansize);
			strchomp(tmptree->name, tmpstring, mc - 1 - HUMANSIZE_LEN - 1);

			wprintw(win->win, "%s", tmpstring);
			mvwprintw(win->win, rowcount, mc - HUMANSIZE_LEN - 1,
			          "%6s\n", humansize);
		}
		/* Higlight the element marked as selected in the dir tree */
		if(rowcount == win->dir->sel_idx - win->offset)
			try_highlight(win, rowcount);

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
	int mr, cur_row;
	attr_t attr;

	assert(win);
	assert(win->win);

	/* If we have reached the top/bottom of the screen, scroll*/
	mr = getmaxy(win->win);
	if(idx >= mr - 1)
	{
		win->offset += (idx - mr + 1);
		idx = mr - 1;
	}
	else if(idx < 0 && win->offset > 0)
	{
		win->offset += idx;
		idx = 0;
	}

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
	char ch;
	char *tmpcat, *center_wd, *center_selected;

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
	init_listing(&(main_view[LEFT_WIN].dir), "../");
	init_listing(&(main_view[CENTER_WIN].dir), "./");
	init_listing(&(main_view[RIGHT_WIN].dir), "./");

	/* Associate status bars to the main direntry */
	main_view[TOP_WIN].dir = main_view[CENTER_WIN].dir;
	main_view[BOT_WIN].dir = main_view[CENTER_WIN].dir;

	/* Initialize colorschemes */
	init_colors();
	/* Initialize windows */
	init_windows(main_view, max_row, max_col, MAIN_PERC);

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

		assert(!free_listing(&(main_view[RIGHT_WIN].dir)));
		if(main_view[CENTER_WIN].dir->sel->type == DT_DIR)
		{
			center_wd = main_view[CENTER_WIN].dir->path;
			center_selected = main_view[CENTER_WIN].dir->sel->name;

			assert(center_wd);
			assert(center_selected);

			tmpcat = safealloc(sizeof(char) * (strlen(center_wd) + strlen(center_selected) + 1 + 1));
			sprintf(tmpcat, "%s/%s", center_wd, center_selected);

			init_listing(&(main_view[RIGHT_WIN].dir), tmpcat);

			free(tmpcat);
		}
		refresh_listing(main_view + RIGHT_WIN, 0);
		print_status_bottom(main_view + BOT_WIN);
		print_status_top(main_view + TOP_WIN);
	}
	/* Terminate ncurses session */
	deinit_windows(main_view);
	endwin();
	return 0;
}

