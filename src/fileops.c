#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fileops.h"
#include "sheriff.h"
#include "utils.h"

static Progress m_progress;
static unsigned enumerate_dir(char *path);
static int      s_copy_file(char *src, char *dest);

Progress *
fileop_progress()
{
	return &m_progress;
}

int
copy_file(char *src, char *dest)
{
	m_progress.obj_count = enumerate_dir(src);
	m_progress.obj_done = 0;
	return s_copy_file(src, dest);
}

/* Recursive deletion TODO: make this iterative */
int
delete_file(char *name)
{
	DIR *dp;
	struct dirent *ep;
	char *subpath;
	struct stat st;

	stat(name, &st);

	/* Delete a directory's contents before attempting to delete the directory
	 * itself */
	if(S_ISDIR(st.st_mode)) {
		if ((dp = opendir(name))) {
			while ((ep = readdir(dp))) {
				if (!is_dot_or_dotdot(ep->d_name)) {
					subpath = join_path(name, ep->d_name);
					delete_file(subpath);
					free(subpath);
				}
			}
			closedir(dp);
		}
	}

	if (remove(name) < 0) {
		return errno;
	}

	return 0;
}


int
link_file(char *src, char *dest)
{
	if (symlink(dest, src) < 0) {
		return errno;
	}
	return 0;
}

int
move_file(char *src, char *dest)
{
	int retval;
	if ((retval = copy_file(src, dest)) < 0) {
		return retval;
	}
	if ((retval = delete_file(src)) < 0) {
		return retval;
	}
	return retval;
}

/* Static functions {{{ */
unsigned
enumerate_dir(char *path)
{
	DIR *dp;
	struct dirent *ep;
	struct stat st;
	char *subpath;
	unsigned ret;

	ret = 0;
	if ((dp = opendir(path))) {
		while ((ep = readdir(dp))) {
			ret++;
			if (!is_dot_or_dotdot(ep->d_name)) {
				if (ep->d_type == DT_DIR) {
					subpath = join_path(path, ep->d_name);
					ret += enumerate_dir(subpath);
					free(subpath);
				} else if (ep->d_type == DT_UNKNOWN) {
					subpath = join_path(path, ep->d_name);
					lstat(subpath, &st);

					if (S_ISDIR(st.st_mode)) {
						ret += enumerate_dir(subpath);
					}

					free(subpath);
				}
			}
		}
		closedir(dp);
	}

	return ret;
}

/* Recursive copying TODO: make this iterative */
int
s_copy_file(char *src, char *dest)
{
	int in_fd, out_fd;
	struct stat st;
	DIR *dp;
	struct dirent *ep;
	char *subpath_src, *subpath_dest;
	int retval;

	retval = -1;

	m_progress.fname = dest;

	if ((in_fd = open(src, O_RDONLY)) >= 0) {
		fstat(in_fd, &st);

		/* Recursive copy if it's a directory */
		if (S_ISDIR(st.st_mode)) {
			close(in_fd);
			mkdir(dest, st.st_mode);
			m_progress.obj_done++;
			if ((dp = opendir(src))) {
				while ((ep = readdir(dp))) {
					if (!is_dot_or_dotdot(ep->d_name)) {
						subpath_src = join_path(src, ep->d_name);
						subpath_dest = join_path(dest, ep->d_name);

						retval = s_copy_file(subpath_src, subpath_dest);
						queue_master_update(UPDATE_ALL);

						free(subpath_src);
						free(subpath_dest);
					}
				}
				closedir(dp);
			}
		} else {
			if ((out_fd = open(dest, O_WRONLY|O_CREAT, &st.st_mode)) >= 0) {
/*				fchown(out_fd, st.st_uid, st.st_gid); */
				if (sendfile(out_fd, in_fd, NULL, st.st_size) < 0) {
					retval = errno;
				} else {
					retval = 0;
				}

				m_progress.obj_done++;
				close(out_fd);
				close(in_fd);
			}
		}
	}

	m_progress.fname = NULL;

	return retval;
}
/*}}}*/