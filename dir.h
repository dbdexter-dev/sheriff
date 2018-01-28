/**
 * The main directory manipulation functions are here. Most of the low-level
 * ones are declared static inside dir.c, while the ones listed here are
 * expected to be used inside sheriff.h and backend.h when needed.
 */
#ifndef DIR_H
#define DIR_H

#define _GNU_SOURCE         /* This way NAME_MAX is defined */

#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <sys/types.h>


typedef struct fileentry
{
	char name[NAME_MAX+1];  /* NAME_MAX defined in dirent.h */
	unsigned long size;
	uid_t uid, gid;
	mode_t mode;
	time_t lastchange;
} fileentry_t;

struct direntry
{
	char* path;             /* Path this struct represents */
	fileentry_t** tree;     /* Array of file metadata */
	int count;              /* Number of entries in the list */
	int sel_idx;            /* Selected entry index */
	int max_nodes;          /* Number of nodes allocated in fileentry_t** */
};

int init_listing(struct direntry** direntry, char* path);
int free_listing(struct direntry** direntry);
int try_select(struct direntry* direntry, int idx);

#endif
