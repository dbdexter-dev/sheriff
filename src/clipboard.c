#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "clipboard.h"
#include "dir.h"
#include "fileops.h"
#include "sheriff.h"
#include "utils.h"
#include "ui.h"

/* Struct that gets passed to a thread by reference */
struct pthr_clip_arg {
	Clipboard *clip;
	char *destpath;
};

static Clipboard m_clip;

static int  clip_clear(Clipboard *clip);
static int  clip_clone(Clipboard *dest, Clipboard *src);
static void* pthr_clip_exec(void *arg);

int
clip_change_op(enum clip_ops op)
{
	m_clip.op = op;
	return 0;
}
/* Deallocate a clipboard object */
int
clip_deinit()
{
	clip_clear(&m_clip);
	pthread_mutex_destroy(&m_clip.mutex);
	return 0;
}


/* Spawn a thread that will execute what the clipboard is holding */
int
clip_exec(char *destpath)
{
	struct pthr_clip_arg *arg;
	pthread_attr_t wt_attr;
	pthread_t thr;
	sigset_t wt_sigset, wt_old_sigset;

	arg = safealloc(sizeof(*arg));

	arg->clip = safealloc(sizeof(Clipboard));
	clip_clone(arg->clip, &m_clip);
	arg->destpath = safealloc(sizeof(*arg->destpath) * (strlen(destpath) + 1));
	sprintf(arg->destpath, "%s", destpath);

	sigemptyset(&wt_sigset);
	sigaddset(&wt_sigset, SIGUSR1);

	pthread_attr_init(&wt_attr);
	pthread_attr_setdetachstate(&wt_attr, PTHREAD_CREATE_DETACHED);

	pthread_sigmask(SIG_SETMASK, &wt_sigset, &wt_old_sigset);
	pthread_create(&thr, &wt_attr, pthr_clip_exec, arg);
	pthread_sigmask(SIG_SETMASK, &wt_old_sigset, NULL);

	pthread_attr_destroy(&wt_attr);

	return 0;
}

/* Initialize a clipboard object */
int
clip_init()
{
	memset(&m_clip, '\0', sizeof(m_clip));
	pthread_mutex_init(&m_clip.mutex, NULL);
	return 0;
}

/* Update a clipboard object with a specified path and operation*/
int
clip_update(Direntry* dir, int op)
{
	/* Overwrite the clipboard contents, if any */
	clip_clear(&m_clip);

	m_clip.op = op;
	snapshot_tree_selected(&m_clip.dir, dir);
	return 0;
}

/* Static functions {{{*/
/* Clear a clipboard */
int
clip_clear(Clipboard *clip)
{
	pthread_mutex_lock(&clip->mutex);
	if (clip->dir) {
		free_listing(&clip->dir);
		clip->dir = NULL;
	}
	clip->op = 0;
	pthread_mutex_unlock(&clip->mutex);
	return 0;
}

int
clip_clone(Clipboard *dest, Clipboard *src)
{
	dest->op = src->op;

	pthread_mutex_lock(&src->mutex);
	assert(!snapshot_tree_selected(&dest->dir, src->dir));
	pthread_mutex_unlock(&src->mutex);

	return 0;
}

/* Execute the action specified in a clipboard over the files in the clipboard */
void *
pthr_clip_exec(void *arg)
{
	int i, status;
	mode_t mode;
	char *tmpsrc, *tmpdest;
	Clipboard *clip;
	char *destpath;

	/* NOTE: destpath here is improperly named, as it can also contain the
	 * permissions to give to the files coded in octal. This isn't too bad,
	 * since a chmod doesn't need a destpath */
	clip = ((struct pthr_clip_arg*)arg)->clip;
	destpath = ((struct pthr_clip_arg*)arg)->destpath;
	status = 0;

	if (clip->dir) {
		switch(clip->op) {
		case OP_COPY:
			for (i=0; i<clip->dir->count; i++) {
				tmpsrc = join_path(clip->dir->path, clip->dir->tree[i]->name);
				tmpdest = join_path(destpath, clip->dir->tree[i]->name);
				status |= copy_file(tmpsrc, tmpdest);
				free(tmpsrc);
				free(tmpdest);
			}
			break;
		case OP_MOVE:
			for (i=0; i<clip->dir->count; i++) {
				tmpsrc = join_path(clip->dir->path, clip->dir->tree[i]->name);
				tmpdest = join_path(destpath, clip->dir->tree[i]->name);
				status |= move_file(tmpsrc, tmpdest);
				free(tmpsrc);
				free(tmpdest);
			}
			break;
		case OP_LINK:
			for (i=0; i<clip->dir->count; i++) {
				tmpsrc = join_path(clip->dir->path, clip->dir->tree[i]->name);
				tmpdest = join_path(destpath, clip->dir->tree[i]->name);
				status |= link_file(tmpsrc, tmpdest);
				free(tmpsrc);
				free(tmpdest);
			}
			break;
		case OP_DELETE:
			for (i=0; i<clip->dir->count; i++) {
				tmpsrc = join_path(clip->dir->path, clip->dir->tree[i]->name);
				status |= delete_file(tmpsrc);
				free(tmpsrc);
			}
			break;
		case OP_CHMOD:
			mode = atoo(destpath);
			if (mode > 0) {
				for (i=0; i<clip->dir->count; i++) {
					tmpsrc = join_path(clip->dir->path, clip->dir->tree[i]->name);
					status |= chmod_file(tmpsrc, mode);
					free(tmpsrc);
				}
			}
			break;
		default:
			break;
		}
	}

	free(destpath);
	/* Signal the main thread that the current directory contents have changed */
	queue_master_update();
	/* arg was passed on the heap to prevent it being overwritten */
	free_listing(&clip->dir);
	free(clip);
	free(arg);
	return NULL;
}
/*}}}*/
