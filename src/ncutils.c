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
		wtimeout(win, -1);              /* Disable input timeout */
		echo();                         /* Enable input echo */
		curs_set(1);                    /* Show cursor */

		wgetnstr(win, input, MAXCMDLEN);
		werase(win);

		curs_set(0);                    /* Go back to default */
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
	getyx(win, savedrow, savedcol);     /* Save cursor position */

	/* Get the old attribute, un-reverse it, and set it */
	attr = mvwinch(win, oidx, 0) & (A_COLOR | A_ATTRIBUTES) & ~A_REVERSE;
	wchgat(win, -1, attr, PAIR_NUMBER(attr), NULL);

	/* Get the new attribute, reverse it, then set it */
	attr = (mvwinch(win, nidx, 0) & (A_COLOR | A_ATTRIBUTES)) | A_REVERSE;
	wchgat(win, -1, attr, PAIR_NUMBER(attr), NULL);

	wmove(win, savedrow, savedcol);     /* Restore cursor position */
}
