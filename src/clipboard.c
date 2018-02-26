#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "clipboard.h"
#include "dir.h"
#include "utils.h"
#include "ui.h"

struct pthr_clip_arg {
	Clipboard *clip;
	char *destpath;
};

static int clip_clear(Clipboard *clip);
static int move_file(char *src, char *dest, int preserve_src);
static int delete_file(char *fname);
static void *pthr_clip_exec(void *arg);

/* Deallocate a clipboard object */
int
clip_deinit(Clipboard *clip)
{
	clip_clear(clip);
	pthread_mutex_destroy(&clip->mutex);
	return 0;
}


/* Spawn a thread that will execute what the clipboard is holding */
int
clip_exec(Clipboard *clip, char *destpath)
{
	struct pthr_clip_arg *arg;
	pthread_attr_t wt_attr;
	pthread_t thr;

	arg = safealloc(sizeof(*arg));
	arg->clip = clip;
	arg->destpath = safealloc(sizeof(*arg->destpath) * (strlen(destpath) + 1));

	sprintf(arg->destpath, "%s", destpath);

	pthread_attr_init(&wt_attr);
	pthread_attr_setdetachstate(&wt_attr, PTHREAD_CREATE_DETACHED);

	pthread_create(&thr, &wt_attr, pthr_clip_exec, arg);

	return 0;
}


/* Initialize a clipboard object */
int
clip_init(Clipboard *clip, Direntry* dir, int op)
{
	/* Overwrite the clipboard contents, if any */
	if (clip->dir) {
		clip_clear(clip);
	} else {
		pthread_mutex_init(&clip->mutex, NULL);
	}

	clip->op = op;
	snapshot_tree_selected(&(clip->dir), dir);
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

/* Perma-delete a file */
int
delete_file(char *fname)
{
	pid_t pid;
	int wstatus;

	if (!fname) {
		return -1;
	}

	if (!(pid = fork())) {
		execlp("/bin/rm", "/bin/rm", fname, NULL);
		exit(0);
	} else if (pid < 0) {
		return -1;
	} else {
		waitpid(pid, &wstatus, 0);
	}

	return 0;
}

/* Copy and move in a single function */
int
move_file(char *dest, char *src, int preserve_src)
{
	pid_t pid;
	int wstatus;

	if (!src || !dest) {
		return -1;
	}

	if (!(pid = fork())) {
		if(preserve_src) {
			execlp("/bin/cp", "/bin/cp", "-rn",  src, dest, NULL);
		} else {
			execlp("/bin/mv", "/bin/mv", "-n",  src, dest, NULL);
		}
		exit(0);
	} else if (pid < 0) {
		return -1;
	} else {
		waitpid(pid, &wstatus, 0);
	}

	return 0;
}
/* Execute the action specified in a clipboard over the files in the clipboard */
void *
pthr_clip_exec(void *arg)
{
	int i;
	char *tmppath;
	Clipboard *clip;
	char *destpath;

	clip = ((struct pthr_clip_arg*)arg)->clip;
	destpath = ((struct pthr_clip_arg*)arg)->destpath;

	pthread_mutex_lock(&clip->mutex);

	if (clip->dir) {
		switch(clip->op) {
		case OP_COPY:
			for (i=0; i<clip->dir->count; i++) {
				tmppath = join_path(clip->dir->path, clip->dir->tree[i]->name);
				move_file(destpath, tmppath, 1);
				free(tmppath);
			}
			break;
		case OP_MOVE:
			for (i=0; i<clip->dir->count; i++) {
				tmppath = join_path(clip->dir->path, clip->dir->tree[i]->name);
				move_file(destpath, tmppath, 0);
				free(tmppath);
			}
			break;
		case OP_LINK:
			break;
		case OP_DELETE:
			for (i=0; i<clip->dir->count; i++) {
				tmppath = join_path(clip->dir->path, clip->dir->tree[i]->name);
				delete_file(tmppath);
				free(tmppath);
			}
			break;
		case OP_RENAME:
			break;
		default:
			break;
		}
	}

	pthread_mutex_unlock(&clip->mutex);
	free(destpath);
	/* Signal the main thread that the current directory contents have changed */
	kill(0, SIGUSR1);
	/* arg was passed on the heap to prevent it being overwritten */
	free(arg);
	return NULL;
}
/*}}}*/
