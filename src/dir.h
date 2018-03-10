/**
 * The main directory manipulation functions are here. Most of the low-level
 * ones are declared static inside dir.c, while the ones listed here are
 * expected to be used inside sheriff.c and backend.c when needed.
 */
#ifndef DIR_H
#define DIR_H

#include <dirent.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct {
	char name[NAME_MAX+1];  /* NAME_MAX defined in dirent.h */
	long size;
	uid_t uid, gid;
	mode_t mode;
	time_t lastchange;
	int selected;
} Fileentry;

typedef struct {
	char *path;             /* Path this struct represents */
	Fileentry **tree;       /* Array of file metadata */
	int count;              /* Number of entries in the list */
	int sel_idx;            /* Selected entry index */
	int max_nodes;          /* Number of nodes allocated in Fileentry** */
} Direntry;

int  clear_dir_selection(Direntry *direntry);
void dir_toggle_hidden();
int  init_listing(Direntry **direntry, const char *path);
int  free_listing(Direntry **direntry);
int  rescan_listing(Direntry *direntry);
int  snapshot_tree_selected(Direntry **dest, Direntry *src);
int  try_select(Direntry *direntry, int idx, int mark);

#endif
