#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "dir.h"
#include "utils.h"

#include <stdio.h>

static fileentry_t* dirlist(char* path);
static void         free_tree(fileentry_t* tree);
static void         list_xchg(fileentry_t* a, fileentry_t* b);
static int          sort_tree(fileentry_t* tree);

/* Free memory associated with a tree */
int
free_listing(struct direntry** direntry)
{
	if(!(*direntry))
		return 0;

	if(!(*direntry)->tree || !(*direntry)->path)
		return 1;

	free_tree((*direntry)->tree);
	free((*direntry)->path);
	free((*direntry));
	*direntry = NULL;

	return 0;
}

/* Populate a direntry structure with a directory listing */
int
init_listing(struct direntry** direntry, char* path)
{
	fileentry_t* tmp;

	assert(direntry);
	assert(path);

	*direntry = safealloc(sizeof(struct direntry));
	/* Save the current workdir */
	(*direntry)->path = realpath(path, NULL);

	/* Populate and sort */
	(*direntry)->tree = dirlist(path);
	sort_tree((*direntry)->tree);

	/* Find the tree size */
	(*direntry)->tree_size = 0;
	for(tmp = (*direntry)->tree; tmp != NULL; tmp=tmp->next, (*direntry)->tree_size++);
	try_select(*direntry, 0);

	return 0;
}

/* Try to select the idxth element in the list */
int
try_select(struct direntry* direntry, int idx)
{
	int i;

	if(idx > direntry->tree_size)
		direntry->sel_idx = direntry->tree_size - 1;
	else if (idx < 0)
		direntry->sel_idx = 0;
	else
		direntry->sel_idx = idx;

	/* Update the sel pointer to the currently selected entry */
	direntry->sel = direntry->tree;
	for(i=0; i<direntry->sel_idx; i++, direntry->sel = direntry->sel->next);

	return direntry->sel_idx;
}

/* Static functions *//*{{{*/
/* Populate a fileentry_t list with a directory listing */
fileentry_t*
dirlist(char* path)
{
	DIR* dp;
	struct dirent *ep;
	struct stat st;
	char* tmp;
	fileentry_t* head, *tree = NULL;

	assert(path);

	if((dp = opendir(path)))
	{
		while((ep = readdir(dp)))
		{
			if(!tree)
			{
				tree = safealloc(sizeof(fileentry_t));
				head = tree;
			}
			else
			{
				tree->next = safealloc(sizeof(fileentry_t));
				tree = tree->next;
			}

			/* Populate the first struct fields */
			tree->type = ep->d_type;
			strncpy(tree->name, ep->d_name, MAXLEN);

			/* Allocate space for the concatenation <path>.<filename> */
			tmp = safealloc(sizeof(char) * (strlen(path) + strlen(tree->name) + 1 + 1));
			sprintf(tmp, "%s/%s", path, tree->name);

			/* Get file stats */
			stat(tmp, &st);

			/* Populate the remaining struct fields */
			tree->size = st.st_size;
			tree->uid = st.st_uid;
			tree->gid = st.st_gid;
			tree->mode = st.st_mode;
			tree->lastchange = st.st_mtim.tv_sec;

			free(tmp);
		}
		tree->next = NULL;
		closedir(dp);
	}
	else
		return NULL;

	return head;
}
/* Free a fileentry_t linked list */
void
free_tree(fileentry_t* list)
{
	int status;
	fileentry_t* next;

	while(list != NULL)
	{
		next = list->next;
		free(list);
		list = next;
	}
}

/* XXX XXX XXX XXX XXX XXX XXX XXX XXX */
/* Will exchange a->next with b->next */
void
list_xchg(fileentry_t* a, fileentry_t* b)
{
	fileentry_t* tmp;

	assert(a->next);
	assert(b->next);

	tmp = a->next;
	a->next = b->next;
	b->next = tmp;

	tmp = a->next->next;
	a->next->next = b->next->next;
	b->next->next = tmp;
	return;
}

/* Alphabetically sort a directory listing, directories first
 * This looks ugly because I need to keep track of the element referencing
 * the one I'm looking at in order to exchange them efficiently */
int
sort_tree(fileentry_t* tree)
{
	int done;
	fileentry_t* tmpptr, *dirptr, *orderedptr, *list_begin;
	fileentry_t* prevtmp, *prevdir;

	if(!tree)
		return -1;

	/* Split directories and files first */
	done = 0;
	for(dirptr = prevdir = tree; dirptr->next != NULL && !done; prevdir = dirptr, dirptr=dirptr->next)
	{
		if(dirptr->type != DT_DIR)		/* This is not a directory */
		{
			/* Find the next available directory */
			prevtmp = dirptr;
			for(tmpptr = dirptr->next; tmpptr != NULL && tmpptr->type != DT_DIR; prevtmp = tmpptr, tmpptr = tmpptr->next);
			if(tmpptr == NULL)
				done = 1;
			else
			{
				list_xchg(prevtmp, prevdir);
				/* dirptr has changed, update it with the new value */
				dirptr = prevdir->next;
			}
		}
	}

	list_begin = tree;
	prevdir = list_begin;
	for(dirptr = list_begin->next; dirptr->next != NULL && dirptr->type == DT_DIR; prevdir = dirptr, dirptr = dirptr->next)
	{
		done = 0;
		for(prevtmp = NULL, orderedptr = list_begin; orderedptr != dirptr && !done; prevtmp = orderedptr, orderedptr = orderedptr->next)
			if(strcasecmp(dirptr->name, orderedptr->name) < 0)
			{
				assert(prevdir);
				assert(dirptr);
				assert(prevtmp);
				/* Link the list removing dirptr, which is to be inserted */
				prevdir->next = dirptr->next;

				/* Insert dirptr in its correct place */
				tmpptr = prevtmp->next;
				prevtmp->next = dirptr;
				dirptr->next = tmpptr;
				done = 1;
			}
	}

	list_begin = prevdir;
	for(dirptr = list_begin->next; dirptr != NULL; prevdir = dirptr, dirptr = dirptr->next)
	{
		done = 0;
		for(prevtmp = list_begin, orderedptr = list_begin->next; orderedptr != dirptr && !done; prevtmp = orderedptr, orderedptr = orderedptr->next)
			if(strcasecmp(dirptr->name, orderedptr->name) < 0)
			{
				assert(prevdir);
				assert(dirptr);
				assert(prevtmp);
				prevdir->next = dirptr->next;

				tmpptr = prevtmp->next;
				prevtmp->next = dirptr;
				dirptr->next = tmpptr;
				done = 1;
			}
	}
	return 0;
}/*}}}*/
