#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

void
die(char* msg)
{
	fprintf(stderr, "[FATAL]: %s\n", msg);
	exit(1);
}

void
octal_to_str(int oct, char str[])
{
	int i;
	/* Cycle through the three rwx fields */
	for(i=0; i < 9; i++)
	{
		if(oct & (1 << i))
			switch(i % 3)
			{
				case 0:
					str[9-i] = 'x';
					break;
				case 1:
					str[9-i] = 'w';
					break;
				case 2:
					str[9-i] = 'r';
					break;
			}
		else
			str[9-i] = '-';
	}
	/* Last extended bits */
	/* TODO: this is not complete by any means */
	if (oct & 01000)
		str[0] = '+';
	else
		str[0] = '-';

	return;
}

void*
safealloc(size_t s)
{
	void* ret;

	ret = malloc(s);

	if(!ret)
		die("Malloc failed");

	return ret;
}

/* Truncate a string to length, adding "~" to the end if needed */
int
strchomp(const char* src, char* dst, const int maxlen)
{
	if(!src)
		return 1;

	strncpy(dst, src, maxlen);
	if(strlen(src) <= maxlen)
		return 0;

	dst[maxlen-1] = '~';
	dst[maxlen] = '\0';

	return 0;
}

/* Human formatting of file sizes */
void
tohuman(unsigned long bytes, char* human)
{
	const char suffix[] = "BKMGTPE";
	float fbytes;
	int exp_3;

	assert(human);

	/* Integer divide until the last moment */
	for(exp_3 = 0, fbytes = bytes; fbytes > 1000; fbytes /= 1000, exp_3++);
	if(fbytes > 100)
		sprintf(human, "%3.f %c", fbytes, suffix[exp_3]);
	else if(fbytes > 10)
		sprintf(human, "%3.1f %c", fbytes, suffix[exp_3]);
	else
		sprintf(human, "%3.2f %c", fbytes, suffix[exp_3]);
}

int
wdshorten(const char* src, char* dest, const unsigned destsize)
{
	assert(src);
	assert(dest);

	int count;
	if(src == NULL || dest == NULL)
		return -1;

	for(; *src != '\0'; src++)
	{
		if(*src == '/')
		{
			if(*(src+1) == '.')
				count = 3;
			else
				count = 2;

			if(count > destsize)
			{
				*dest = '\0';
				return 1;
			}
			for(; count>0; count--)
				*dest++ = *src++;
		}
	}
	*dest = '\0';
	return 0;
}
