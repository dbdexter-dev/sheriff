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

static int s_chmod_file(char *name, mode_t mode);
static int s_copy_file(char *src, char *dest);
static int s_delete_file(char *name);

static Progress m_progress;

/* Accessory function to get the m_progress static global outside of here */
Progress *
fileop_progress()
{
	return &m_progress;
}

void
fileops_deinit()
{
	pthread_mutex_destroy(&m_progress.mutex);
}

void
fileops_init()
{
	pthread_mutex_init(&m_progress.mutex, NULL);
	m_progress.obj_count = 0;
	m_progress.obj_done = 0;
}


/* Chmod a file, and if it's a directory, chmod all of its contents as well */
int
chmod_file(char *name, mode_t mode)
{
	s_chmod_file(name, mode);
	queue_master_update();

	return 0;
}

/* Copy a file, and if it's a directory, all of its contents as well */
int
copy_file(char *src, char *dest)
{
	int copy_status;

	copy_status = s_copy_file(src, dest);

	switch (copy_status) {
	case ENOMEM:    /* 4 intentional fallthroughs */
	case EINVAL:
	case EOVERFLOW:
	case EIO:
		delete_file(dest);
		break;
	default:
		break;
	}

	queue_master_update();

	return copy_status;
}

/* Delete a file, and if it's a directory, all of its contents as well */
int
delete_file(char *name)
{
	int delete_status;

	delete_status = s_delete_file(name);
	queue_master_update();

	return delete_status;
}

/* Symlink a file, and only that file, even if it is a directory */
int
link_file(char *src, char *dest)
{
	if (symlink(src, dest) < 0) {
		return errno;
	}
	return 0;
}

/* Move something by trying to use rename() since it's faster and atomic by
 * definition. If this does not work, resort to copying src to dest and deleting
 * the source. If anything fails during the copy, bail out, since we can't be
 * sure the files have made it safely to their new destination */
int
move_file(char *src, char *dest)
{
	int retval;

	retval = 0;
	if (rename(src, dest)) {    /* Try to rename atomically */
		if (errno == EXDEV) {   /* We're moving across filesystems */
			if ((retval = copy_file(src, dest)) < 0) {
				return retval;
			}
			if ((retval = delete_file(src)) < 0) {
				return retval;
			}
		} else {
			return errno;
		}
	}
	return retval;
}

/* Static functions {{{ */
/* Enumerate a directory contents to guesstimate how long an operation will
 * approximately take. Note than a finer-grained estimate would slow down the
 * operation significatly, because it wouldn't be carried out in a single
 * sendfile() call like it is now */
unsigned
enumerate_dir(char *path)
{
	DIR *dp;
	struct dirent *ep;
	struct stat st;
	char *subpath;
	unsigned count;

	count = 1;
	if ((dp = opendir(path))) {
		while ((ep = readdir(dp))) {
			if (!is_dot_or_dotdot(ep->d_name)) {
				count++;
				subpath = join_path(path, ep->d_name);
				lstat(subpath, &st);

				if (S_ISDIR(st.st_mode)) {
					count += enumerate_dir(subpath);
				}

				free(subpath);
			}
		}
		closedir(dp);
	}

	return count;
}

/* Recursive chmodding of a directory and its children. Possible TODO: add an
 * option to just chmod the top level file */
int
s_chmod_file(char *name, mode_t mode)
{
	char *subpath;
	struct stat st;
	DIR *dp;
	struct dirent *ep;

	lstat(name, &st);
	chmod(name, mode);

	pthread_mutex_lock(&m_progress.mutex);
	m_progress.obj_done++;
	pthread_mutex_unlock(&m_progress.mutex);

	if (S_ISDIR(st.st_mode) && (dp = opendir(name))) {
		while ((ep = readdir(dp))) {                /* Self-call on subnodes */
			if (!is_dot_or_dotdot(ep->d_name)) {    /* Except for . and .. */
				subpath = join_path(name, ep->d_name);
				s_chmod_file(subpath, mode);
				free(subpath);
			}
		}
		closedir(dp);   /* Don't forget to pick up the trash on your way out! */
	}

	return 0;
}

/* Recursive copying backend. Possible TODO: make this iterative */
int
s_copy_file(char *src, char *dest)
{
	char *subpath_src, *subpath_dest;
	int in_fd, out_fd;
	int retval;
	DIR *dp;
	struct stat st;
	struct dirent *ep;

	pthread_mutex_lock(&m_progress.mutex);
	m_progress.fname = dest;
	pthread_mutex_unlock(&m_progress.mutex);

	retval = -1;
	if ((in_fd = open(src, O_RDONLY)) < 0) {
		return retval;
	}
	if (fstat(in_fd, &st)) {
		return retval;
	}

	if ((dp = fdopendir(in_fd))) {      /* Try to open in_fd as a directory */
		mkdir(dest, st.st_mode);        /* """copy""" the directory */

		/* Scan the directory, and self-call on each node contained inside */
		while ((ep = readdir(dp))) {
			if (!is_dot_or_dotdot(ep->d_name)) {    /* Skip . and .. */
				subpath_src = join_path(src, ep->d_name);
				subpath_dest = join_path(dest, ep->d_name);
				retval = s_copy_file(subpath_src, subpath_dest);

				pthread_mutex_lock(&m_progress.mutex);
				m_progress.fname = dest;
				pthread_mutex_unlock(&m_progress.mutex);

				free(subpath_src);
				free(subpath_dest);

				/* Clean up the mess if the copy failed (read: delete the file
				 * if something bad happened during the copy */
				switch (retval) {
				case ENOMEM:            /* 4 intentional fallthroughs */
				case EINVAL:
				case EOVERFLOW:
				case EIO:
					delete_file(subpath_dest);
					break;
				default:
					break;
				}

			}
		}
		closedir(dp);                   /* No need to close(in_fd) */
	} else {
		if (errno == ENOTDIR) {         /* Src is a file: fstat and copy it */
			if ((out_fd = open(dest, O_WRONLY|O_CREAT, st.st_mode)) >= 0) {
				if (sendfile(out_fd, in_fd, NULL, st.st_size) < 0) {
					retval = errno;
				} else {
					retval = 0;
				}
				close(out_fd);
			} else {
				retval = errno;
			}
		}
		close(in_fd);
	}


	pthread_mutex_lock(&m_progress.mutex);
	m_progress.obj_done++;
	pthread_mutex_unlock(&m_progress.mutex);

	queue_master_update();              /* Request an UI update */

	return retval;
}

/* Recursive deletion. Possible TODO: make this iterative */
int
s_delete_file(char *name)
{
	DIR *dp;
	struct dirent *ep;
	char *subpath;
	struct stat st;

	lstat(name, &st);

	pthread_mutex_lock(&m_progress.mutex);
	m_progress.fname = name;
	pthread_mutex_unlock(&m_progress.mutex);

	/* Delete a directory's contents before attempting to delete the directory
	 * itself to prevent an ENOTEMPTY */
	if(S_ISDIR(st.st_mode) && (dp = opendir(name))) {
		while ((ep = readdir(dp))) {
			if (!is_dot_or_dotdot(ep->d_name)) {
				subpath = join_path(name, ep->d_name);
				s_delete_file(subpath);
				free(subpath);
			}
		}
		closedir(dp);
	}

	/* Delete the file/folder itself */
	if (remove(name) < 0) {
		return errno;
	}

	pthread_mutex_lock(&m_progress.mutex);
	m_progress.obj_done++;
	pthread_mutex_unlock(&m_progress.mutex);

	queue_master_update();

	return 0;
}
/*}}}*/
