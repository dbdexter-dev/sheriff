#include <assert.h>
#include <string.h>
#include "backend.h"
#include "tabs.h"
#include "utils.h"

static int tabctx_free(TabCtx *ctx);

/* Add a tab to a TabCtx linked list, populating it from a path */
int
tabctx_append(TabCtx **ctx, const char *path)
{
	char *tmp;
	TabCtx *ptr;

	assert(ctx);

	/* Allocate another element and insert it at the beginning of the list */
	ptr = safealloc(sizeof(**ctx));
	ptr->next = *ctx;
	*ctx = ptr;

	ptr->left = safealloc(sizeof(*ptr->left));
	ptr->center = safealloc(sizeof(*ptr->center));
	ptr->right = safealloc(sizeof(*ptr->right));

	/* Needed to mark the panes as uninitialized */
	memset(ptr->left, '\0', sizeof(*ptr->left));
	memset(ptr->center, '\0', sizeof(*ptr->center));
	memset(ptr->right, '\0', sizeof(*ptr->right));

	tmp = join_path(path, "../");
	init_pane_with_path(ptr->left, tmp);
	free(tmp);
	init_pane_with_path(ptr->center, path);
	init_pane_with_path(ptr->right, path);

	return 0;
}

/* Free a whole TabCtx linked list */
int
tabctx_deinit(TabCtx **ctx)
{
	TabCtx *tmp, *freeme;

	for (freeme = *ctx; freeme != NULL; ) {
		tmp = freeme->next;
		tabctx_free(freeme);
		freeme = tmp;
	}

	*ctx = NULL;
	return 0;
}

/* Remove a tab from the linked list */
int
tabctx_remove(TabCtx **ctx, int idx)
{
	TabCtx *tmp, *tofree;

	/* Special case: we have to remove the head */
	if (idx == 0) {
		tofree = *ctx;
		*ctx = (*ctx)->next;
		tabctx_free(tofree);
	} else {
		/* Scan until the element before the one we have to delete, so as to
		 * allow for the linked list to be relinked */
		for (tmp = *ctx; tmp != NULL && idx > 1; idx--, tmp=tmp->next)
			;

		/* tmp = NULL means that we reached the end before idx elements had
		 * passed: there's no such tab to remove */
		if (tmp) {
			tofree = tmp->next;
			tmp->next = tmp->next->next;
			tabctx_free(tofree);
		}
	}
	return 0;
}

/* Static functions {{{ */
int
tabctx_free(TabCtx *ctx)
{
	free_pane(ctx->left);
	free_pane(ctx->center);
	free_pane(ctx->right);
	free(ctx);
	return 0;
}
/*}}}*/
