/**
 * Functions that operate directly on files are defined here. Most of these are
 * just wrappers around recursive, statically-defined functions in the companion
 * .c file. The Progress struct is used to report to the main thread how far
 * into an operation we are, as well as the name of the file currently being
 * copied/deleted/linked/chmodded/you_name_it.
 */

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
