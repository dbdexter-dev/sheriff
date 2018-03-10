#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "dir.h"
#include "utils.h"

#include <stdio.h>

static int  populate_tree(Direntry *dir, const char *path);
static int  quicksort_pass(Fileentry* *dir, int istart, int iend);
static void quicksort(Fileentry* *dir, int istart, int iend);
static void tree_xchg(Fileentry* *tree, int a, int b);
static int  sort_tree(Direntry *dir);

/* Mark all files in a direntry tree as not selected */
int
clear_dir_selection(Direntry *direntry)
{
	int i;

	for (i=0; i<direntry->count; i++) {
		direntry->tree[i]->selected = 0;
	}

	return 0;
}

/* Free memory associated with a tree */
int
free_listing(Direntry **direntry)
{
	int i;

	if (!direntry) {
		return 0;
	}

	/* If the direntry has a tree, free all the elements in it, then free the
	 * pointer itself */
	if ((*direntry)->tree) {
		for (i=0; i<(*direntry)->max_nodes; i++) {
			free((*direntry)->tree[i]);
		}
		free((*direntry)->tree);
		(*direntry)->tree = NULL;
	}

	/* If there's a path associated to the direntry, free it */
	if ((*direntry)->path) {
		free((*direntry)->path);
		(*direntry)->path = NULL;
	}

	free(*direntry);
	*direntry = NULL;

	return 0;
}

/* Populate a direntry structure with a directory listing. If path is NULL,
 * don't populate it, set direntry->path to NULL and return */
int
init_listing(Direntry **direntry, const char *path)
{
	assert(direntry);

	if (!(*direntry)) {
		/* Fully initialize the Direntry struct */
		*direntry = safealloc(sizeof(**direntry));
		(*direntry)->max_nodes = 0;
		(*direntry)->path = NULL;
	} else {
		/* Semi-initialize a dirty Direntry struct */
		(*direntry)->count = 0;
	}

	if ((*direntry)->path) {
		free((*direntry)->path);
	}

	if (path) {
		(*direntry)->path = realpath(path, NULL);
		populate_tree(*direntry, path);
		sort_tree(*direntry);
	} else {
		(*direntry)->path = NULL;
	}

	(*direntry)->sel_idx = 0;
	clear_dir_selection(*direntry);
	return 0;
}

/* Update a directory listing without changing the directory it points to, or
 * the highlighted file index. It does clear the current selection though, since
 * rescan_listing is called when the directory contents have changed, and so the
 * selected files might get pushed around */
int
rescan_listing(Direntry *direntry)
{
	if (direntry->path) {
		populate_tree(direntry, direntry->path);
		sort_tree(direntry);
		clear_dir_selection(direntry);
	}
	return 0;
}

/* Clone all the elements that have been selected in a tree */
int
snapshot_tree_selected(Direntry **dest, Direntry *src)
{
	int i, j, select_count;
	Direntry *d;

	assert(src);
	assert(dest);

	if (!src->tree) {
		return -1;
	}

	src->tree[src->sel_idx]->selected = 1;

	select_count = 0;
	for (i=0; i<src->count; i++) {
		if (src->tree[i]->selected) {
			select_count++;
		}
	}

	*dest = safealloc(sizeof(**dest));
	d = *dest;

	if (src->tree || !select_count) {
		d->count = select_count;
		d->sel_idx = 0;
		d->max_nodes = select_count;

		d->path = safealloc(sizeof(*(d->path)) * (strlen(src->path) + 1));
		strcpy(d->path, src->path);

		d->tree = safealloc(sizeof(*(d->tree)) * select_count);
		for (i=0, j=0; i<src->count; i++) {
			if (src->tree[i]->selected) {
				d->tree[j] = safealloc(sizeof(*(d->tree[0])));
				memcpy(d->tree[j], src->tree[i], sizeof(*(src->tree[0])));
				j++;
			}
		}
	}

	return 0;
}

/* Try to select the idxth element in the list, and return the index of the line
 * that was actually selected */
int
try_select(Direntry *direntry, int idx, int mark)
{
	int i;

	if (idx >= direntry->count) {
		idx = direntry->count - 1;
	} else if (idx < 0) {
		idx = 0;
	}

	if (mark) {
		/* Mark the files from the previous idx to the current idx as selected */
		if (idx > direntry->sel_idx) {
			for (i=direntry->sel_idx+1; i<=idx; i++) {
				direntry->tree[i]->selected ^= 1;
			}
		} else if (idx < direntry->sel_idx) {
			for (i=idx; i<direntry->sel_idx; i++) {
				direntry->tree[i]->selected ^= 1;
			}
		}
	}


	/* Update the sel pointer to the currently selected entry */
	direntry->sel_idx = idx;

	return direntry->sel_idx;
}

/* Static functions {{{*/
/* Populate a Fileentry list with a directory listing */
int
populate_tree(Direntry *dir, const char *path)
{
	DIR *dp;
	struct dirent *ep;
	struct stat st;
	int i, entries;
	char *tmp;

	/* Calculate how many elements we need to allocate */
	entries = 0;
	if ((dp = opendir(path))) {
		while ((ep = readdir(dp))) {
			entries++;
		}
		closedir(dp);
		dir->count = entries;
	/* Opendir failed, create a single element with error code */
	} else {
		if (!(dir->tree)) {
			dir->tree = safealloc(sizeof(*(dir->tree)));
		}

		dir->tree[0]->size = -1;
		dir->tree[0]->mode = 0;
		switch (errno) {
		case EACCES:
			strcpy(dir->tree[0]->name, "(permission denied)");
			break;
		case EIO:
			strcpy(dir->tree[0]->name, "(unreadable)");
			break;
		case EMFILE:        /* Intentional fallthrough */
		case ENFILE:
			strcpy(dir->tree[0]->name, "(file descriptor limit reached)");
			break;
		case ENOMEM:
			strcpy(dir->tree[0]->name, "(out of memory)");
			break;
		default:
			strcpy(dir->tree[0]->name, "(on fire)");
			break;
		}

		dir->count = 1;
		return 1;
	}
	/* Reallocte a chunk of memory to contain all the elements if we don't have
	 * enough. Use realloc or malloc depending on whether the dir->tree array is
	 * already initalized or not */
	if (dir->max_nodes < entries) {
		if (dir->max_nodes == 0) {
			dir->tree = safealloc(sizeof(*dir->tree) * entries);
		} else {
			dir->tree = realloc(dir->tree, sizeof(*dir->tree) * entries);
		}

		for (i=dir->max_nodes; i<entries; i++) {
			dir->tree[i] = safealloc(sizeof(*dir->tree[0]));
		}

		dir->max_nodes = entries;
	}

	/* Populate the directory listing */
	if ((dp = opendir(path))) {
		for (i = 0; i < entries; i++) {
			ep = readdir(dp);
			/* Populate the first struct fields */
			strcpy(dir->tree[i]->name, ep->d_name);

			/* Allocate space for the concatenation <path>/<filename> */
			tmp = safealloc(sizeof(*tmp) * (strlen(path) +
			                                strlen(dir->tree[i]->name) +
			                                1 + 1));
			sprintf(tmp, "%s/%s", path, dir->tree[i]->name);

			/* Get file stats */
			lstat(tmp, &st);

			/* Populate the remaining struct fields */
			dir->tree[i]->size = st.st_size;
			dir->tree[i]->uid = st.st_uid;
			dir->tree[i]->gid = st.st_gid;
			dir->tree[i]->mode = st.st_mode;
			dir->tree[i]->lastchange = st.st_mtim.tv_sec;

			free(tmp);
		}
		closedir(dp);
	} else {
		dir->tree[0]->size = -1;
		dir->tree[0]->mode = 0;
		switch (errno) {
		case EACCES:
			strcpy(dir->tree[0]->name, "(permission denied)");
			break;
		case EIO:
			strcpy(dir->tree[0]->name, "(unreadable)");
			break;
		case EMFILE:        /* Intentional fallthrough */
		case ENFILE:
			strcpy(dir->tree[0]->name, "(file descriptor limit reached)");
			break;
		case ENOMEM:
			strcpy(dir->tree[0]->name, "(out of memory)");
			break;
		default:
			strcpy(dir->tree[0]->name, "(on fire)");
			break;
		}

		dir->count = 1;
		return 1;
	}

	if (entries == 0) {
		dir->tree[0]->size = -1;
		dir->tree[0]->mode = 0;
		strcpy(dir->tree[0]->name, "(empty)");
		dir->count = 1;
	}

	return 0;
}

/* Quicksort algorithm pass, center element is the pivot */
int
quicksort_pass(Fileentry* *tree, int istart, int iend)
{
	int i, r, pi;
	char *pivot;

	pi = (istart+iend)/2;
	pivot = tree[pi]->name;
	tree_xchg(tree, pi, iend);

	r = istart;

	for (i=istart; i < iend; i++) {
		if (strcasecmp(tree[i]->name, pivot) < 0)
			tree_xchg(tree, i, r++);
	}

	tree_xchg(tree, r, iend);
	return r;
}

/* Iterative implementation of quicksort */
void
quicksort(Fileentry* *tree, int istart, int iend)
{
	int *stack;
	int sp;
	int p;

	if (istart >= iend)
		return;

	stack = safealloc(sizeof(*stack) * (iend - istart + 1));
	sp = 0;

	stack[sp++] = istart;
	stack[sp++] = iend;
	while (sp > 0) {
		iend = stack[--sp];
		istart = stack[--sp];
		p = quicksort_pass(tree, istart, iend);

		if (p-1 > istart) {
			stack[sp++] = istart;
			stack[sp++] = p-1;
		}

		if (p+1 < iend) {
			stack[sp++] = p+1;
			stack[sp++] = iend;
		}
	}
	free(stack);
}


/* Alphabetically sort a directory listing, directories first
 * This looks ugly because I need to keep track of the element referencing
 * the one I'm looking at in order to exchange them efficiently */
int
sort_tree(Direntry *dir)
{
	int i, j, files_present;
	Fileentry* *tree = dir->tree;

	if (!tree) {
		return -1;
	}

	/* Split directories and files first.
	 * j points to the beginning of the files, i to the end of the directories.
	 * What lies in between has yet to be sorted */
	j = dir->count;
	files_present = 0;
	for (i=0; i<j; i++) {
		if (!S_ISDIR(tree[i]->mode)) {
			if (j == dir->count) {
				j--;
				files_present = 1;
			}
			for (; j>i && !S_ISDIR(tree[j]->mode); j--)
				;
			tree_xchg(tree, i, j);
		}
	}
	/* Sort directories */
	quicksort(dir->tree, 0, j - 1);

	/* Sort files */
	if (files_present) {
		quicksort(dir->tree, j, dir->count-1);
	}
	return 0;
}

inline void
tree_xchg(Fileentry* *tree, int a, int b)
{
	Fileentry* tmp;
	tmp = tree[a];
	tree[a] = tree[b];
	tree[b] = tmp;
}
/*}}}*/
