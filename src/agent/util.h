/* Copyright 2011 Joyent, Inc. */
#ifndef UTIL_H_
#define UTIL_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Simple wrapper over calloc
 *
 */
void *xcalloc(size_t count, size_t sz);

/**
 * Simple wrapper over memory allocation..
 *
 * This call will allocate an entry of the size requested, and checks for a
 * NULL response from malloc.  If NULL, Logs an out of memory message to
 * stderr.  Your memory will be initialize to '\0' (i.e., this wraps calloc).
 *
 */
void *xmalloc(size_t sz);

/**
 * Simple wrapper over strdup
 */
char *xstrdup(const char *str);

/**
 * Simple check if ptr is NULL before a free.
 *
 * This could be a macro, but assuradly the compiler will inline this...
 *
 * @param ptr
 */
void xfree(void *ptr);

/**
 * Destructively removes any trailing whitespace.
 *
 * @param s
 */
void chomp(char *s);

/**
 * Simple wrapper over gettimeofday()->tv_usec.
 *
 * @return current unix time in microseconds
 */
long get_system_us();

#ifdef __cplusplus
}
#endif

#endif /* UTIL_H_ */
