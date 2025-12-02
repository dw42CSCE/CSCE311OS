#ifndef _STRING_H
#define _STRING_H

unsigned long strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, unsigned long n);
int strlcpy(char *dst, const char *src, unsigned long dstsz);
void *memset(void *dst, int val, unsigned long n);
void *memcpy(void *dst, const void *src, unsigned long n);

#endif
