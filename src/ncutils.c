#include <assert.h>
#include <ncurses.h>
#include <time.h>
#include <unistd.h>
#include "backend.h"
#include "ncutils.h"
#include "utils.h"

/* Render a dialog prompt in a specified window. Will return the input string
 * through char *input, or just print msg if input == NULL*/
void
dialog(WINDOW *win, char *msg, char *input)
{
	werase(win);
	wattrset(win, COLOR_PAIR(PAIR_WHITE_DEF));
	mvwprintw(win, 0, 0, msg);

	if (input) {
		echo();
		curs_set(1);

		wgetnstr(win, input, MAXCMDLEN);
		werase(win);

		curs_set(0);
		noecho();
	}
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

/* Update the bottom status bar. Format: <permissions>  <uid>  <gid>  <last
 * modified> */
void
print_status_bottom(WINDOW *win, mode_t raw_m, struct tm *mtime, int uid, int gid)
{
	char mode[10+1];                /* Something like -rwxr-xr-x */
	char last_modified[MAXDATELEN+1];

	octal_to_str(raw_m, mode);
	strftime(last_modified, MAXDATELEN, "%F %R", mtime);

	assert(win);
	werase(win);
	wattrset(win, COLOR_PAIR(PAIR_GREEN_DEF));
	mvwprintw(win, 0, 0, "%s ", mode);
	wattrset(win, COLOR_PAIR(PAIR_WHITE_DEF));
	wprintw(win," %d  %d  %s", uid, gid, last_modified);

	wrefresh(win);
}

/* Updates the top status bar. Format: <user>@<host> $PWD/<highlighted entry> */
void
print_status_top(WINDOW *win, char *user, char *wd, char *hostn, char *hi)
{
	werase(win);
	wattron(win, A_BOLD | COLOR_PAIR(PAIR_BLUE_DEF));
	mvwprintw(win, 0, 0, "%s@%s", user, hostn);
	wattron(win, COLOR_PAIR(PAIR_GREEN_DEF));
	wprintw(win, " %s/", wd);
	wattron(win, COLOR_PAIR(PAIR_WHITE_DEF));
	wprintw(win, "%s", hi);

	wrefresh(win);
}


void
change_highlight(WINDOW *win, int oidx, int nidx)
{
	int savedrow, savedcol;
	attr_t attr;

	assert(win);
	getyx(win, savedrow, savedcol);

	attr = mvwinch(win, oidx, 0) & (A_COLOR | A_ATTRIBUTES) & ~A_REVERSE;
	wchgat(win, -1, attr, PAIR_NUMBER(attr), NULL);
	attr = (mvwinch(win, nidx, 0) & (A_COLOR | A_ATTRIBUTES)) | A_REVERSE;
	wchgat(win, -1, attr, PAIR_NUMBER(attr), NULL);

	wmove(win, savedrow, savedcol);
	return;
}
