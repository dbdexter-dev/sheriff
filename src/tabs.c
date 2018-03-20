#include <assert.h>
#include <string.h>
#include "backend.h"
#include "tabs.h"
#include "utils.h"

static int tabctx_free(TabCtx *ctx);

/* Internl linked list, used to store the currently open tabs */
static TabCtx *m_ctx;
static int m_tabcount = 0;

/* Add a tab to a TabCtx linked list, populating it from a path */
int
tabctx_append(const char *path)
{
	char *tmp;
	TabCtx *ptr;

	/* Allocate another element and insert it at the beginning of the list */
	ptr = safealloc(sizeof(*ptr));
	ptr->next = m_ctx;
	m_ctx = ptr;

	ptr->left = calloc(1, sizeof(*ptr->left));
	ptr->center = calloc(1, sizeof(*ptr->center));
	ptr->right = calloc(1, sizeof(*ptr->right));

	/* Initialize left with ${path}/../, center with ${path}, right with
	 * ${path}/${center[0]}, if this makes any sense */
	tmp = join_path(path, "..");
	init_pane_with_path(ptr->left, tmp);
	free(tmp);

	init_pane_with_path(ptr->center, path);

	tmp = join_path(path, ptr->center->dir->tree[0]->name);
	if (S_ISDIR(ptr->center->dir->tree[0]->mode)) {
		init_pane_with_path(ptr->right, tmp);
	} else {
		init_pane_with_path(ptr->right, NULL);
	}
	free(tmp);

	m_tabcount++;
	return 0;
}

/* Retun the idxth tab context in the linked list, taking the liberty to update
 * idx in case it is out of range */
TabCtx *
tabctx_by_idx(int *idx)
{
	TabCtx *retval;
	int i;

	/* Compute a valid index if a weird ass one was supplied */
	i = (m_tabcount ? (*idx % m_tabcount) : 0);
	if (i < 0) {
		i += m_tabcount;
	}
	*idx = i;

	/* Count tabs until the idxth one is reached */
	for (retval = m_ctx; i > 0; i--) {
		retval = retval->next;
	}

	return retval;
}

void
tabctx_cd(TabCtx *ctx, const char *path)
{
	char *tmp, *ptr;

	init_pane_with_path(ctx->center, path);

	tmp = join_path(path, ctx->center->dir->tree[0]->name);
	init_pane_with_path(ctx->right, tmp);

	if (S_ISDIR(ctx->center->dir->tree[0]->mode)) {
		for (ptr = tmp + strlen(tmp); *ptr != '/'; ptr--);
		for (ptr--; *ptr != '/'; ptr--);
		*(ptr+1) = '\0';
		init_pane_with_path(ctx->left, tmp);
	} else {
		init_pane_with_path(ctx->left, NULL);
	}

	free(tmp);

}

/* Free a whole TabCtx linked list */
int
tabctx_deinit()
{
	TabCtx *tmp, *freeme;

	for (freeme = m_ctx; freeme != NULL; ) {
		tmp = freeme->next;
		tabctx_free(freeme);
		freeme = tmp;
	}

	m_ctx = NULL;
	m_tabcount = 0;
	return 0;
}

/* Get the m_ctx linked list out of this file. This shouldn't be used by
 * anything but the update_status_top function, since it has to know all the
 * tabs that are open */
TabCtx*
tabctx_get()
{
	return m_ctx;
}

/* Remove a tab from the linked list. Returns 1 if there are no tabs left, 0
 * otherwise */
int
tabctx_remove(int idx)
{
	TabCtx *tmp, *tofree;

	/* Special case: we have to remove the head */
	if (idx == 0) {
		tofree = m_ctx;
		m_ctx = (m_ctx)->next;
		tabctx_free(tofree);
	} else {
		/* Scan until the element before the one we have to delete, so as to
		 * allow for the linked list to be relinked */
		for (tmp = m_ctx; tmp != NULL && idx > 1; idx--, tmp=tmp->next)
			;

		/* tmp = NULL means that we reached the end before idx elements had
		 * passed: there's no such tab to remove */
		if (tmp) {
			tofree = tmp->next;
			tmp->next = tmp->next->next;
			tabctx_free(tofree);
		}
	}

	m_tabcount--;
	if (m_tabcount < 1) {
		return 1;
	}
	return 0;
}

/* Static functions {{{ */
/* Free a tab context */
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
