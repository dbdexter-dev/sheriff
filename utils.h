/**
 * Various simple accessory functions, that mostly operate on native data types
 * (e.g char*, int, and the like).
 */
#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>

void  die(char* msg);
void  octal_to_str(int oct, char str[]);
void* safealloc(size_t s);
int   strchomp(const char* src, char* dst, const int maxlen);
void  tohuman(unsigned long bytes, char* human);

#endif
