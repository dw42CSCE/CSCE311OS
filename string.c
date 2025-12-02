#include "string.h"

/* minimal strlen for kernel usage */
unsigned long strlen(const char *s) {
    unsigned long n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

/* minimal strcmp for kernel usage */
int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* minimal strncmp for kernel usage */
int strncmp(const char *a, const char *b, unsigned long n) {
    if (n == 0) return 0;
    while (n-- && *a && (*a == *b)) {
        if (n == 0) break;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* copy string with size cap, returns length of src */
int strlcpy(char *dst, const char *src, unsigned long dstsz) {
    if (!src) {
        if (dstsz) dst[0] = '\0';
        return 0;
    }
    unsigned long i = 0;
    if (dstsz == 0) return (int)strlen(src);
    while (i + 1 < dstsz && src && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return (int)strlen(src);
}

void *memset(void *dst, int val, unsigned long n) {
    unsigned char *p = (unsigned char *)dst;
    for (unsigned long i = 0; i < n; ++i) p[i] = (unsigned char)val;
    return dst;
}

void *memcpy(void *dst, const void *src, unsigned long n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (unsigned long i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}
