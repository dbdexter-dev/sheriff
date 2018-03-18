#ifndef FILEOPS_H_MINE
#define FILEOPS_H_MINE

#include <fcntl.h>

typedef struct {
	char *fname;
	unsigned obj_count;
	unsigned obj_done;
} Progress;

Progress *fileop_progress();

int chmod_file(char *name, mode_t mode);
int copy_file(char *src, char *dest);
int delete_file(char *name);
int link_file(char *src, char *dest);
int move_file(char *src, char *dest);

#endif
