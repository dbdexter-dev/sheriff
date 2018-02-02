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
static void  visualmode_toggle(const Arg *arg);
static void  yank_cur(const Arg *arg);

/* This is the only variable I can't make non-global
 * since it is needed by the SIGWINCH handler and the various
 * keybind handlers
 */
static Dirview m_view[WIN_NR];
static Clipboard m_clip;

/* Keybind handlers {{{*/

/* File search, both forwards and backwards, depending on the value of arg->i */
void
filesearch(const Arg *arg)
{
	int i, found;
	char fname[MAXSEARCHLEN+1];
	char *fullpath;
	const Direntry *dir = m_view[CENTER].dir;

	dialog(m_view + BOT, arg->i > 0 ? "/" : "?", fname);
	if (*fname == '\0') {
		return;
	}

	/* Basically, go through all the elements from the currently selected one to
	 * the very [beginning | end], exiting once a match is found. TODO: add the
	 * ability to quickly redo the search */
	found = 0;
	if (arg->i > 0) {
		for (i=m_view[CENTER].dir->sel_idx+1; i<dir->count; i++) {
			if (strcasestr(dir->tree[i]->name, fname)) {
				m_view[CENTER].dir->sel_idx = i;
				found = 1;
				break;
			}
		}
	} else {
		for (i=m_view[CENTER].dir->sel_idx-1; i>=0; i--) {
			if (strcasestr(dir->tree[i]->name, fname)) {
				m_view[CENTER].dir->sel_idx = i;
				found = 1;
				break;
			}
		}
	}

	/* If a match has been found, update the right pane and repaint it */
	if (found) {
		fullpath = join_path(dir->path, dir->tree[dir->sel_idx]->name);
		update_win_with_path(m_view + RIGHT, fullpath);
		free(fullpath);
		refresh_listing(m_view + RIGHT, 0);
		refresh_listing(m_view + CENTER, 1);
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
		ch = wgetch(m_view[BOT].win);
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
	Direntry *dir = m_view[CENTER].dir;
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
	Direntry *dir = m_view[CENTER].dir;

	clip_exec(&m_clip, dir->path);
	clip_clear(&m_clip);
	visualmode_toggle(NULL);

	dialog(m_view + BOT, "Selection pasted", NULL);

	/* Temporarily save the path we're in since we'll be freed during the
	 * update_win_with_path, but we want dir->path to exist long enough for the
	 * window to be initialized with it */
	tmp = safealloc(sizeof(*tmp) * (strlen(dir->path) + 1));
	strcpy(tmp, dir->path);
	update_win_with_path(m_view + CENTER, tmp);
	free(tmp);

	associate_dir(m_view + TOP, m_view[CENTER].dir);
	associate_dir(m_view + BOT, m_view[CENTER].dir);
	refresh_listing(m_view + CENTER, 1);
}

/* Cd into a specific directory directly */
void
quick_cd(const Arg *arg)
{
	char path[256];

	dialog(m_view + BOT, "cd: ", path);
	if (*path == '\0') {
		return;
	} if (direct_cd(path)) {
		dialog(m_view + BOT,
		       "Destination is not a valid directory",
		       NULL);
	}
}
/* Highlight a file in the center window given an offset from the currently
 * highlighted index (offset in arg->i)*/
/* TODO visual selection not updating until you get out of it */
void
rel_highlight(const Arg *arg)
{
	const Direntry *dir = m_view[CENTER].dir;
	char *fullpath;
	int cur_pos, prev_pos;

	if (arg->i == 0) {
		return;
	}

	/* Account for possible offsets in the window */
	prev_pos = dir->sel_idx - m_view[CENTER].offset;
	cur_pos = try_highlight(m_view + CENTER, prev_pos + arg->i);

	/* If the selected element is a directory, update the right pane
	 * Otherwise, free it so that refresh_listing will show a blank pane */
	if (S_ISDIR(dir->tree[dir->sel_idx]->mode)) {
		fullpath = join_path(dir->path, dir->tree[dir->sel_idx]->name);
		update_win_with_path(m_view + RIGHT, fullpath);
		free(fullpath);
	} else {
		update_win_with_path(m_view + RIGHT, NULL);
	}

	/* Update only if we actually moved inside the window */
	if (cur_pos != prev_pos) {
		refresh_listing(m_view + RIGHT, 0);
	}

	/* If the directory viuw doesn't have to be changed, do a simple wrefresh;
	 * otherwise do a full redraw */
	if (check_offset_changed(m_view + CENTER) || m_view[CENTER].visual) {
		refresh_listing(m_view + CENTER, 1);
	} else {
		wrefresh(m_view[CENTER].win);
	}

	print_status_top(m_view + TOP);
	print_status_bottom(m_view + BOT);
}

void
visualmode_toggle(const Arg *arg)
{
	m_view[CENTER].visual ^= 1;
	m_view[BOT].visual = m_view[CENTER].visual;
	m_view[CENTER].dir->tree[m_view[CENTER].dir->sel_idx]->selected ^= 1;
	refresh_listing(m_view + CENTER, 1);
}

void
yank_cur(const Arg *arg)
{
	clip_init(&m_clip, m_view[CENTER].dir, (arg->i == 1 ? OP_COPY : OP_MOVE));
	clear_dir_selection(m_view[CENTER].dir);

	dialog(m_view + BOT, "Selection yanked", NULL);
	refresh_listing(m_view + CENTER, 1);
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
		status |= update_win_with_path(m_view + LEFT, fullpath);
		status |= update_win_with_path(m_view + CENTER, path);
		status |= update_win_with_path(m_view + RIGHT, path);
		free(fullpath);
		status |= associate_dir(m_view + BOT, m_view[CENTER].dir);
		status |= associate_dir(m_view + TOP, m_view[CENTER].dir);
		status |= refresh_listing(m_view + LEFT, 0);
		status |= refresh_listing(m_view + CENTER, 1);
		status |= refresh_listing(m_view + RIGHT, 0);

		print_status_top(m_view + TOP);
		print_status_bottom(m_view + BOT);
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
	if (m_view[CENTER].dir->tree[m_view[CENTER].dir->sel_idx]->mode == 0) {
		status = 1;
	} else {
		if (navigate_fwd(m_view + LEFT, m_view + CENTER, m_view + RIGHT))
			die("Couldn't navigate_fwd");
		/* Update the top and bottom bars to reflect the change in the center
		 * window, then refresh the main views. The two bar-windows are updated
		 * every time something happens anyway in the main control loop */
		status |= associate_dir(m_view + TOP, m_view[CENTER].dir);
		status |= associate_dir(m_view + BOT, m_view[CENTER].dir);
		status |= refresh_listing(m_view + LEFT, 0);
		status |= refresh_listing(m_view + CENTER, 1);
		status |= refresh_listing(m_view + RIGHT, 0);

		print_status_top(m_view + TOP);
		print_status_bottom(m_view + BOT);
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
	if (navigate_back(m_view + LEFT, m_view + CENTER, m_view + RIGHT))
		die("Couldn't navigate_back");
	/* As in enter_directory, update the directories associated to the top and
	 * bottom bars, and update the views since we've changed their underlying
	 * associations */
	status |= associate_dir(m_view + TOP, m_view[CENTER].dir);
	status |= associate_dir(m_view + BOT, m_view[CENTER].dir);
	status |= refresh_listing(m_view + LEFT, 0);
	status |= refresh_listing(m_view + CENTER, 1);
	status |= refresh_listing(m_view + RIGHT, 0);

	print_status_top(m_view + TOP);
	print_status_bottom(m_view + BOT);

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
		mvwin(m_view[i].win, 0, 0);
	}

	wresize(m_view[TOP].win, 1, nc);
	mvwin(m_view[TOP].win, 0, 0);

	wresize(m_view[BOT].win, 1, nc);
	mvwin(m_view[BOT].win, nr - 1, 0);

	wresize(m_view[LEFT].win, nr - 2, sc_l - 1);
	mvwin(m_view[LEFT].win, 1, 0);

	wresize(m_view[CENTER].win, nr - 2, mc - 1);
	mvwin(m_view[CENTER].win, 1, sc_l);

	wresize(m_view[RIGHT].win, nr - 2, sc_r - 1);
	mvwin(m_view[RIGHT].win, 1, sc_l + mc);

	print_status_top(m_view + TOP);
	refresh_listing(m_view + LEFT, 0);
	refresh_listing(m_view + CENTER, 1);
	refresh_listing(m_view + RIGHT, 0);
	print_status_bottom(m_view + BOT);
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

	/* Extract the file extension, if one exists. Has to work from the end of
	 * the string backwards so that the very last '.' is considered the
	 * extension delimiter */
	for (ext = fname + strlen(fname); *ext != '.' && ext != fname; ext--)
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
		dialog(m_view + BOT,  "open_with: ", cmd);
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
	memset(&m_clip, '\0', sizeof(m_clip));

	/* Initialize ncurses */
	initscr();                            /* Initialize ncurses sesion */
	noecho();                             /* Don't echo keys pressed */
	cbreak();                             /* Quasi-raw input */
	curs_set(0);                          /* Hide cursor */
	use_default_colors();                 /* Enable default 16 colors */
	start_color();
	init_colors();

	getmaxyx(stdscr, max_row, max_col);

	init_windows(m_view, max_row, max_col, MAIN_PERC);
	keypad(m_view[BOT].win, TRUE);

	path = realpath(".", NULL);
	assert(!direct_cd(path));
	free(path);

	/* Main control loop */
	while ((ch = wgetch(m_view[BOT].win)) != 'q') {
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
	deinit_windows(m_view);
	endwin();
	return 0;
}

