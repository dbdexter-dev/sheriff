/**
 * Functions dealing with moving/renaming/copying/linking files around and the
 * like. Also clipboard management.
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
	OP_RENAME
};

typedef struct filebuffer {
	char *file;
	int preserve_src;
	struct filebuffer* next;
} Filebuffer;

typedef struct {
	int op;
	Direntry* dir;
	pthread_mutex_t mutex;
} Clipboard;

int clip_deinit(Clipboard *clip);
int clip_exec(Clipboard *clip, char *destpath);
int clip_init(Clipboard *clip, Direntry *dir, int op);

#endif
