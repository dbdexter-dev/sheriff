/**
 * Functions dealing with moving/renaming/copying/linking files around and the
 * like
 */
#ifndef FILEOPS_H
#define FILEOPS_H

typedef struct {
	char *file;
	int preserve_src;
} Filebuffer;

int move_file(char *src, char *dest, int preserve_src);
int delete_file(char *fname);

#endif
