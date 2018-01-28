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
	struct direntry* tmpdir;

	/* Rotate right by one the allocated directories */
	tmpdir = right->dir;
	right->dir = center->dir;
	center->dir = left->dir;
	left->dir = tmpdir;

	right->offset = center->offset;
	center->offset = left->offset;
	left->offset = 0;

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
	struct direntry* tmpdir;

	centersel = center->dir->tree[center->dir->sel_idx];
	if(!S_ISDIR(centersel->mode))
		return 1;

	/* Rotate left by one the allocated directories */
	tmpdir = left->dir;
	left->dir = center->dir;
	center->dir = right->dir;
	right->dir = tmpdir;

	left->offset = center->offset;
	center->offset = right->offset;
	right->offset = 0;

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
	/* If leaf is a directory, populate with the contents of said directory;
	 * otherwise, just put up a blank view. Also has the nice side effect that
	 * you can initialize a window to blank by passing NULL to either parent or
	 * leaf */
	if(leaf && S_ISDIR(leaf->mode) && parent)
	{
		tmp = safealloc(sizeof(*tmp) * (strlen(parent) + strlen(leaf->name) + 1 + 1));
		sprintf(tmp, "%s/%s", parent, leaf->name);
		init_listing(&(win->dir), tmp);
		free(tmp);

		win->offset = 0;
	}
	else
		init_listing(&(win->dir), NULL);

	return 0;
}
