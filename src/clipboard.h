/**
 * Functions that operate on a Clipboard struct are defined here.
 * A Clipboard struct contains a snapshot of the files to operate on, as well as
 * the operation to apply to these files. The mutex prevents things like a 
 * deallocation from happening while a copy is in progress.
 */

#ifndef FILEOPS_H
#define FILEOPS_H

#include <pthread.h>
#include "dir.h"

enum clip_ops {
	OP_COPY,
	OP_MOVE,
	OP_LINK,
	OP_DELETE,
	OP_CHMOD
};

typedef struct {
	enum clip_ops op;
	Direntry* dir;
	pthread_mutex_t mutex;
} Clipboard;

int clip_change_op(enum clip_ops op);
int clip_deinit();
int clip_exec(char *destpath);
int clip_init();
int clip_update(Direntry *dir, int op);

#endif
