#ifndef __UTILS_H
#define __UTILS_H

#include <stdlib.h>

void  octal_to_str(int oct, char str[]);
void* safealloc(size_t s);
int   strchomp(const char* src, char* dst, const int maxlen);
void  tohuman(unsigned long bytes, char* human);
int   wdshorten(const char* src, char* dest, const unsigned destsize);

#endif
