/* Copyright 2011 Joyent, Inc. */
#include <alloca.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "util.h"

static const int BUF_SZ = 27;
static const char *PREFIX = "%s GMT T(%d) %s: ";

int g_debug_level = 0;
boolean_t g_info_enabled = B_TRUE;
boolean_t g_error_enabled = B_TRUE;

static void
make_time_str(char **buf)
{
	struct tm tm = {};
	time_t now;

	now = time(0);
	(void) gmtime_r(&now, &tm);
	(void) asctime_r(&tm, *buf, BUF_SZ);
	chomp(*buf);
}

void
debug(const char *fmt, ...)
{
	char *buf = (char *)alloca(BUF_SZ);
	va_list alist;

	if (g_debug_level <= 0 || buf == NULL)
		return;

	va_start(alist, fmt);

	make_time_str(&buf);
	(void) fprintf(stderr, PREFIX, buf, pthread_self(), "DEBUG");
	(void) vfprintf(stderr, fmt, alist);
	va_end(alist);
}


void
debug2(const char *fmt, ...)
{
	char *buf = (char *)alloca(BUF_SZ);
	va_list alist;

	if (g_debug_level <= 1 || buf == NULL)
		return;

	va_start(alist, fmt);

	make_time_str(&buf);
	(void) fprintf(stderr, PREFIX, buf, pthread_self(), "DEBUG2");
	(void) vfprintf(stderr, fmt, alist);
	va_end(alist);
}

void
info(const char *fmt, ...)
{
	char *buf = (char *)alloca(BUF_SZ);
	va_list alist;

	if (!g_info_enabled || buf == NULL)
		return;

	va_start(alist, fmt);

	make_time_str(&buf);
	(void) fprintf(stderr, PREFIX, buf, pthread_self(), "INFO");
	(void) vfprintf(stderr, fmt, alist);
	va_end(alist);
}

void
error(const char *fmt, ...)
{
	char *buf = (char *)alloca(BUF_SZ);
	va_list alist;

	if (!g_error_enabled || buf == NULL)
		return;

	va_start(alist, fmt);
	make_time_str(&buf);
	(void) fprintf(stderr, PREFIX, buf, pthread_self(), "ERROR");
	(void) vfprintf(stderr, fmt, alist);
	va_end(alist);
}
