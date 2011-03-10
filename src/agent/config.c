/* Copyright 2011 Joyent, Inc. */
#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "util.h"

char *
read_cfg_key(const char *file, const char *key)
{
	boolean_t next_is_val = B_FALSE;
	char buffer[BUFSIZ] = {0};
	char *rest = NULL;
        char *token = NULL;
        char *ptr = NULL;
	char *val = NULL;
	FILE *fp = NULL;

	if (file == NULL || key == NULL) {
		debug("read_cfg_key: NULL arguments\n");
		return NULL;
	}

	fp = fopen(file, "r");
	if (fp == NULL) {
		error("read_cfg_key: couldn't open %s: %s\n", file,
		      strerror(errno));
		return NULL;
	}

	while (fgets(buffer, sizeof (buffer), fp)) {
		ptr = buffer;
		while((token = strtok_r(ptr, "=", &rest)) != NULL) {
			if (next_is_val) {
				chomp(token);
				val = xstrdup(token);
				goto out;
			}

			if (strcasecmp(key, token) == 0)
				next_is_val = B_TRUE;

			ptr = rest;
		}
	}

out:
	fclose(fp);
	info("config param %s: %s\n", key, val);
	return val;
}
