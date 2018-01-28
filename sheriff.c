#define _GNU_SOURCE

#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <locale.h>
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
#include "ncutils.h"
#include "utils.h"

#define MAIN_PERC 0.6
#define MAXSEARCHLEN MAXCMDLEN

#define LENGTH(X) sizeof(X) / sizeof(X[0])

typedef union
{
	int i;
	void* v;
} Arg;

static void  enter_directory();
static void  exit_directory();
static void  resize_handler();
static void  xdg_open(struct direntry* file);

static void  navigate(const Arg* arg);
static void  filesearch(const Arg* arg);
static void  rel_highlight(const Arg* arg);

/* This is the only variable I can't make non-global
 * since it is needed by the SIGWINCH handler and the various
 * keybind handlers
 */
Dirview main_view[WIN_NR];

/* Keybind handlers {{{*/

/* File search, both forwards and backwards, depending on the value of arg->i */
void
filesearch(const Arg* arg)
{
	int i, found;
	char fname[MAXSEARCHLEN+1];
	const struct direntry* dir = main_view[CENTER_WIN].dir;

	dialog(main_view + BOT_WIN, arg->i > 0 ? "/" : "?", fname);

	/* Basically, go through all the elements from the currently selected one to
	 * the very [beginning | end], exiting once a match is found. TODO: add the
	 * ability to quickly redo the search */
	found = 0;
	if(arg->i > 0)
	{
		for(i=main_view[CENTER_WIN].dir->sel_idx+1; i<dir->count && !found; i++)
			if(strcasestr(dir->tree[i]->name, fname))
			{
				main_view[CENTER_WIN].dir->sel_idx = i;
				found = 1;
			}
	}
	else
	{
		for(i=main_view[CENTER_WIN].dir->sel_idx-1; i>=0 && !found; i--)
			if(strcasestr(dir->tree[i]->name, fname))
			{
				main_view[CENTER_WIN].dir->sel_idx = i;
				found = 1;
			}
	}

	/* If a match has been found, update the right pane and repaint it */
	if(found)
	{
		update_win_with_path(main_view + RIGHT_WIN, dir->path, dir->tree[dir->sel_idx]);
		refresh_listing(main_view + RIGHT_WIN, 0);
		refresh_listing(main_view + CENTER_WIN, 1);
	}
}

/* Handle navigation, either forward or backwards, through directories or
 * files */
void
navigate(const Arg* arg)
{
	struct direntry* dir = main_view[CENTER_WIN].dir;
	if(arg->i == 0)
		return;

	/* Forward navigation */
	if(arg->i > 0)
	{
		if(S_ISDIR(dir->tree[dir->sel_idx]->mode))
			enter_directory();
		else
			xdg_open(dir);
	}
	/* Backward navigation */
	else if (arg->i < 0)
		exit_directory();
}

/* Highlight a file in the center window given an offset from the currently
 * highlighted index (offset in arg->i)*/
void
rel_highlight(const Arg* arg)
{
	const struct direntry* dir = main_view[CENTER_WIN].dir;
	int cur_pos, prev_pos;

	if(arg->i == 0)
		return;

	/* Account for possible offsets in the window */
	prev_pos = dir->sel_idx - main_view[CENTER_WIN].offset;
	cur_pos = try_highlight(main_view + CENTER_WIN, prev_pos + arg->i);

	/* If the selected element is a directory, update the right pane
	 * Otherwise, free it so that refresh_listing will show a blank pane */
	if(S_ISDIR(dir->tree[dir->sel_idx]->mode))
		update_win_with_path(main_view + RIGHT_WIN, dir->path, dir->tree[dir->sel_idx]);
	else
		if(free_listing(&main_view[RIGHT_WIN].dir))
			die("free_listing failed");

	/* Update only if we actually moved inside the window */
	if(cur_pos != prev_pos)
		refresh_listing(main_view + RIGHT_WIN, 0);

	/* If the page offset doesn't have to be changed, do a simple wrefresh;
	 * otherwise do a full redraw */
	if(check_offset_changed(main_view + CENTER_WIN))
		refresh_listing(main_view + CENTER_WIN, 1);
	else
		wrefresh(main_view[CENTER_WIN].win);
}
/*}}}*/
#include "config.h"

/* Navigate to the directory selected in the center window */
void
enter_directory()
{
	/* If type < 0, it's not even a file, but a message (e.g. "inaccessible"):
	 * don't attempt a cd, just exit */
	if(main_view[CENTER_WIN].dir->tree[main_view[CENTER_WIN].dir->sel_idx]->mode == 0)
		return;

	if(navigate_fwd(main_view + LEFT_WIN, main_view + CENTER_WIN, main_view + RIGHT_WIN))
		die("Couldn't navigate_fwd");
	/* Update the top and bottom bars to reflect the change in the center
	 * window, then refresh the main views. The two bar-windows are updated
	 * every time something happens anyway in the main control loop */
	associate_dir(main_view + TOP_WIN, main_view[CENTER_WIN].dir);
	associate_dir(main_view + BOT_WIN, main_view[CENTER_WIN].dir);
	refresh_listing(main_view + LEFT_WIN, 0);
	refresh_listing(main_view + CENTER_WIN, 1);
	refresh_listing(main_view + RIGHT_WIN, 0);
}

/* Navigate backwards in the directory tree, into the parent of the center
 * window */
void
exit_directory()
{
	if(navigate_back(main_view + LEFT_WIN, main_view + CENTER_WIN, main_view + RIGHT_WIN))
		die("Couldn't navigate_back");
	/* As in enter_directory, update the directories associated to the top and
	 * bottom bars, and update the views since we've changed their underlying
	 * associations */
	associate_dir(main_view + TOP_WIN, main_view[CENTER_WIN].dir);
	associate_dir(main_view + BOT_WIN, main_view[CENTER_WIN].dir);
	refresh_listing(main_view + LEFT_WIN, 0);
	refresh_listing(main_view + CENTER_WIN, 1);
	refresh_listing(main_view + RIGHT_WIN, 0);
}

/* Handler that takes care of resizing the subviews when a SIGWINCH is received.
 * This function is one of the reasons why there has to be a global array of
 * Dirview ptrs */
void
resize_handler()
{
	int i, nr, nc, mc, sc_l, sc_r;

	/* Re-initialize ncurses with the correct dimensions */
	werase(stdscr);
	endwin();

	refresh();
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


/* Just like xdg_open, check file associations and spawn a child process */
void
xdg_open(struct direntry* dir)
{
	char* fname, *ext;
	char cmd[MAXCMDLEN+1];
	int i, associated;
	pid_t pid;

	fname = safealloc(sizeof(*fname) * (strlen(dir->path) + strlen(dir->tree[dir->sel_idx]->name) + 1 + 1));
	sprintf(fname, "%s/%s", dir->path, dir->tree[dir->sel_idx]->name);

	/* Extract the file extension, if one exists */
	associated = 0;
	for(ext = fname; *ext != '.' && *ext != '\0'; ext++);

	/* If the file has an extension, check if it's already associated to a
	 * command to run */
	if(*ext == '.')
		for(i=0; i<LENGTH(associations) && !associated; i++)
			if(!strcmp(associations[i].ext, ext))
			{
				associated = 1;
				strcpy(cmd, associations[i].cmd);
			}

	/* Ask the user for a command to open the file with */
	if(!associated)
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

int
main(int argc, char* argv[])
{
	int i, max_row, max_col;
	wchar_t ch;

	/* Initialize ncurses */
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

	/* Associate status bars with the main direntry */
	associate_dir(main_view + TOP_WIN, main_view[CENTER_WIN].dir);
	associate_dir(main_view + BOT_WIN, main_view[CENTER_WIN].dir);

	init_colors();
	init_windows(main_view, max_row, max_col, MAIN_PERC);
	keypad(main_view[BOT_WIN].win, TRUE);

	/* Initial screen update */
	refresh_listing(main_view + LEFT_WIN, 0);
	refresh_listing(main_view + CENTER_WIN, 1);
	refresh_listing(main_view + RIGHT_WIN, 0);
	print_status_top(main_view + TOP_WIN);
	print_status_bottom(main_view + BOT_WIN);

	/* Main control loop */
	while((ch = wgetch(main_view[BOT_WIN].win)) != 'q')
	{
		/* Call the function associated with the key pressed */
		if(ch == KEY_RESIZE)
			resize_handler();
		for(i=0; i<LENGTH(keys); i++)
			if(ch == keys[i].key)
			{
				keys[i].funct(&keys[i].arg);
				break;
			}

		/* Update the status bars every time a key is pressed */
		print_status_top(main_view + TOP_WIN);
		print_status_bottom(main_view + BOT_WIN);
	}

	/* Terminate ncurses session */
	deinit_windows(main_view);
	endwin();
	return 0;
}

