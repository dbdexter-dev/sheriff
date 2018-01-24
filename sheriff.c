#include <assert.h>
#include <dirent.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "dir.h"
#include "utils.h"
#include "config.h"

#define MAXHOSTNLEN 32
#define MAXPWDLEN 64
#define MAIN_PERC 0.6
#define WIN_NR 5

#define PAIR_BLUE_DEF 1
#define PAIR_GREEN_DEF 2
#define PAIR_WHITE_DEF 3
#define PAIR_CYAN_DEF 4

enum windows
{
	top_win = 0,
	prev_win = 1,
	curr_win = 2,
	next_win = 3,
	bot_win = 4
};

typedef struct
{
	WINDOW* win;
	fileentry_t* tree;
	int tree_size;
	int sel_idx;
} Dirview;

static void  colors_init();
static int   populate_listing(Dirview* win, char* dir);
static void  print_status(Dirview* win, int y, int x);
static void  resize_handler(int sig);
static int   reverse_line_fg_bg(WINDOW* win, int y, int x);
static int   show_listing(Dirview* win, int show_sizes);
static int   try_highlight(Dirview* win, int idx);
static int   windows_init(Dirview view[WIN_NR], int w, int h, float main_perc);
static int   windows_deinit(Dirview view[WIN_NR]);
static int   windows_prepare_for_resize(Dirview view[WIN_NR]);

Dirview main_view[WIN_NR];

/* Initialize color pairs */
void
colors_init(void)
{
	init_pair(PAIR_BLUE_DEF, COLOR_BLUE, -1);
	init_pair(PAIR_GREEN_DEF, COLOR_GREEN, -1);
	init_pair(PAIR_WHITE_DEF, COLOR_WHITE, -1);
	init_pair(PAIR_CYAN_DEF, COLOR_CYAN, -1);
}

/* Updates the top status line
 * Format: <user>@<host> $PWD/<selected entry>
 */
void 
print_status(Dirview* win, int y, int x)
{
	char* username;
	char* workdir;
	char hostname[MAXHOSTNLEN+1];

	username = getenv("USER");
	workdir = getenv("PWD");
	gethostname(hostname, MAXHOSTNLEN);

	assert(username);
	assert(workdir);
	assert(win->win);

	wattrset(win->win, A_BOLD | COLOR_PAIR(PAIR_BLUE_DEF));
	mvwprintw(win->win, y, x, "%s@%s", username, hostname);
	wattron(win->win, COLOR_PAIR(PAIR_GREEN_DEF));
	wprintw(win->win, " %s/", workdir);
/*	wattron(win->win, COLOR_PAIR(PAIR_WHITE_DEF)); */
/*	wprintw(win->win, "%s", selected_item); */

	wrefresh(win->win);
}

/* Populates a window with a directory listing */
int
populate_listing(Dirview* win, char* dir)
{
	fileentry_t* tmp;

	/* Populate and sort */
	win->tree = dirlist(dir);
	sort_tree(win->tree); 

	/* Find the tree size */
	for(tmp = win->tree; tmp != NULL; tmp=tmp->next, win->tree_size++);
	return 0;
}

void  
resize_handler(int sig)
{
	int nr, nc;

	windows_prepare_for_resize(main_view);
	endwin();

	initscr();
	getmaxyx(stdscr, nr, nc);
	windows_init(main_view, nr, nc, MAIN_PERC);

	print_status(main_view + top_win, 0, 0);
	show_listing(main_view + prev_win, 0);
	show_listing(main_view + curr_win, 1);
}

int
reverse_line_fg_bg(WINDOW* win, int y, int x)
{
	attr_t attr;

	attr = (mvwinch(win, y, x) & A_ATTRIBUTES) ^ A_REVERSE;
	wchgat(win, -1, attr, 0, NULL);

	return 0;
}

/* Render a directory listing on a window */
int
show_listing(Dirview* win, int show_sizes)
{
	int maxrows, maxcols, rowcount;
	char* tmpstring;
	const fileentry_t* tmptree;

	assert(win->win);
	assert(win->tree);

	/* Allocate enough space to fit the shortened listing names */
	getmaxyx(win->win, maxrows, maxcols);
	tmpstring = safealloc(sizeof(char) * maxcols);

	/* Go to the top corner */
	wmove(win->win, 0, 0);
	wclear(win->win);
	
	rowcount = 0;
	/* Read up to $maxrows entries */
	for(tmptree = win->tree; (tmptree != NULL) && rowcount < maxrows; tmptree = tmptree->next)
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
			/* TODO: print file size if we're asked to 
			 * So, chomp string to maxcols - 1 - (space reserved to size) */
			wprintw(win->win, "%s", tmptree->name);
			wprintw(win->win, "\n");
		}
		/* if(rowcount == win->sel_idx)
			try_highlight(win, rowcount);

		*/
		rowcount++;
	}

	free(tmpstring);
	wrefresh(win->win);

	return 0;
}

int
try_highlight(Dirview* win, int idx)
{
	return 0;
	if(idx == win->sel_idx)
		return idx;

	reverse_line_fg_bg(win->win, win->sel_idx, 0);

	if(idx > win->tree_size - 1)
		win->sel_idx = win->tree_size - 1;
	else if (idx < 0)
		win->sel_idx = 0;
	else
		win->sel_idx = idx;

	reverse_line_fg_bg(win->win, win->sel_idx, 0);

	return win->sel_idx;
}
/* Initialize the windows that make up the main view */
int
windows_init(Dirview view[WIN_NR], int row, int col, float main_perc)
{
	int main_cols, side_cols;

	if(!view)
		return -1;

	main_cols = col * main_perc;
	side_cols = (col - main_cols) / 2;

	view[top_win].win = newwin(1, col, 0, 0);
	view[bot_win].win = newwin(1, col, row - 1, 0);
	view[prev_win].win = newwin(row - 2, side_cols, 1, 1);
	view[curr_win].win = newwin(row - 2, main_cols, 1, 1 + side_cols);
	view[next_win].win = newwin(row - 2, side_cols, 1, 1 + side_cols + main_cols);

	return 0;
}

int
windows_deinit(Dirview view[WIN_NR])
{
	int i;

	assert(view);
	/* Deallocate directory listings */
	for(i=0; i<WIN_NR; i++)
		free_tree(view[i].tree);

	windows_prepare_for_resize(view);
	return 0;
}

int
windows_prepare_for_resize(Dirview view[WIN_NR])
{
	int i;

	assert(view);
	/* Deallocate directory listings */
	for(i=0; i<WIN_NR; i++)
		delwin(view[i].win);

	return 0;
}

int
main(int argc, char* argv[])
{
	int max_row, max_col;
	int i;
	char ch;
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

	/* Initialize colorschemes */
	colors_init();
	/* Initialize windows */
	windows_init(main_view, max_row, max_col, MAIN_PERC);

	for(i=0; i<WIN_NR; i++)
		main_view[i].sel_idx = 0;

	refresh();

	print_status(main_view + top_win, 0, 0);
	populate_listing(main_view + prev_win, "../");
	populate_listing(main_view + curr_win, "./");

	show_listing(main_view + prev_win, false);
	show_listing(main_view + curr_win, false);

	while((ch = wgetch(main_view[curr_win].win)) != 'q')
	{
		switch(ch)
		{
			case 'j':
				cur_highlight = try_highlight(main_view + curr_win, ++cur_highlight);
				break;
			case 'k':
				cur_highlight = try_highlight(main_view + curr_win, --cur_highlight);
				break;
			case 'h':
				break;
			case 'l':
				break;
			default:
				break;
		}
	}
	/* Terminate ncurses session */
	windows_deinit(main_view);
	endwin();
	return 0;
}

