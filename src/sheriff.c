#include <assert.h>
#include <dirent.h>
#include <locale.h>
#include <ncurses.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "backend.h"
#include "dir.h"
#include "clipboard.h"
#include "ncutils.h"
#include "tabs.h"
#include "ui.h"
#include "utils.h"

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

char *realpath(const char *path, char *resolved_path);

static int   direct_cd(char *center_path);
static int   enter_directory();
static int   exit_directory();
static void  queue_update();
static void  resize_handler();
static int   tab_select(int idx);
static void  update_reaper();
static void  xdg_open(Direntry *file);

/* Functions that can be used in config.h */
static void  abs_highlight(const Arg *arg);
static void  chain(const Arg *arg);
static void  filesearch(const Arg *arg);
static void  navigate(const Arg *arg);
static void  paste_cur(const Arg *arg);
static void  quick_cd(const Arg *arg);
static void  refresh_all(const Arg *arg);
static void  rel_highlight(const Arg *arg);
static void  rel_tabswitch(const Arg *arg);
static void  rename_cur(const Arg *arg);
static void  tab_clone(const Arg *arg);
static void  tab_delete(const Arg *arg);
static void  visualmode_toggle(const Arg *arg);
static void  yank_cur(const Arg *arg);

/* This is the only variable I can't make non-global
 * since it is needed by the SIGWINCH handler and the various
 * keybind handlers
 */
static Dirview m_view[WIN_NR];
static int cur_tab = 0;
static sem_t m_update_sem;

/* Keybind handlers {{{*/

/* Select an element in the center view by absolute index */
void
abs_highlight(const Arg *arg)
{
	int abs_i, cur_pos, prev_pos;
	char *fullpath;
	const Direntry *dir;

	dir = m_view[CENTER].ctx->dir;

	/* Negative number means "from the bottom up" */
	if (arg->i < 0) {
		abs_i = dir->count - arg->i;
	} else {
		abs_i = arg->i;
	}

	if (abs_i < 0) {
		return;
	}

	abs_i -= m_view[CENTER].ctx->offset;

	prev_pos = dir->sel_idx - m_view[CENTER].ctx->offset;
	cur_pos = try_highlight(m_view + CENTER, abs_i);

	/* Current and previous positions are the same, we got nothing more to do */
	if (cur_pos == prev_pos) {
		return;
	}

	/* If the selected element is a directory, update the right pane
	 * Otherwise, free it so that render_tree will show a blank pane */
	if (S_ISDIR(dir->tree[dir->sel_idx]->mode)) {
		fullpath = join_path(dir->path, dir->tree[dir->sel_idx]->name);
		init_pane_with_path(m_view[RIGHT].ctx, fullpath);
		free(fullpath);
	} else {
		init_pane_with_path(m_view[RIGHT].ctx, NULL);
	}

	render_tree(m_view + RIGHT, 0);

	/* If the directory view doesn't have to be changed, do a simple wrefresh;
	 * otherwise do a full redraw */
	if (check_offset_changed(m_view + CENTER) || m_view[CENTER].ctx->visual) {
		render_tree(m_view + CENTER, 1);
	} else {
		wrefresh(m_view[CENTER].win);
	}

	update_status_top(m_view + TOP);
	update_status_bottom(m_view + BOT);

	return;
}

/* File search, both forwards and backwards, depending on the value of arg->i */
void
filesearch(const Arg *arg)
{
	int i, found;
	char fname[MAXSEARCHLEN+1];
	char *fullpath;
	const Direntry *dir = m_view[CENTER].ctx->dir;

	dialog(m_view[BOT].win, fname, arg->i > 0 ? "/" : "?");
	if (*fname == '\0') {
		return;
	}

	/* Basically, go through all the elements from the currently selected one to
	 * the very [beginning | end], exiting once a match is found. TODO: add the
	 * ability to quickly redo the search */
	found = 0;
	if (arg->i > 0) {
		for (i=m_view[CENTER].ctx->dir->sel_idx+1; i<dir->count; i++) {
			if (strcasestr(dir->tree[i]->name, fname)) {
				m_view[CENTER].ctx->dir->sel_idx = i;
				found = 1;
				break;
			}
		}
	} else {
		for (i=m_view[CENTER].ctx->dir->sel_idx-1; i>=0; i--) {
			if (strcasestr(dir->tree[i]->name, fname)) {
				m_view[CENTER].ctx->dir->sel_idx = i;
				found = 1;
				break;
			}
		}
	}

	/* If a match has been found, update the right pane and repaint it, checking
	 * whether the selected file is a directory */
	if (found) {
		if (S_ISDIR(dir->tree[dir->sel_idx]->mode)) {
			fullpath = join_path(dir->path, dir->tree[dir->sel_idx]->name);
			init_pane_with_path(m_view[RIGHT].ctx, fullpath);
			free(fullpath);
		} else {
			init_pane_with_path(m_view[RIGHT].ctx, NULL);
		}
		render_tree(m_view + RIGHT, 0);
		render_tree(m_view + CENTER, 1);
	}
}


/* Meta-keybinding function, so that you can chain multiple characters together
 * to perform a single action */
void
chain(const Arg *arg)
{
	wchar_t ch;
	int i;
	Key *binds = arg->v;

	wtimeout(m_view[BOT].win, -1);
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
	wtimeout(m_view[BOT].win, UPD_INTERVAL);
}

/* Handle navigation, either forward or backwards, through directories or
 * files */
void
navigate(const Arg *arg)
{
	Direntry *dir = m_view[CENTER].ctx->dir;

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

/* Clone the current tab */
void
tab_clone(const Arg *arg)
{
	const char *path;
	const Arg zero = {.i = 0};

	path = m_view[CENTER].ctx->dir->path;
	tabctx_append(path);

	rel_tabswitch(&zero);
}

/* Delete the current tab, and exit if there are no tabs left */
void
tab_delete(const Arg *arg)
{
	const Arg zero = {.i = 0};

	if (tabctx_remove(cur_tab)) {
		ungetch('q');
	} else {
		rel_tabswitch(&zero);
	}
}

/* Basically, execute what the clipboard says */
void
paste_cur(const Arg *arg)
{
	Direntry *dir = m_view[CENTER].ctx->dir;

	clip_exec(dir->path);
	m_view[CENTER].ctx->visual = 0;

	dialog(m_view[BOT].win, NULL, "Selection pasted");

	render_tree(m_view + CENTER, 1);
}

/* Cd into a specific directory directly */
void
quick_cd(const Arg *arg)
{
	char path[256];

	dialog(m_view[BOT].win, path, "cd: ");
	if (*path == '\0') {
		return;
	} if (direct_cd(path)) {
		dialog(m_view[BOT].win,
		       NULL,
		       "Destination is not a valid directory");
	}
}

void
refresh_all(const Arg *arg)
{
	queue_update();
}

/* Highlight a file in the center window given an offset from the currently
 * highlighted index (offset in arg->i)*/
void
rel_highlight(const Arg *arg)
{
	const Direntry *dir;
	int abs_pos;

	dir = m_view[CENTER].ctx->dir;

	if (arg->i == 0) {
		return;
	}

	abs_pos = dir->sel_idx + arg->i;
	if (abs_pos < 0) {
		abs_pos = 0;
	}
	abs_highlight((Arg*)&abs_pos);
}

void
rel_tabswitch(const Arg *arg)
{
	cur_tab = tab_select(cur_tab - arg->i);
}

/* Rename the currently highlighted file */
void
rename_cur(const Arg *arg)
{
	char dest[256];
	char *realdest;

	/* TODO this will change once bulkrename is implemented */
	clear_dir_selection(m_view[CENTER].ctx->dir);
	m_view[CENTER].ctx->dir->tree[m_view[CENTER].ctx->dir->sel_idx]->selected = 1;
	clip_update(m_view[CENTER].ctx->dir, OP_MOVE);
	m_view[CENTER].ctx->dir->tree[m_view[CENTER].ctx->dir->sel_idx]->selected = 0;

	dialog(m_view[BOT].win, dest, "rename: ");

	if (dest[0] != '\0') {
		realdest = join_path(m_view[CENTER].ctx->dir->path, dest);
		clip_exec(realdest);
		free(realdest);
	}
}

/* Toggle visual selection mode */
void
visualmode_toggle(const Arg *arg)
{
	m_view[CENTER].ctx->visual ^= 1;
	m_view[CENTER].ctx->dir->tree[m_view[CENTER].ctx->dir->sel_idx]->selected ^= 1;
	render_tree(m_view + CENTER, 1);
}

/* Yank the current selection to clipboard */
void
yank_cur(const Arg *arg)
{
	clip_update(m_view[CENTER].ctx->dir, (arg->i == 1 ? OP_COPY : OP_MOVE));
	clear_dir_selection(m_view[CENTER].ctx->dir);
	m_view[CENTER].ctx->visual = 0;

	dialog(m_view[BOT].win, NULL, "Selection yanked");
	render_tree(m_view + CENTER, 1);
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
		status |= init_pane_with_path(m_view[LEFT].ctx, fullpath);
		free(fullpath);
		status |= init_pane_with_path(m_view[CENTER].ctx, path);
		status |= init_pane_with_path(m_view[RIGHT].ctx, path);

		status |= render_tree(m_view + LEFT, 0);
		status |= render_tree(m_view + CENTER, 1);
		status |= render_tree(m_view + RIGHT, 0);

		update_status_top(m_view + TOP);
		update_status_bottom(m_view + BOT);
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
	if (m_view[CENTER].ctx->dir->tree[m_view[CENTER].ctx->dir->sel_idx]->mode == 0) {
		status = 1;
	} else {
		assert(!navigate_fwd(m_view[LEFT].ctx, m_view[CENTER].ctx, m_view[RIGHT].ctx));
		/* Update the top and bottom bars to reflect the change in the center
		 * window, then refresh the main views. The two bar-windows are updated
		 * every time something happens anyway in the main control loop */
		status |= associate_dir(m_view[TOP].ctx, m_view[CENTER].ctx->dir);
		status |= associate_dir(m_view[BOT].ctx, m_view[CENTER].ctx->dir);
		status |= render_tree(m_view + LEFT, 0);
		status |= render_tree(m_view + CENTER, 1);
		status |= render_tree(m_view + RIGHT, 0);

		update_status_top(m_view + TOP);
		update_status_bottom(m_view + BOT);
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
	assert(!navigate_back(m_view[LEFT].ctx, m_view[CENTER].ctx, m_view[RIGHT].ctx));
	/* As in enter_directory, update the directories associated to the top and
	 * bottom bars, and update the views since we've changed their underlying
	 * associations */
	status |= associate_dir(m_view[TOP].ctx, m_view[CENTER].ctx->dir);
	status |= associate_dir(m_view[BOT].ctx, m_view[CENTER].ctx->dir);
	status |= render_tree(m_view + LEFT, 0);
	status |= render_tree(m_view + CENTER, 1);
	status |= render_tree(m_view + RIGHT, 0);

	update_status_top(m_view + TOP);
	update_status_bottom(m_view + BOT);

	return status;
}

/* Signal the updater that it has something to do on the next check */
void
queue_update()
{
	sem_post(&m_update_sem);
	return;
}

/* Handler that takes care of resizing the subviews when KEY_RESIZE is received.
 * This function is one of the reasons why there has to be a global array of
 * Dirview ptrs */
void
resize_handler()
{
	int i, nr, nc, mc, sc_l, sc_r;
	float sum;

	/* Re-initialize ncurses with the correct dimensions */
	werase(stdscr);
	endwin();

	refresh();
	getmaxyx(stdscr, nr, nc);

	sum = pane_proportions[0] + pane_proportions[1] + pane_proportions[2];

	sc_l = nc * pane_proportions[0]/sum;
	mc = nc * pane_proportions[1]/sum;
	sc_r = nc - sc_l - mc;

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

	update_status_top(m_view + TOP);
	render_tree(m_view + LEFT, 0);
	render_tree(m_view + CENTER, 1);
	render_tree(m_view + RIGHT, 0);
	update_status_bottom(m_view + BOT);
}

/* Switch to the idxth tab */
int
tab_select(int idx)
{
	TabCtx *switch_dest;

	switch_dest = tabctx_by_idx(&idx);
	tab_switch(m_view, switch_dest);

	return idx;
}


/* The core updater function, it gets called periodically and checks whether a
 * worker has done something in the background and has requested a current
 * directory rescan */
void
update_reaper()
{
	if (!sem_trywait(&m_update_sem)) {
		rescan_listing(m_view[LEFT].ctx->dir);
		rescan_listing(m_view[CENTER].ctx->dir);
		rescan_listing(m_view[RIGHT].ctx->dir);
		render_tree(m_view + LEFT, 0);
		render_tree(m_view + CENTER, 1);
		render_tree(m_view + RIGHT, 0);
	}
}

/* Just like xdg_open, check file associations and spawn a child process */
void
xdg_open(Direntry *dir)
{
	char *fname, *ext;
	char cmd[MAXCMDLEN+1];
	int i, associated;
	int wstatus;
	pid_t pid;

	fname = join_path(dir->path, dir->tree[dir->sel_idx]->name);
	associated = 0;

	/* Extract the file extension, if one exists. Has to work from the end of
	 * the string backwards so that the very last '.' is considered the
	 * extension delimiter */
	for (ext = fname + strlen(fname); *ext != '.' && ext != fname; ext--)
		;

	/* If the file has an extension, check if it's already associated to a
	 * command to run */
	if (*ext == '.') {
		for (i=0; associations[i].ext != NULL; i++) {
			if (!strcmp(associations[i].ext, ext)) {
				associated = 1;
				strcpy(cmd, associations[i].cmd);
				break;
			}
		}
	}

	/* Ask the user for a command to open the file with */
	if (!associated) {
		dialog(m_view[BOT].win, cmd, "open_with: ");
	}

	/* Leave curses mode */
	def_prog_mode();
	endwin();

	/* Spawn the requested command */
	if (!(pid = fork())) {
		exit(execlp(cmd, cmd, fname, NULL));
	}

	do {
		waitpid(pid, &wstatus, 0);
	} while (!WIFEXITED(wstatus));

	free(fname);

	/* Restore curses mode */
	reset_prog_mode();

	/* Issue warning if the subprocess exited with an error status */
	if (WEXITSTATUS(wstatus)) {
		dialog(m_view[BOT].win, NULL,
		       "Subprocess exited with status %d", WEXITSTATUS(wstatus));
	}

	for (i=0; i<WIN_NR; i++) {
		wrefresh(m_view[i].win);
	}
}

int
main(int argc, char *argv[])
{
	int i, max_row, max_col;
	char *path;
	wchar_t ch;
	struct sigaction update_act;

	setlocale(LC_ALL, "");                 /* Enable unicode goodness */
	clip_init();                           /* Initialize clipboard */
	sem_init(&m_update_sem, 0, 0);         /* Initialize the update semaphore */
	update_act.sa_handler = queue_update;
	sigemptyset(&update_act.sa_mask);
	update_act.sa_flags = 0;
	sigaction(SIGUSR1, &update_act, NULL); /* Allow children to ask for an update */

	/* Initialize ncurses */
	initscr();                             /* Initialize ncurses screen */
	noecho();                              /* Don't echo keys pressed */
	cbreak();                              /* Quasi-raw input */
	curs_set(0);                           /* Hide cursor */
	use_default_colors();                  /* Enable default 16 colors */
	start_color();
	init_colors();

	getmaxyx(stdscr, max_row, max_col);

	windows_init(m_view, max_row, max_col, pane_proportions);
	keypad(m_view[BOT].win, TRUE);

	path = realpath(".", NULL);
	tabctx_append(path);
	free(path);

	tab_select(0);
	/* Main control loop */
	while ((ch = wgetch(m_view[BOT].win)) != 'q') {
		/* Call the function associated with the key pressed */
		switch (ch) {
		case KEY_RESIZE:
			resize_handler();
			break;
		case ERR:
			break;
		default:
			for (i=0; keys[i].key != '\0'; i++) {
				if (ch == keys[i].key) {
					keys[i].funct(&keys[i].arg);
					break;
				}
			}
			break;
		}
		update_reaper();
	}

	/* Terminate ncurses session */
	sem_destroy(&m_update_sem);
	windows_deinit(m_view);
	tabctx_deinit();
	clip_deinit();
	endwin();
	return 0;
}
