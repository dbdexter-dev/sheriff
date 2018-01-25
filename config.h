#ifndef CONFIG_H
#define CONFIG_H

typedef struct
{
	char key;
	void(*funct)(const Arg* arg);
	const Arg arg;
} Key;


static Key keys[] = {
	{ 'j',      rel_highlight,      {.i = +1}},
	{ 'k',      rel_highlight,      {.i = -1}},
	{ 'h',      exit_directory,     {0}},
	{ 'l',      enter_directory,    {0}},
};


#endif
