/**
 * Various simple accessory functions, that mostly operate on native data types
 * (e.g char*, int, and the like).
 */
#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>

void  die(const char *msg);
char *join_path(const char *parent, const char *child);
void  octal_to_str(int oct, char str[]);
void *safealloc(size_t s);
char *strcasestr(const char *haystack, const char *needle);
int   strchomp(const char *src, char *dest, const int maxlen);
void  tohuman(unsigned long bytes, char *human);

#endif
