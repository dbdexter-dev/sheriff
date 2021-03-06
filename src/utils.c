#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "utils.h"

static int toupper(int c);

/* Given a string containing an octal-coded integer, extract said int value */
int
atoo(const char *str)
{
	unsigned octal;

	for (octal = 0; *str != '\0'; str++) {
		if (*str < '0' || *str > '9') {
			return -1;
		}
		octal = (octal << 3) + (*str - '0');
	}

	return octal;
}

char*
extract_filename(const char *path)
{
	char *ret;

	for (ret = (char*)(path + strlen(path) - 1); ret >= path; ret--) {
		if (*ret == '/') {
			return ret+1;
		}
	}

	return NULL;
}

/* Check if a directory is "." or ".." more efficiently than calling strcmp
 * twice */
int
is_dot_or_dotdot(char *fn)
{
	if (!fn) {
		return 0;
	}
	return (fn[0] == '.' && (fn[1] == '\0' || (fn[1] == '.' && fn[2] == '\0')));
}

/* Given a parent directory and a file in it, concatenate them to create a full
 * path. XXX: remember to free the returned pointer in the caller */
char*
join_path(const char *parent, const char *child)
{
	char *ret;
	ret = safealloc(sizeof(*ret) * (strlen(parent) + strlen(child) + 1 + 1));
	sprintf(ret, "%s/%s", parent, child);
	return ret;
}

/* Convert an octal mode into a ls-like string, aka "-rwxr-xr-x" */
void
octal_to_str(int mode, char str[])
{
	int i;
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
	str[10]='\0';
	return;
}

/* Wrapper that dies when malloc fails */
void*
safealloc(size_t s)
{
	void *ret;

	ret = malloc(s);
	assert(ret);
	return ret;
}

/* Compare two strings case-insensitively */
char*
strcasestr(const char *haystack, const char *needle)
{
	int i;

	for (i = 0; haystack[i] != '\0'; ) {
		if (toupper(haystack[i]) == toupper(needle[i])) {
			i++;
			if (needle[i] == '\0') {
				return (char*)haystack;
			}
		} else {
			i = 0;
			haystack++;
		}
	}

	return NULL;
}

/* Truncate a string to length, adding "~" to the end if needed */
int
strchomp(const char *src, char *dest, const int maxlen)
{
	if (!src) {
		return 1;
	}

	memcpy(dest, src, maxlen);
	if (strlen(src) < maxlen || maxlen < 1) {
		return 0;
	}

	dest[maxlen-1] = '~';
	dest[maxlen] = '\0';
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
		for (exp_3 = 0, fbytes = bytes; fbytes > 1000; fbytes /= 1000, exp_3++)
			;
		if (fbytes > 99.9) {
			sprintf(human, "%3.f %c", fbytes, suffix[exp_3]);
		} else if (fbytes > 9.99) {
			sprintf(human, "%3.1f %c", fbytes, suffix[exp_3]);
		} else {
			sprintf(human, "%3.2f %c", fbytes, suffix[exp_3]);
		}
	}
}

/* Compress a string, truncating like the fish shell does as a default */
void
zip_path(char *str)
{
	int i, last_slash;
	char *truncd;

	if (!str || str[0] == '\0' || str[1] == '\0') {
		return;
	}

	/* There's a trailing slash most of the time */
	last_slash = strlen(str) - 2;
	for (; str[last_slash] != '/' && last_slash > 0; last_slash--)
		;

	truncd=str;
	i=0;

	while (i < last_slash) {
		*(truncd++) = str[i++];
		*(truncd++) = str[i];

		if (str[i++] == '.') {
			*(truncd++) = str[i++];
		}

		for (; str[i] != '/' && i < last_slash; i++)
			;
	}

	for (; str[i] != '\0'; i++) {
		*(truncd++) = str[i];
	}

	*(truncd) = '\0';
}

/* Static functions {{{*/
inline int
toupper(int c)
{
	return (c >= 'a' && c <= 'z') ? (c & 0xDF) : c;
}
/*}}}*/
