#ifndef TABCTX_H
#define TABCTX_H

#include "backend.h"

typedef struct tbctx {
	PaneCtx *left, *center, *right;
	struct tbctx *next;
} TabCtx;

int  tabctx_append(TabCtx **ctx, const char *path);
int  tabctx_deinit(TabCtx **ctx);
int  tabctx_remove(TabCtx **ctx, int idx);

#endif
