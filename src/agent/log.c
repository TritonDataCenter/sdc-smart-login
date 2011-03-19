/* Copyright 2011 Joyent, Inc. */
#include <pthread.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "log.h"

static const char *PREFIX = "%s::%s T(%d) %s: ";

int g_debug_level = 0;
boolean_t g_info_enabled = B_TRUE;
boolean_t g_error_enabled = B_TRUE;

void
debug(const char *fmt, ...)
{
	va_list alist;

	if (g_debug_level <= 0)
		return;

	va_start(alist, fmt);

	(void) fprintf(stderr, PREFIX, __DATE__, __TIME__, pthread_self(),
		"DEBUG");

	(void) vfprintf(stderr, fmt, alist);
	va_end(alist);
}


void
debug2(const char *fmt, ...)
{
	va_list alist;

	if (g_debug_level <= 1)
		return;

	va_start(alist, fmt);

	(void) fprintf(stderr, PREFIX, __DATE__, __TIME__, pthread_self(),
		"DEBUG2");

	(void) vfprintf(stderr, fmt, alist);
	va_end(alist);
}

void
info(const char *fmt, ...)
{
	va_list alist;

	if (!g_info_enabled)
		return;

	va_start(alist, fmt);

	(void) fprintf(stderr, PREFIX, __DATE__, __TIME__, pthread_self(),
		"INFO");

	(void) vfprintf(stderr, fmt, alist);
	va_end(alist);
}

void
error(const char *fmt, ...)
{
	va_list alist;

	if (!g_error_enabled)
		return;

	va_start(alist, fmt);

	(void) fprintf(stderr, PREFIX, __DATE__, __TIME__, pthread_self(),
		"ERROR");

	(void) vfprintf(stderr, fmt, alist);
	va_end(alist);
}
