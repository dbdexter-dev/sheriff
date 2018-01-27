/**
 * User configuration file. Contains the various keybindings (and possibily
 * other options in the future)
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <ncurses.h>

typedef struct
{
	int key;
	void(*funct)(const Arg* arg);
	const Arg arg;
} Key;

typedef struct
{
	char* ext;
	char* cmd;
} Assoc;

static Assoc associations[] = {
	{ ".pdf",   "zathura"},
	{ ".c",     "nvim"},
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
};

#endif
