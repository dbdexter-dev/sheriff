#define _GNU_SOURCE

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

static fileentry_t** dirlist(char* path, int* size);
static int           quicksort_pass(fileentry_t** dir, int istart, int iend);
static void          quicksort(fileentry_t** dir, int istart, int iend);
static void          tree_xchg(fileentry_t** tree, int a, int b);
static int           sort_tree(struct direntry* dir);

/* Free memory associated with a tree */
int
free_listing(struct direntry** direntry)
{
	int i;

	if(!(*direntry))
		return 0;

	if((*direntry)->tree)
	{
		for(i=0; i<(*direntry)->count; i++)
			free((*direntry)->tree[i]);
		free((*direntry)->tree);
		(*direntry)->tree = NULL;
	}

	if((*direntry)->path)
	{
		free((*direntry)->path);
		(*direntry)->path = NULL;
	}

	free((*direntry));
	*direntry = NULL;


	return 0;
}

/* Populate a direntry structure with a directory listing */
int
init_listing(struct direntry** direntry, char* path)
{
	assert(direntry);
	assert(path);

	*direntry = safealloc(sizeof(**direntry));
	/* Save the current workdir */
	(*direntry)->path = realpath(path, NULL);

	/* Populate and sort */
	(*direntry)->tree = dirlist(path, &(*direntry)->count);
	sort_tree(*direntry);

	/* Find the tree size */
	(*direntry)->sel_idx = 0;

	return 0;
}

/* Try to select the idxth element in the list */
int
try_select(struct direntry* direntry, int idx)
{
	if(idx >= direntry->count)
		idx = direntry->count - 1;
	else if (idx < 0)
		idx = 0;

	/* Update the sel pointer to the currently selected entry */
	direntry->sel_idx = idx;

	return direntry->sel_idx;
}

/* Static functions *//*{{{*/
/* Populate a fileentry_t list with a directory listing */
fileentry_t**
dirlist(char* path, int* entry_nr)
{
	DIR* dp;
	struct dirent *ep;
	struct stat st;
	int i;
	char* tmp;
	fileentry_t** tree;

	assert(path);

	/* Calculate how many elements we need to allocate */
	*entry_nr = 0;
	if((dp = opendir(path)))
	{
		while((ep = readdir(dp)))
			(*entry_nr)++;
		closedir(dp);
	}
	/* Opendir failed, create a single element */
	else
	{
		tree = safealloc(sizeof(*tree));
		*tree = safealloc(sizeof(**tree));
		memset(*tree, '\0', sizeof(**tree));

		(*tree)->size = -1;
		switch(errno)
		{
			case EACCES:
				strcpy((*tree)->name, "(inaccessible)");
				break;
			case EIO:
				strcpy((*tree)->name, "(unreadable)");
				break;
			default:
				strcpy((*tree)->name, "(on fire)");
				break;
		}
		*entry_nr = 1;
		return tree;
	}

	/* Allocate a chunk of memory to contain all the elements */
	tree = safealloc(sizeof(*tree) * *entry_nr);
	for(i=0; i<*entry_nr; i++)
		tree[i] = safealloc(sizeof(*tree[i]));

	/* Populate the directory listing */
	if((dp = opendir(path)))
	{
		for(i = 0; i < *entry_nr; i++)
		{
			ep = readdir(dp);
			/* Populate the first struct fields */
			strcpy(tree[i]->name, ep->d_name);

			/* Allocate space for the concatenation <path>/<filename> */
			tmp = safealloc(sizeof(*tmp) * (strlen(path) + strlen(tree[i]->name) + 1 + 1));
			sprintf(tmp, "%s/%s", path, tree[i]->name);

			/* Get file stats */
			stat(tmp, &st);

			/* Populate the remaining struct fields */
			tree[i]->size = st.st_size;
			tree[i]->uid = st.st_uid;
			tree[i]->gid = st.st_gid;
			tree[i]->mode = st.st_mode;
			tree[i]->lastchange = st.st_mtim.tv_sec;

			free(tmp);
		}
		closedir(dp);
	}
	else
	{
		memset(*tree, '\0', sizeof(**tree));

		(*tree)->size = -1;
		switch(errno)
		{
			case EACCES:
				strcpy((*tree)->name, "(forbidden)");
				break;
			case EIO:
				strcpy((*tree)->name, "(unreadable)");
				break;
			default:
				strcpy((*tree)->name, "(on fire)");
				break;
		}
		*entry_nr = 1;
		return tree;
	}

	return tree;
}

/* Implementation of the quicksort algorithm, center element is the pivot */
int
quicksort_pass(fileentry_t** tree, int istart, int iend)
{
	int i, r, pi;
	char* pivot;

	pi = (istart+iend)/2;
	pivot = tree[pi]->name;
	tree_xchg(tree, pi, iend);

	r = istart;   /* Right ordered partition index */
	for(i=istart; i < iend; i++)
	{
		if(strcasecmp(tree[i]->name, pivot) < 0)
			tree_xchg(tree, i, r++);
	}
	tree_xchg(tree, r, iend);
	return r;
}

void
quicksort(fileentry_t** tree, int istart, int iend)
{
	int* stack;
	int sp;
	int p;

	if(istart >= iend)
		return;

	stack = safealloc(sizeof(*stack) * (iend - istart + 1));
	sp = 0;

	stack[sp++] = istart;
	stack[sp++] = iend;
	while(sp > 0)
	{
		iend = stack[--sp];
		istart = stack[--sp];
		p = quicksort_pass(tree, istart, iend);
		if(p-1 > istart)
		{
			stack[sp++] = istart;
			stack[sp++] = p-1;
		}
		if(p+1 < iend)
		{
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
sort_tree(struct direntry* dir)
{
	int i, j;
	fileentry_t** tree = dir->tree;

	if(!tree)
		return -1;

	/* Split directories and files first.
	 * j points to the beginning of the files, i to the end of the directories.
	 * What lies in between has yet to be sorted */
	j = dir->count - 1;
	for(i=0; i<j; i++)
		if(!S_ISDIR(tree[i]->mode))
		{
			for(; j>i && !S_ISDIR(tree[j]->mode); j--);
			tree_xchg(tree, i, j);
		}
	/* Sort directories */
	quicksort(dir->tree, 0, j - 1);

	/* Sort files */
	quicksort(dir->tree, j, dir->count-1);
	return 0;
}

inline void
tree_xchg(fileentry_t** tree, int a, int b)
{
	fileentry_t* tmp;
	tmp = tree[a];
	tree[a] = tree[b];
	tree[b] = tmp;
}
/*}}}*/
