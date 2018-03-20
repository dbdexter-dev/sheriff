/**
 * The TabCtx struct is an element of a linked list of tabs. It holds info about
 * the three main backend structures that define a tab. All the functions in
 * here handle interfacing with this linked list: getting items from it, adding
 * items to it, and that's about it.
 */

#ifndef TABCTX_H
#define TABCTX_H

#include "backend.h"

typedef struct tbctx {
	PaneCtx *left, *center, *right;
	struct tbctx *next;
} TabCtx;

int     tabctx_append(const char *path);
TabCtx* tabctx_by_idx(int *idx);
int     tabctx_deinit();
TabCtx* tabctx_get();
int     tabctx_remove(int idx);

#endif
