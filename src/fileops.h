#ifndef FILEOPS_H_MINE
#define FILEOPS_H_MINE

typedef struct {
	char *fname;
	unsigned obj_count;
	unsigned obj_done;
} Progress;

Progress *fileop_progress();
int copy_file(char *src, char *dest);
int delete_file(char *name);
int link_file(char *src, char *dest);
int move_file(char *src, char *dest);

#endif
