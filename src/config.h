/**
 * User configuration file. Contains the various keybindings (and possibily
 * other options in the future)
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <ncurses.h>

typedef struct
{
	char* ext;
	char* cmd;
} Assoc;

static Assoc associations[] = {
	{ ".pdf",   "zathura"},
	{ ".c",     "nvim"},
	{ NULL,     NULL},
};

static Key c_multi[] = {
	{ 'd',          quick_cd,           {0}},
	{ '\0',         NULL,               {0}},
};

static Key d_multi[] = {
	{ 'd',          yank_cur,           {.i = 0}},
	{ '\0',         NULL,               {0}},
};

static Key y_multi[] = {
	{ 'y',          yank_cur,           {.i = 1}},
	{ '\0',         NULL,               {0}},
};

static Key keys[] = {
	{ 'j',          rel_highlight,      {.i = +1}},
	{ 'k',          rel_highlight,      {.i = -1}},
	{ 'h',          navigate,           {.i = -1}},
	{ 'l',          navigate,           {.i = +1}},
	{ KEY_UP,       rel_highlight,      {.i = -1}},
	{ KEY_DOWN,     rel_highlight,      {.i = +1}},
	{ KEY_LEFT,     navigate,           {.i = -1}},
	{ KEY_RIGHT,    navigate,           {.i = +1}},
	{ '/',          filesearch,         {.i = +1}},
	{ '?',          filesearch,         {.i = -1}},
	{ 'y',          multibind,          {.v = y_multi}},
	{ 'd',          multibind,          {.v = d_multi}},
	{ 'c',          multibind,          {.v = c_multi}},
	{ 'p',          paste_cur,          {0}},
/*	{ '!',          shell_exec,         {0}}, */
	{ '\0',         NULL,               {0}},
};

#endif
