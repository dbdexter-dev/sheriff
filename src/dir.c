#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "dir.h"
#include "utils.h"

static void dir_set_error(Fileentry *dir, char *msg);
static int  populate_listing(Direntry *dir, const char *path);
static void quicksort(Fileentry **dir, int istart, int iend);
static int  quicksort_pass(Fileentry* *dir, int istart, int iend);
static int  sort_tree(Direntry *dir);
static void tree_xchg(Fileentry **tree, int a, int b);

static int  m_include_hidden = 1;   /* Should we show hidden files globally? */

/* Mark all files in a direntry tree as not selected */
void
clear_dir_selection(Direntry *direntry)
{
	int i;

	for (i=0; i<direntry->count; i++) {
		direntry->tree[i]->selected = 0;
	}
}

/* Toggle hidden files visibility, or rather, whether we should keep them in the
 * tree array in every subsequent populate_listing() */
void
dir_toggle_hidden()
{
	m_include_hidden ^= 1;
}

/* Free memory associated with a Direntry, marking it as not allocated once it
 * has been fully deallocated */
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
	if (!direntry) {            /* Direntry must exist for this to make sense */
		return -1;
	}

	if (!(*direntry)) {         /* Fully initialize the Direntry struct */
		*direntry = safealloc(sizeof(**direntry));
		(*direntry)->max_nodes = 0;
		(*direntry)->tree = NULL;
		(*direntry)->path = NULL;
	}
	if ((*direntry)->path) {    /* Free the path if it isn't empty already */
		free((*direntry)->path);
	}

	(*direntry)->count = 0;
	(*direntry)->path = NULL;

	if (path) {                 /* If path isn't null, make a listing of it */
		(*direntry)->path = realpath(path, NULL);
		populate_listing(*direntry, path);
		sort_tree(*direntry);
		clear_dir_selection(*direntry);
	} 
	(*direntry)->sel_idx = 0;
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
		populate_listing(direntry, direntry->path);
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

	if (!src || !dest || !src->tree) {
		return -1;
	}

	src->tree[src->sel_idx]->selected = 1;  /* Mark current elem as selected */

	/* Count the elemnts that are selected */
	for (i=0, select_count=0; i<src->count; i++) {
		if (src->tree[i]->selected) {
			select_count++;
		}
	}

	if (!select_count) {    /* Return if there are no elements selected */
		return 0;
	}

	*dest = safealloc(sizeof(**dest));
	d = *dest;

	/* Copy tree metadata */
	d->count = select_count;
	d->sel_idx = 0;
	d->max_nodes = select_count;
	d->path = safealloc(sizeof(*(d->path)) * (strlen(src->path) + 1));
	strcpy(d->path, src->path);

	/* Copy tree members */
	d->tree = safealloc(sizeof(*(d->tree)) * select_count);
	for (i=0, j=0; j<select_count; i++) {
		if (src->tree[i]->selected) {
			d->tree[j] = safealloc(sizeof(*(d->tree[0])));
			memcpy(d->tree[j], src->tree[i], sizeof(*(src->tree[0])));
			j++;
		}
	}

	return 0;
}

/* Try to select the idxth element in the list, and return the index of the
 * element that was actually selected */
int
try_select(Direntry *direntry, int idx, int mark)
{
	int i;

	/* Clamp idx between 0 and direntry->count - 1 */
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

	/* Update the sel pointer to the currently selected entry and return */
	direntry->sel_idx = idx;
	return idx;
}

/* Static functions {{{*/
/* When any function fails to read a file attributes, it calls this function,
 * which populates the Fileentry struct with a special value, signaling that
 * it's not a valid file, and sets its name to communicate what kind of error
 * happened */
void
dir_set_error(Fileentry *file, char *msg)
{
	file->gid = 0;
	file->uid = 0;
	file->lastchange = 0;
	file->mode = 0;
	file->selected = 0;
	file->size = -1;

	if (msg) {
		strcpy(file->name, msg);
	} else {
		switch(errno) {
		case EACCES:
			strcpy(file->name, "(permission denied)");
			break;
		case EIO:
			strcpy(file->name, "(unreadable)");
			break;
		case EMFILE:        /* Intentional fallthrough */
		case ENFILE:
			strcpy(file->name, "(file descriptor limit reached)");
			break;
		case ENOMEM:
			strcpy(file->name, "(out of memory)");
			break;
		default:
			strcpy(file->name, "(on fire)");
			break;
		}
	}
}

/* Populate a Fileentry list with a directory listing */
int
populate_listing(Direntry *dir, const char *path)
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
			if (m_include_hidden || ep->d_name[0] != '.') {
				if (!is_dot_or_dotdot(ep->d_name)) {
					entries++;
				}
			}
		}
		closedir(dp);
		dir->count = entries;
	} else {
		if (!dir->tree) {
			dir->tree = safealloc(sizeof(*(dir->tree)));
			dir->tree[0] = safealloc(sizeof(*(dir->tree[0])));
			dir->max_nodes = 1;
		}
		dir_set_error(dir->tree[0], NULL);
		dir->count = 1;
		return 1;
	}

	if (entries == 0) {
		if (dir->max_nodes == 0) {
			if (!dir->tree) {
				dir->tree = safealloc(sizeof(*(dir->tree)));
			}
			dir->tree[0] = safealloc(sizeof(*(dir->tree[0])));
			dir->max_nodes = 1;
		}
		dir_set_error(dir->tree[0], "(empty)");
		dir->count = 1;
		return 0;
	}

	/* Reallocate a chunk of memory to contain all the elements if we don't have
	 * enough. Use realloc or malloc depending on whether the dir->tree array is
	 * already initalized or not */
	if (dir->max_nodes < entries) {
		if (dir->max_nodes == 0) {
			dir->tree = safealloc(sizeof(*dir->tree) * entries);
		} else {
			dir->tree = realloc(dir->tree, sizeof(*dir->tree) * entries);
		}

		for (i = dir->max_nodes; i < entries; i++) {
			dir->tree[i] = safealloc(sizeof(*dir->tree[0]));
		}

		dir->max_nodes = entries;
	}

	/* Populate the Direntry struct with all the items inside the directory
	 * listing. If opendir fails, set error and exit */
	if ((dp = opendir(path))) {
		i = 0;
		while (i < entries && (ep = readdir(dp))) {
			if (m_include_hidden || ep->d_name[0] != '.') {
				if (!is_dot_or_dotdot(ep->d_name)) {    /* Exclude . and .. */
					/* Populate file stats, if this fails create an error entry
					 * instead */
					tmp = join_path(path, ep->d_name);
					if (lstat(tmp, &st) < 0) {
						dir_set_error(dir->tree[i], NULL);
					} else {
						strcpy(dir->tree[i]->name, ep->d_name);
						dir->tree[i]->size = st.st_size;
						dir->tree[i]->uid = st.st_uid;
						dir->tree[i]->gid = st.st_gid;
						dir->tree[i]->mode = st.st_mode;
						dir->tree[i]->lastchange = st.st_mtim.tv_sec;
					}
					free(tmp);
					i++;
				}
			}
		}
		closedir(dp);
	} else {
		dir_set_error(dir->tree[0], NULL);
		dir->count = 1;
		return 1;
	}

	return 0;
}

/* Iterative implementation of quicksort {{{*/
void
quicksort(Fileentry* *tree, int istart, int iend)
{
	int *stack;
	int sp;
	int p;

	if (istart >= iend) {
		return;
	}

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
/*}}}*/
/* Quicksort algorithm pass, center element is the pivot {{{*/
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
		if (strcasecmp(tree[i]->name, pivot) < 0) {
			tree_xchg(tree, i, r++);
		}
	}

	tree_xchg(tree, r, iend);
	return r;
}
/*}}}*/

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

/* Just because I don't like seeing this too much knowing that xchg exists */
inline void
tree_xchg(Fileentry* *tree, int a, int b)
{
	Fileentry* tmp;
	tmp = tree[a];
	tree[a] = tree[b];
	tree[b] = tmp;
}
/*}}}*/
