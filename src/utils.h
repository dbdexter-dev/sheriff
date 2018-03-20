/**
 * Various simple accessory functions, that mostly operate on native data types
 * (e.g char*, int, and the like).
 */
#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>

int   atoo(const char *str);
const char* extract_filename(const char *path);
int   is_dot_or_dotdot(char *name);
char* join_path(const char *parent, const char *child);
void  octal_to_str(int oct, char str[]);
void* safealloc(size_t s);
char* strcasestr(const char *haystack, const char *needle);
int   strchomp(const char *src, char *dest, const int maxlen);
void  tohuman(unsigned long bytes, char *human);
void  zip_path(char *str);

#endif
