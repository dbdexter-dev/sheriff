/**
 * User configuration file. Contains the various keybindings (and possibily
 * other options in the future)
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <ncurses.h>

typedef struct
{
	wchar_t key;
	void(*funct)(const Arg* arg);
	const Arg arg;
} Key;


static Key keys[] = {
	{ 'j',          rel_highlight,      {.i = +1}},
	{ 'k',          rel_highlight,      {.i = -1}},
	{ 'h',          exit_directory,     {0}},
	{ 'l',          enter_directory,    {0}},
	{ KEY_DOWN,     rel_highlight,      {.i = +1}},
	{ KEY_UP,       rel_highlight,      {.i = -1}},
	{ KEY_LEFT,     exit_directory,     {0}},
	{ KEY_RIGHT,    enter_directory,    {0}},
	{ '/',          filesearch,         {.i = +1}},
	{ '?',          filesearch,         {.i = -1}},
};


#endif
