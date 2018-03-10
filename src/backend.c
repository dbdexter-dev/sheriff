#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "backend.h"
#include "dir.h"
#include "utils.h"
#include "ncutils.h"

/* Associate a Direntry struct with a Dirview */
int
associate_dir(PaneCtx *ctx, Direntry *direntry)
{
	assert(ctx);
	assert(direntry);

	ctx->dir = direntry;
	return 0;
}

/* Initialize a window with a given path, which can also be NULL. In that case,
 * the window passed as an argument is initialized empty. XXX this funcion used
 * to be a bit more complex, now it's just here in case that complexity arises
 * in the future */
int
init_pane_with_path(PaneCtx *ctx, const char *path)
{
	init_listing(&ctx->dir, path);
	ctx->visual = 0;
	ctx->offset = 0;
	return 0;
}

/* Navigate back out of a directory, updating the Direntries */
int
navigate_back(PaneCtx *left, PaneCtx *center, PaneCtx *right)
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
	init_pane_with_path(left, fullpath);
	free(fullpath);

	return 0;
}

/* Navigate forward in the directory listing */
int
navigate_fwd(PaneCtx *left, PaneCtx *center, PaneCtx *right)
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
	if (S_ISDIR(centersel->mode)) {
		init_pane_with_path(right, fullpath);
	} else {
		init_pane_with_path(right, NULL);
	}
	free(fullpath);

	return 0;
}

/* Check whether the window offset should be changed, and return 1 if the offset
 * has changed, 0 otherwise */
int
recheck_offset(PaneCtx *ctx, int nr)
{
	if (ctx->dir->sel_idx - ctx->offset >= nr) {
		ctx->offset = ctx->dir->sel_idx - nr + 1;
		return 1;
	} else if (ctx->dir->sel_idx - ctx->offset < 0) {
		ctx->offset = ctx->dir->sel_idx;
		return 1;
	}

	return 0;
}

/* Wrapper for rescan_listing, to keep the abstraction layer consistent */
int
rescan_pane(PaneCtx *ctx)
{
	rescan_listing(ctx->dir);
	return 0;
}

/* Free a single pane */
int
free_pane(PaneCtx *ctx)
{
	if (!ctx) {
		return 0;
	}

	free_listing(&ctx->dir);
	free(ctx);
	return 0;
}
