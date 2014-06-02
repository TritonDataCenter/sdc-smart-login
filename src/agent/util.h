/* Copyright 2014 Joyent, Inc. */

#ifndef UTIL_H_
#define	UTIL_H_

#include <sys/types.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	__SENTINEL	__attribute__((sentinel))
#define	__UNUSED	__attribute__((unused))

#define	HR_USEC(a)	((int)((a) / 1000LL))
#define	HR_MSEC(a)	((int)((a) / 1000000LL))
#define	HR_SEC(a)	((int)((a) / 1000000000LL))

/**
 * Simple wrapper over calloc
 *
 */
extern void *xcalloc(size_t count, size_t sz);

/**
 * Simple wrapper over memory allocation..
 *
 * This call will allocate an entry of the size requested, and checks for a
 * NULL response from malloc.  If NULL, Logs an out of memory message to
 * stderr.  Your memory will be initialize to '\0' (i.e., this wraps calloc).
 *
 */
extern void *xmalloc(size_t sz);

/**
 * Simple wrapper over strdup
 */
extern char *xstrdup(const char *str);

/**
 * Simple check if ptr is NULL before a free.
 *
 * This could be a macro, but assuradly the compiler will inline this...
 *
 * @param ptr
 */
extern void xfree(void *ptr);

/**
 * Destructively removes any bytes after a terminating '\0'.
 *
 * @param s
 */
extern void chomp(char *s);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_H_ */
