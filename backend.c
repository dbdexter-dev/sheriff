#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "backend.h"
#include "dir.h"
#include "utils.h"

int
navigate_back(Dirview* left, Dirview* center, Dirview* right)
{
	char *tmppath;

	assert(!free_listing(&(right->dir)));
	right->dir = center->dir;
	center->dir = left->dir;
	left->dir = NULL;

	assert(center->dir->path);

	tmppath = safealloc(sizeof(char) * (strlen(center->dir->path) + 4 + 1));
	sprintf(tmppath, "%s/../", center->dir->path);
	init_listing(&(left->dir), tmppath);

	free(tmppath);
	tmppath = NULL;

	return 0;
}

/* Navigate forward in the directory listing */
int
navigate_fwd(Dirview* left, Dirview* center, Dirview* right)
{
	char *tmppath;

	if(center->dir->sel->type != DT_DIR)
		return 1;

	assert(!free_listing(&left->dir));
	left->dir = center->dir;
	center->dir = right->dir;
	center->dir->sel = center->dir->tree;
	center->dir->sel_idx = 0;
	center->offset = 0;
	right->dir = NULL;

	assert(center->dir->path);

	return 0;
}

int
associate_dir(Dirview* view, struct direntry* direntry)
{
	assert(view);
	assert(direntry);

	view->dir = direntry;
	return 0;
}
