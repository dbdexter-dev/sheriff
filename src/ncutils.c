#include <assert.h>
#include <ncurses.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include "backend.h"
#include "ncutils.h"
#include "utils.h"

/* Render a dialog prompt in a specified window. Will return the input string
 * through char *input, or just print msg if input == NULL*/
void
dialog(WINDOW *win, char *input, const char *msg, ...)
{
	va_list ap;

	werase(win);
	wattrset(win, COLOR_PAIR(PAIR_WHITE_DEF));
	wmove(win, 0, 0);

	va_start(ap, msg);
	vwprintw(win, msg, ap);
	va_end(ap);

	if (input) {
		wtimeout(win, -1);
		echo();
		curs_set(1);

		wgetnstr(win, input, MAXCMDLEN);
		werase(win);

		curs_set(0);
		noecho();
		wtimeout(win, UPD_INTERVAL);
	}
}

/* Initialize color pairs */
void
init_colors(void)
{
	init_pair(PAIR_RED_DEF, COLOR_RED, COLOR_WHITE);
	init_pair(PAIR_GREEN_DEF, COLOR_GREEN, -1);
	init_pair(PAIR_BLUE_DEF, COLOR_BLUE, -1);
	init_pair(PAIR_CYAN_DEF, COLOR_CYAN, -1);
	init_pair(PAIR_YELLOW_DEF, COLOR_YELLOW, -1);
	init_pair(PAIR_WHITE_DEF, COLOR_WHITE, -1);
}

/* Change the highlighted line from oidx to nidx */
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
