#define _GNU_SOURCE

#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "backend.h"
#include "dir.h"
#include "utils.h"

int
associate_dir(Dirview* view, struct direntry* direntry)
{
	assert(view);
	assert(direntry);

	view->dir = direntry;
	return 0;
}

int
navigate_back(Dirview* left, Dirview* center, Dirview* right)
{
	fileentry_t tmp = {.name = "../", .mode = S_IFDIR};

	if(free_listing(&(right->dir)))
		return 1;

	right->dir = center->dir;
	right->offset = center->offset;
	center->dir = left->dir;
	center->offset = left->offset;
	left->dir = NULL;

	assert(center->dir->path);

	/* Init left pane with parent directory contents */
	update_win_with_path(left, center->dir->path, &tmp);

	return 0;
}

/* Navigate forward in the directory listing */
int
navigate_fwd(Dirview* left, Dirview* center, Dirview* right)
{
	fileentry_t* centersel;

	centersel = center->dir->tree[center->dir->sel_idx];
	if(!S_ISDIR(centersel->mode))
		return 1;

	assert(!free_listing(&left->dir));
	left->dir = center->dir;
	left->offset = center->offset;
	center->dir = right->dir;
	center->offset = right->offset;
	right->dir = NULL;

	assert(center->dir->path);
	centersel = center->dir->tree[center->dir->sel_idx];

	/* Init right pane with child directory contents */
	update_win_with_path(right, center->dir->path, centersel);

	return 0;
}

int
update_win_with_path(Dirview* win, char* parent, fileentry_t* leaf)
{
	char* tmp;

	if(free_listing(&win->dir))
		return 1;

	if(S_ISDIR(leaf->mode))
	{
		tmp = safealloc(sizeof(*tmp) * (strlen(parent) + strlen(leaf->name) + 1 + 1));
		sprintf(tmp, "%s/%s", parent, leaf->name);
		init_listing(&(win->dir), tmp);
		win->offset = 0;
		free(tmp);
	}
}
