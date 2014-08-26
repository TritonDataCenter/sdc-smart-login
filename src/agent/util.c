/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2014, Joyent, Inc.
 */

#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "util.h"


void *
xcalloc(size_t count, size_t sz)
{
	void *ptr = NULL;

	if (count == 0 || sz == 0)
		return (NULL);

	ptr = calloc(count, sz);

	return (ptr);
}


void *
xmalloc(size_t sz)
{
	return (xcalloc(1, sz));
}

char *
xstrdup(const char *str)
{
	int len = 0;
	char *buf = NULL;
	if (str == NULL)
		return (NULL);

	len = strlen(str) + 1;
	buf = xmalloc(len);
	if (buf == NULL)
		return (NULL);

	(void) strncpy(buf, str, len);
	return (buf);
}

void
xfree(void *ptr)
{
	if (ptr != NULL)
		free(ptr);
}


void
chomp(char *s) {
	while (*s && *s != '\n' && *s != '\r')
		s++;
	*s = 0;
}
