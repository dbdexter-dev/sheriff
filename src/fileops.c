#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "fileops.h"
#include "utils.h"

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
move_file(char *src, char *dest, int preserve_src)
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
