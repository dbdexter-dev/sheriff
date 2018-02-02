/**
 * Functions dealing with moving/renaming/copying/linking files around and the
 * like. Also clipboard management.
 */
#ifndef FILEOPS_H
#define FILEOPS_H

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
} Clipboard;

int clip_clear(Clipboard *clip);
int clip_exec(Clipboard *clip, char *destpath);
int clip_init(Clipboard *clip, Direntry *dir, int op);
int move_file(char *src, char *dest, int preserve_src);
int delete_file(char *fname);

#endif
