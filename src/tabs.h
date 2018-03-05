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
