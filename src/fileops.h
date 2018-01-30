/**
 * Functions dealing with moving/renaming/copying/linking files around and the
 * like.
 */
#ifndef FILEOPS_H
#define FILEOPS_H

typedef struct filebuffer {
	char *file;
	int preserve_src;
	struct filebuffer* next;
} Filebuffer;

int move_file(char *src, char *dest, int preserve_src);
int delete_file(char *fname);
int yank_file(Filebuffer *fb, char *path);

#endif
