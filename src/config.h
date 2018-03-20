/**
 * User configuration file. Contains the various keybindings and file
 * associations (and possibily other options in the future)
 */

#include <ncurses.h>

typedef struct {
	char *ext;
	char *cmd;
} Assoc;

static int pane_proportions[] = { 1, 4, 2 };

static Assoc associations[] = {
	{ ".pdf",   "zathura"},
	{ ".c",     "nvim"},
	{ ".mkv",   "mpv"},
	{ ".mp3",   "mpv"},
	{ ".flac",  "mpv"},
	{ NULL,     NULL},
};

static Key c_multi[] = {
	{ 'd',          quick_cd,           {0}},
	{ 'w',          rename_cur,         {0}},
	{ 'm',          chmod_cur,          {0}},
	{ '\0',         NULL,               {0}},
};

static Key d_multi[] = {
	{ 'd',          yank_cur,           {.i = 0}},
	{ 'D',          delete_cur,         {.i = 0}},
	{ '\0',         NULL,               {0}},
};

static Key g_multi[] = {
	{ 'g',          abs_highlight,      {.i = 0}},
	{ '\0',         NULL,               {0}},
};

static Key p_multi[] = {
	{ 'p',          paste_cur,          {0}},
	{ 'l',          link_cur,           {0}},
	{ '\0',         NULL,               {0}},
};

static Key y_multi[] = {
	{ 'y',          yank_cur,           {.i = 1}},
	{ '\0',         NULL,               {0}},
};

static Key keys[] = {
	{ 'k',          rel_highlight,      {.i = -1}},
	{ 'j',          rel_highlight,      {.i = +1}},
	{ 'h',          navigate,           {.i = -1}},
	{ 'l',          navigate,           {.i = +1}},
	{ '\n',         navigate,           {.i = +1}},
	{ KEY_UP,       rel_highlight,      {.i = -1}},
	{ KEY_DOWN,     rel_highlight,      {.i = +1}},
	{ KEY_LEFT,     navigate,           {.i = -1}},
	{ KEY_RIGHT,    navigate,           {.i = +1}},
	{ '',         rel_highlight,      {.i = -5}},
	{ '',         rel_highlight,      {.i = +5}},
	{ '',         rel_highlight,      {.i = -20}},
	{ '',         rel_highlight,      {.i = +20}},
	{ '/',          filesearch,         {.i = 0}},
	{ 'n',          filesearch,         {.i = +1}},
	{ 'y',          chain,              {.v = y_multi}},
	{ 'd',          chain,              {.v = d_multi}},
	{ 'c',          chain,              {.v = c_multi}},
	{ 'g',          chain,              {.v = g_multi}},
	{ 'G',          abs_highlight,      {.i = -1}},
	{ 'p',          chain,              {.v = p_multi}},
	{ 'v',          visualmode_toggle,  {0}},
	{ '',         tab_clone,          {0}},
	{ '\t',         rel_tabswitch,      {.i = +1}},
	{ 'x',          tab_delete,         {0}},
	{ '',         refresh_all,        {0}},
	{ 'H',          toggle_hidden,      {0}},
/*	{ '!',          shell_exec,         {0}}, */
	{ '\0',         NULL,               {0}},
};
