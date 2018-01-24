#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "dir.h"
#include "utils.h"

#include <stdio.h>

static unsigned long get_file_size(char* path, char* fname);
static void list_xchg(fileentry_t* a, fileentry_t* b);

/* Populate a fileentry_t list with a directory listing */
fileentry_t*
dirlist(char* path)
{
	DIR* dp;
	struct dirent *ep;
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

			tree->type = ep->d_type;
			strncpy(tree->name, ep->d_name, MAXLEN);
			tree->size = get_file_size(path, tree->name);
		}
		tree->next = NULL;
		closedir(dp);
	}

	return head;
}
/* Free a fileentry_t linked list */
int 
free_tree(fileentry_t* list)
{
	fileentry_t* next;

	while(list != NULL)
	{
		next = list->next;
		free(list);
		list = next;
	}

	return 0;
}

/* Alphabetically sort a directory listing, directories first 
 * This looks ugly because I need to keep track of the element referencing
 * the one I'm looking at in order to exchange them efficiently */
int
sort_tree(fileentry_t* tree)
{
	int done;
	fileentry_t* tmpptr, *dirptr;
	fileentry_t* prevtmp, *prevdir;

	if(!tree)
		die("Cannot sort an empty tree!");

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
				dirptr = prevdir;
			}
		}
	}

	/* TODO Sort directotries */
	/* TODO Sort files */
	return 0;
}


/* Given a path and a filename, return its size */
unsigned long
get_file_size(char* path, char* fname)
{
	struct stat st;
	char* tmp;

	tmp = safealloc(sizeof(char) * (strlen(path) + strlen(fname) + 1));
	strcpy(tmp, path);
	strcat(tmp, fname);

	stat(tmp, &st);
	free(tmp);
	return st.st_size;
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
