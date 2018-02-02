#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "clipboard.h"
#include "dir.h"
#include "utils.h"

static int move_file(char *src, char *dest, int preserve_src);
static int delete_file(char *fname);

/* Deallocate the stuff in a clipboard */
int
clip_clear(Clipboard *clip)
{
	if (clip->dir) {
		free_listing(&(clip->dir));
		clip->dir = NULL;
	}
	clip->op = 0;
	return 0;
}

/* Execute the action specified in a clipboard over the files in the clipboard */
int
clip_exec(Clipboard *clip, char *destpath)
{
	int i;
	char* tmppath;

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
	return 0;
}

int
clip_init(Clipboard *clip, Direntry* dir, int op)
{
	/* Overwrite the clipboard contents, if any */
	if (clip->dir) {
		clip_clear(clip);
	}

	clip->op = op;
	snapshot_tree_selected(&(clip->dir), dir);
	return 0;
}

/* Static functions {{{*/

/* Perma-delete a file */
int
delete_file(char *fname)
{
	pid_t pid;
	if (!fname) {
		return -1;
	}

	if (!(pid = fork())) {
		execlp("/bin/rm", "/bin/rm", fname, NULL);
		exit(0);
	} else if (pid < 0) {
		return -1;
	} else {
		waitpid(pid, NULL, 0);
	}

	return 0;
}

/* Copy and move in a single function */
int
move_file(char *dest, char *src, int preserve_src)
{
	pid_t pid;

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
		waitpid(pid, NULL, 0);
	}

	return 0;
}
/*}}}*/
