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
#include "fileops.h"
#include "ncutils.h"
#include "utils.h"

#define MAIN_PERC 0.6
#define MAXSEARCHLEN MAXCMDLEN

typedef union {
	int i;
	void *v;
} Arg;

typedef struct {
	int key;
	void(*funct)(const Arg *arg);
	const Arg arg;
} Key;


static int   direct_cd(char *center_path);
static int   enter_directory();
static int   exit_directory();
static void  resize_handler();
static void  xdg_open(Direntry *file);

static void  filesearch(const Arg *arg);
static void  multibind(const Arg *arg);
static void  navigate(const Arg *arg);
static void  paste_cur(const Arg *arg);
static void  quick_cd(const Arg *arg);
static void  rel_highlight(const Arg *arg);
static void  yank_cur(const Arg *arg);

/* This is the only variable I can't make non-global
 * since it is needed by the SIGWINCH handler and the various
 * keybind handlers
 */
static Dirview mainview[WIN_NR];
static Filebuffer yankbuf;

/* Keybind handlers {{{*/

/* File search, both forwards and backwards, depending on the value of arg->i */
void
filesearch(const Arg *arg)
{
	int i, found;
	char fname[MAXSEARCHLEN+1];
	char *fullpath;
	const Direntry *dir = mainview[CENTER].dir;

	dialog(mainview + BOT, arg->i > 0 ? "/" : "?", fname);
	if (*fname == '\0') {
		return;
	}

	/* Basically, go through all the elements from the currently selected one to
	 * the very [beginning | end], exiting once a match is found. TODO: add the
	 * ability to quickly redo the search */
	found = 0;
	if (arg->i > 0) {
		for (i=mainview[CENTER].dir->sel_idx+1; i<dir->count; i++) {
			if (strcasestr(dir->tree[i]->name, fname)) {
				mainview[CENTER].dir->sel_idx = i;
				found = 1;
				break;
			}
		}
	} else {
		for (i=mainview[CENTER].dir->sel_idx-1; i>=0; i--) {
			if (strcasestr(dir->tree[i]->name, fname)) {
				mainview[CENTER].dir->sel_idx = i;
				found = 1;
				break;
			}
		}
	}

	/* If a match has been found, update the right pane and repaint it */
	if (found) {
		fullpath = join_path(dir->path, dir->tree[dir->sel_idx]->name);
		update_win_with_path(mainview + RIGHT, fullpath);
		free(fullpath);
		refresh_listing(mainview + RIGHT, 0);
		refresh_listing(mainview + CENTER, 1);
	}
}


/* Meta-keybinding function, so that you can chain multiple characters together
 * to perform a single action */
void
multibind(const Arg *arg)
{
	wchar_t ch;
	int i;
	Key *binds = arg->v;

	do {
		/* Handle resize events while waiting for input */
		ch = wgetch(mainview[BOT].win);
		if (ch == KEY_RESIZE) {
			resize_handler();
		} else if (ch != KEY_EXIT) {
			/* Exit means exit, duh */
			for (i=0; binds[i].key != '\0'; i++) {
				if (ch == binds[i].key) {
					binds[i].funct(&binds[i].arg);
				}
			}
		}
	} while (ch == KEY_RESIZE);
}

/* Handle navigation, either forward or backwards, through directories or
 * files */
void
navigate(const Arg *arg)
{
	Direntry *dir = mainview[CENTER].dir;
	if (arg->i == 0) {
		return;
	}

	/* Forward navigation */
	if (arg->i > 0) {
		if (S_ISDIR(dir->tree[dir->sel_idx]->mode)) {
			enter_directory();
		} else if (dir->tree[dir->sel_idx]->mode != 0) {
			xdg_open(dir);
		}
	}
	/* Backward navigation */
	else if (arg->i < 0) {
		exit_directory();
	}
}

void
paste_cur(const Arg *arg)
{
	char *tmp;
	Direntry *dir = mainview[CENTER].dir;

	if (!yankbuf.file) {
		/* No file selected, exit */
		return;
	}

	if (move_file(yankbuf.file, dir->path, yankbuf.preserve_src)) {
		die("Paste failed!");
	}

	/* Clear yank buffer */
	free(yankbuf.file);
	yankbuf.file = NULL;

	dialog(mainview + BOT, "Selection pasted", NULL);

	/* Temporarily save the path we're in since we'll be freed during the
	 * update_win_with_path, but we want dir->path to exist long enough for the
	 * window to be initialized with it */
	tmp = safealloc(sizeof(*tmp) * (strlen(dir->path) + 1));
	strcpy(tmp, dir->path);
	update_win_with_path(mainview + CENTER, tmp);
	free(tmp);

	associate_dir(mainview + TOP, mainview[CENTER].dir);
	associate_dir(mainview + BOT, mainview[CENTER].dir);
	refresh_listing(mainview + CENTER, 1);
}

/* Cd into a specific directory directly */
void
quick_cd(const Arg *arg)
{
	char path[256];

	dialog(mainview + BOT, "cd: ", path);
	if (*path == '\0') {
		return;
	} if (direct_cd(path)) {
		dialog(mainview + BOT,
		       "Destination is not a valid directory",
		       NULL);
	}
}
/* Highlight a file in the center window given an offset from the currently
 * highlighted index (offset in arg->i)*/
void
rel_highlight(const Arg *arg)
{
	const Direntry *dir = mainview[CENTER].dir;
	char *fullpath;
	int cur_pos, prev_pos;

	if (arg->i == 0) {
		return;
	}

	/* Account for possible offsets in the window */
	prev_pos = dir->sel_idx - mainview[CENTER].offset;
	cur_pos = try_highlight(mainview + CENTER, prev_pos + arg->i);

	/* If the selected element is a directory, update the right pane
	 * Otherwise, free it so that refresh_listing will show a blank pane */
	if (S_ISDIR(dir->tree[dir->sel_idx]->mode)) {
		fullpath = join_path(dir->path, dir->tree[dir->sel_idx]->name);
		update_win_with_path(mainview + RIGHT, fullpath);
		free(fullpath);
	} else {
		update_win_with_path(mainview + RIGHT, NULL);
	}

	/* Update only if we actually moved inside the window */
	if (cur_pos != prev_pos) {
		refresh_listing(mainview + RIGHT, 0);
	}

	/* If the page offset doesn't have to be changed, do a simple wrefresh;
	 * otherwise do a full redraw */
	if (check_offset_changed(mainview + CENTER)) {
		refresh_listing(mainview + CENTER, 1);
	} else {
		wrefresh(mainview[CENTER].win);
	}

	print_status_top(mainview + TOP);
	print_status_bottom(mainview + BOT);
}

void
yank_cur(const Arg *arg)
{
	const Direntry *dir;
	const char *cw_wd, *cw_selname;

	dir = mainview[CENTER].dir;
	cw_wd = dir->path;
	cw_selname = dir->tree[dir->sel_idx]->name;

	if (yankbuf.file) {
		/* Clear the current yank buffer */
		free(yankbuf.file);
	}

	if(!dir->tree[dir->sel_idx]->mode) {
		/* This is not a file, bail out */
		return;
	}

	yankbuf.file = safealloc(sizeof(*yankbuf.file) * (strlen(cw_wd) +
	                                                  strlen(cw_selname) +
	                                                  1 + 1));
	sprintf(yankbuf.file, "%s/%s", cw_wd, cw_selname);
	yankbuf.preserve_src = arg->i;

	dialog(mainview + BOT, "Selection yanked", NULL);
}
/*}}}*/
#include "config.h"

/* Given a path, check whether it's a directory. If it is, cd into it and
 * refresh all the views */
int
direct_cd(char *path)
{
	char *fullpath;
	int status;
	struct stat st;

	status = 0;

	if (!stat(path, &st) && S_ISDIR(st.st_mode)) {
		fullpath = join_path(path, "../");
		status |= update_win_with_path(mainview + LEFT, fullpath);
		status |= update_win_with_path(mainview + CENTER, fullpath);
		status |= update_win_with_path(mainview + RIGHT, fullpath);
		free(fullpath);
		status |= associate_dir(mainview + BOT, mainview[CENTER].dir);
		status |= associate_dir(mainview + TOP, mainview[CENTER].dir);
		status |= refresh_listing(mainview + LEFT, 0);
		status |= refresh_listing(mainview + CENTER, 1);
		status |= refresh_listing(mainview + RIGHT, 0);

		print_status_top(mainview + TOP);
		print_status_bottom(mainview + BOT);
	} else {
		status = 1;
	}

	return status;
}

/* Navigate to the directory selected in the center window */
int
enter_directory()
{
	int status;

	status = 0;
	/* If type < 0, it's not even a file, but a message (e.g. "inaccessible"):
	 * don't attempt a cd, just exit */
	if (mainview[CENTER].dir->tree[mainview[CENTER].dir->sel_idx]->mode == 0) {
		status = 1;
	} else {
		if (navigate_fwd(mainview + LEFT, mainview + CENTER, mainview + RIGHT))
			die("Couldn't navigate_fwd");
		/* Update the top and bottom bars to reflect the change in the center
		 * window, then refresh the main views. The two bar-windows are updated
		 * every time something happens anyway in the main control loop */
		status |= associate_dir(mainview + TOP, mainview[CENTER].dir);
		status |= associate_dir(mainview + BOT, mainview[CENTER].dir);
		status |= refresh_listing(mainview + LEFT, 0);
		status |= refresh_listing(mainview + CENTER, 1);
		status |= refresh_listing(mainview + RIGHT, 0);

		print_status_top(mainview + TOP);
		print_status_bottom(mainview + BOT);
	}
	return status;
}

/* Navigate backwards in the directory tree, into the parent of the center
 * window */
int
exit_directory()
{
	int status;

	status = 0;
	if (navigate_back(mainview + LEFT, mainview + CENTER, mainview + RIGHT))
		die("Couldn't navigate_back");
	/* As in enter_directory, update the directories associated to the top and
	 * bottom bars, and update the views since we've changed their underlying
	 * associations */
	status |= associate_dir(mainview + TOP, mainview[CENTER].dir);
	status |= associate_dir(mainview + BOT, mainview[CENTER].dir);
	status |= refresh_listing(mainview + LEFT, 0);
	status |= refresh_listing(mainview + CENTER, 1);
	status |= refresh_listing(mainview + RIGHT, 0);

	print_status_top(mainview + TOP);
	print_status_bottom(mainview + BOT);

	return status;
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

	mc = nc  *MAIN_PERC;
	sc_l = (nc - mc) / 2;
	sc_r = nc - mc - sc_l;

	for (i=0; i<WIN_NR; i++) {
		mvwin(mainview[i].win, 0, 0);
	}

	wresize(mainview[TOP].win, 1, nc);
	mvwin(mainview[TOP].win, 0, 0);

	wresize(mainview[BOT].win, 1, nc);
	mvwin(mainview[BOT].win, nr - 1, 0);

	wresize(mainview[LEFT].win, nr - 2, sc_l - 1);
	mvwin(mainview[LEFT].win, 1, 0);

	wresize(mainview[CENTER].win, nr - 2, mc - 1);
	mvwin(mainview[CENTER].win, 1, sc_l);

	wresize(mainview[RIGHT].win, nr - 2, sc_r - 1);
	mvwin(mainview[RIGHT].win, 1, sc_l + mc);

	print_status_top(mainview + TOP);
	refresh_listing(mainview + LEFT, 0);
	refresh_listing(mainview + CENTER, 1);
	refresh_listing(mainview + RIGHT, 0);
	print_status_bottom(mainview + BOT);
}


/* Just like xdg_open, check file associations and spawn a child process */
void
xdg_open(Direntry *dir)
{
	char *fname, *ext;
	char cmd[MAXCMDLEN+1];
	int i, associated;
	pid_t pid;

	fname = safealloc(sizeof(*fname) * (strlen(dir->path) +
	                                    strlen(dir->tree[dir->sel_idx]->name) +
	                                    1 + 1));
	sprintf(fname, "%s/%s", dir->path, dir->tree[dir->sel_idx]->name);

	associated = 0;

	/* Extract the file extension, if one exists */
	for (ext = fname; *ext != '.' && *ext != '\0'; ext++)
		;

	/* If the file has an extension, check if it's already associated to a
	 * command to run */
	if (*ext == '.') {
		for (i=0; associations[i].ext != NULL && !associated; i++) {
			if (!strcmp(associations[i].ext, ext)) {
				associated = 1;
				strcpy(cmd, associations[i].cmd);
			}
		}
	}

	/* Ask the user for a command to open the file with */
	if (!associated) {
		dialog(mainview + BOT,  "open_with: ", cmd);
	}

	/* Leave curses mode */
	def_prog_mode();
	endwin();

	/* Spawn the requested command */
	if (!(pid = fork())) {
		execlp(cmd, cmd, fname, NULL);
		exit(0);
	} else {
		waitpid(pid, NULL, 0);
	}

	/* Restore curses mode */
	reset_prog_mode();
	free(fname);
}

int
main(int argc, char *argv[])
{
	int i, max_row, max_col;
	char *path;
	wchar_t ch;

	/* Initialize the yank buffer */
	memset(&yankbuf, '\0', sizeof(yankbuf));

	/* Initialize ncurses */
	initscr();                            /* Initialize ncurses sesion */
	noecho();                             /* Don't echo keys pressed */
	cbreak();                             /* Quasi-raw input */
	curs_set(0);                          /* Hide cursor */
	use_default_colors();                 /* Enable default 16 colors */
	start_color();
	init_colors();

	getmaxyx(stdscr, max_row, max_col);

	init_windows(mainview, max_row, max_col, MAIN_PERC);
	keypad(mainview[BOT].win, TRUE);

	path = realpath(".", NULL);
	assert(!direct_cd(path));
	free(path);

	/* Main control loop */
	while ((ch = wgetch(mainview[BOT].win)) != 'q') {
		/* Call the function associated with the key pressed */
		if (ch == KEY_RESIZE) {
			resize_handler();
		}

		for (i=0; keys[i].key != '\0'; i++) {
			if (ch == keys[i].key) {
				keys[i].funct(&keys[i].arg);
				break;
			}
		}

	}

	/* Terminate ncurses session */
	deinit_windows(mainview);
	endwin();
	return 0;
}

