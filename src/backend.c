#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "backend.h"
#include "dir.h"
#include "utils.h"

/* Associate a Direntry struct with a Dirview */
int
associate_dir(Dirview *view, Direntry *direntry)
{
	assert(view);
	assert(direntry);

	view->dir = direntry;
	return 0;
}

/* Navigate back out of a directory, updating the Direntries */
int
navigate_back(Dirview *left, Dirview *center, Dirview *right)
{
	Direntry *tmpdir;
	char *fullpath;

	/* Rotate right by one the allocated directories */
	tmpdir = right->dir;
	right->dir = center->dir;
	center->dir = left->dir;
	left->dir = tmpdir;

	right->offset = center->offset;
	center->offset = left->offset;
	left->offset = 0;

	assert(center->dir->path);
	fullpath = join_path(center->dir->path, "../");

	/* Init left pane with parent directory contents */
	update_win_with_path(left, fullpath);
	free(fullpath);

	return 0;
}

/* Navigate forward in the directory listing */
int
navigate_fwd(Dirview *left, Dirview *center, Dirview *right)
{
	Fileentry* centersel;
	Direntry *tmpdir;
	char *fullpath;

	centersel = center->dir->tree[center->dir->sel_idx];
	if (!S_ISDIR(centersel->mode)) {
		return -1;
	}

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
	fullpath = join_path(center->dir->path, centersel->name);

	/* Init right pane with child directory contents */
	update_win_with_path(right, fullpath);
	free(fullpath);

	return 0;
}

/* Update a window with a given path, which can also be NULL. In that case, the
 * window passed as an argument is initialized empty. XXX this funcion used to
 * be a bit more complex, now it's just here in case that complexity arises in
 * the future */
int
update_win_with_path(Dirview *win, const char *path)
{
	init_listing(&(win->dir), path);
	return 0;
}
