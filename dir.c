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
		for(i=0; i<(*direntry)->tree_size; i++)
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

	*direntry = safealloc(sizeof(struct direntry));
	/* Save the current workdir */
	(*direntry)->path = realpath(path, NULL);

	/* Populate and sort */
	(*direntry)->tree = dirlist(path, &(*direntry)->tree_size);
	sort_tree(*direntry);

	/* Find the tree size */
	(*direntry)->sel_idx = 0;

	return 0;
}

/* Try to select the idxth element in the list */
int
try_select(struct direntry* direntry, int idx)
{
	if(idx >= direntry->tree_size)
		idx = direntry->tree_size - 1;
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
		tree = safealloc(sizeof(fileentry_t*));
		*tree = safealloc(sizeof(fileentry_t));
		memset(*tree, '\0', sizeof(fileentry_t));

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
	tree = safealloc(sizeof(fileentry_t*) * *entry_nr);
	for(i=0; i<*entry_nr; i++)
		tree[i] = safealloc(sizeof(fileentry_t));

	/* Populate the directory listing */
	if((dp = opendir(path)))
	{
		for(i = 0; i < *entry_nr; i++)
		{
			ep = readdir(dp);
			/* Populate the first struct fields */
			strncpy(tree[i]->name, ep->d_name, MAXLEN);

			/* Allocate space for the concatenation <path>/<filename> */
			tmp = safealloc(sizeof(char) * (strlen(path) + strlen(tree[i]->name) + 1 + 1));
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
		memset(*tree, '\0', sizeof(fileentry_t));

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

/* XXX XXX XXX XXX XXX XXX XXX XXX XXX */
/* Will exchange a->next with b->next */
/* Edge cases are a bitch */
void
tree_xchg(fileentry_t** tree, int a, int b)
{
	fileentry_t* tmp;

	assert(tree[a]);
	assert(tree[b]);

	tmp = tree[a];
	tree[a] = tree[b];
	tree[b] = tmp;
}

/* Alphabetically sort a directory listing, directories first
 * This looks ugly because I need to keep track of the element referencing
 * the one I'm looking at in order to exchange them efficiently */
int
sort_tree(struct direntry* dir)
{
	int done;
	int i, tmp_i, j;
	char* tmp;
	fileentry_t** tree = dir->tree;

	if(!tree)
		return -1;

	/* Split directories and files first */
	done = 0;
	for(i=0; i<dir->tree_size && !done; i++)
	{
		if(!S_ISDIR(tree[i]->mode))
		{
			for(j=i; j<dir->tree_size && !S_ISDIR(tree[j]->mode); j++);
			if(j == dir->tree_size)
				done = 1;
			else
			{
				tree_xchg(tree, i, j);
				i--;
			}
		}
	}

	/* Sort directories */
	for(i=0; i<dir->tree_size && S_ISDIR(tree[i]->mode); i++)
	{
		tmp = tree[i]->name;
		tmp_i = i;
		for(j=i+1; j<dir->tree_size && S_ISDIR(tree[j]->mode); j++)
		{
			if(strcasecmp(tmp, tree[j]->name) > 0)
			{
				tmp = tree[j]->name;
				tmp_i = j;
			}
		}
		if(i != tmp_i)
			tree_xchg(tree, i, tmp_i);
	}

	/* Sort files */
	for(; i<dir->tree_size; i++)
	{
		tmp = tree[i]->name;
		tmp_i = i;
		for(j=i+1; j<dir->tree_size; j++)
		{
			if(strcasecmp(tmp, tree[j]->name) > 0)
			{
				tmp = tree[j]->name;
				tmp_i = j;
			}
		}
		if(i != tmp_i)
			tree_xchg(tree, i, tmp_i);
	}

	return 0;
}/*}}}*/
