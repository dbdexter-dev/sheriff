#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "utils.h"

void
die(char *msg)
{
	fprintf(stderr, "[FATAL]: %s >.<\n", msg);
	exit(1);
}

/* Given a parent directory and a file in it, concatenate them to create a full
 * path. XXX: remember to free the returned pointer in the callee */
char*
join_path(char *parent, char *child)
{
	char *ret;
	ret = safealloc(sizeof(*ret) * (strlen(parent) + strlen(child) + 1 + 1));
	sprintf(ret, "%s/%s", parent, child);
	return ret;
}

void
octal_to_str(int mode, char str[])
{
	int i;
	str[10]='\0';
	/* Cycle through the three rwx fields */
	for (i=0; i < 9; i++) {
		if (mode & (1 << i)) {
			switch (i % 3) {
			case 0:     /* Execute bit */
				str[9-i] = 'x';
				break;
			case 1:     /* Write bit */
				str[9-i] = 'w';
				break;
			case 2:     /* Read bit */
				str[9-i] = 'r';
				break;
			}
		} else {
			str[9-i] = '-';
		}
	}
	/* Last extended bits */
	/* TODO: this is not complete by any means */
	if (S_ISLNK(mode)) {
		str[0] = 'l';
	} else if (S_ISCHR(mode)) {
		str[0] = 'c';
	} else if (S_ISBLK(mode)) {
		str[0] = 'b';
	} else if (mode & S_ISUID) {
		str[0] = '+';
	} else {
		str[0] = '-';
	}

	return;
}

void*
safealloc(size_t s)
{
	void *ret;

	ret = malloc(s);
	if (!ret) {
		die("Malloc failed");
	}
	return ret;
}

/* Truncate a string to length, adding "~" to the end if needed */
int
strchomp(const char *src, char *dest, const int maxlen)
{
	if (!src) {
		return 1;
	}

	memcpy(dest, src, maxlen);
	if (strlen(src) < maxlen) {
		return 0;
	}

	dest[maxlen-2] = '~';
	dest[maxlen-1] = '\0';
	return 0;
}

/* Human formatting of file sizes */
void
tohuman(unsigned long bytes, char *human)
{
	const char suffix[] = "BKMGTPE";
	float fbytes;
	int exp_3;

	assert(human);
	if (bytes < 1000) {
		sprintf(human, "%lu %c", bytes, suffix[0]);
	} else {
		/* Integer divide until the last moment */
		for (exp_3 = 0, fbytes = bytes; fbytes > 1000; fbytes /= 1000, exp_3++)
			;
		if (fbytes >= 100) {
			sprintf(human, "%3.f %c", fbytes, suffix[exp_3]);
		} else if (fbytes >= 10) {
			sprintf(human, "%3.1f %c", fbytes, suffix[exp_3]);
		} else {
			sprintf(human, "%3.2f %c", fbytes, suffix[exp_3]);
		}
	}
}
