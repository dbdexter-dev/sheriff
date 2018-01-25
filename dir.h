#ifndef __DIR_H
#define __DIR_H

#include <fcntl.h>
#include <sys/stat.h>

#define MAXLEN 256 /* Defined in dirent.h */

typedef struct fileentry
{
	char name[MAXLEN];
	unsigned long size;
	unsigned char type;
	uid_t uid, gid;
	mode_t mode;
	time_t lastchange;
	struct fileentry* next;
} fileentry_t;

fileentry_t* dirlist(char* path);
int sort_tree(fileentry_t* tree);
int free_tree(fileentry_t* tree);

#endif
