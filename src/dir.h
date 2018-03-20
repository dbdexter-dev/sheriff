/**
 * The main directory manipulation functions are here. Most of the low-level
 * ones are declared static inside dir.c, while the ones listed here are
 * expected to be used inside sheriff.c and backend.c when needed.
 * The two structs declared here represent (in order of appearance) a file in a
 * listing, and a whole listing.
 * The first one stores its name, size, owners, mode, time of the last change,
 * and whether it is currently selected.
 * The second one stores the path it refers to, the listing itself, the number of
 * valid items in it, the index of the highlighted element, and how many nodes
 * it can hold without needing a calloc() call.
 * NOTE: you can assume that path won't contain any trailing slashes. It's
 * coming from a call to realpath(), so no worries there
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
	char selected;
} Fileentry;

typedef struct {
	char *path;             /* Path this struct represents */
	Fileentry **tree;       /* Array of file metadata */
	int count;              /* Number of entries in the list */
	int sel_idx;            /* Selected entry index */
	int max_nodes;          /* Number of nodes allocated in Fileentry** */
} Direntry;

void clear_dir_selection(Direntry *direntry);
void dir_toggle_hidden();
int  fuzzy_file_idx(const Direntry *dir, const char *fname, int start_idx);
int  free_listing(Direntry **direntry);
int  init_listing(Direntry **direntry, const char *path);
int  rescan_listing(Direntry *direntry);
int  snapshot_tree_selected(Direntry **dest, Direntry *src);
int  try_select(Direntry *direntry, int idx, int mark);

#endif
