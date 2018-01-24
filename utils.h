#ifndef __UTILS_H
#define __UTILS_H

#include <stdlib.h>

void die(char* msg);
void* safealloc(size_t s);
int strchomp(const char* src, char* dst, const int maxlen);
int wdshorten(const char* src, char* dest, const unsigned destsize);
void tohuman(unsigned long bytes, char* human);

#endif
