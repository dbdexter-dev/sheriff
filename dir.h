#ifndef DIR_H
#define DIR_H

#include <fcntl.h>
#include <sys/stat.h>

#define MAXLEN 256 /* Defined in dirent.h */

typedef struct fileentry
{
	char name[MAXLEN];
	unsigned long size;
	char type;
	uid_t uid, gid;
	mode_t mode;
	time_t lastchange;
	struct fileentry* next;
} fileentry_t;

struct direntry
{
	char* path;             /* Path this struct represents */
	fileentry_t* tree;      /* Linked list of all files */
	int tree_size;          /* Number of entries in the list */
	fileentry_t* sel;       /* Selected entry shorthand */
	int sel_idx;            /* Selected entry index */
};

int init_listing(struct direntry** direntry, char* path);
int free_listing(struct direntry** direntry);
int try_select(struct direntry* direntry, int idx);

#endif
