/* Copyright 2011 Joyent, Inc. */
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "util.h"


void *
xcalloc(size_t count, size_t sz)
{
	void *ptr = NULL;

	if (count == 0 || sz == 0)
		return NULL;

	ptr = calloc(count, sz);
	if (ptr == NULL)
		error("Out of Memory (%d bytes)\n", sz * count);

	return ptr;
}


void *
xmalloc(size_t sz)
{
	return xcalloc(1, sz);
}

char *
xstrdup(const char *str)
{
	int len = 0;
	char *buf = NULL;
	if (str == NULL)
		return NULL;

	len = strlen(str) + 1;
	buf = xmalloc(len);
	if (buf == NULL)
		return NULL;

	strncpy(buf, str, len);
	return buf;
}

void
xfree(void *ptr)
{
	if (ptr != NULL)
		free(ptr);
}


void
chomp(char *s) {
	while(*s && *s != '\n' && *s != '\r') s++;
	*s = 0;
}


long
get_system_us() {
	struct timeval tp = {0};
	gettimeofday(&tp, NULL);
	return tp.tv_usec;
}
