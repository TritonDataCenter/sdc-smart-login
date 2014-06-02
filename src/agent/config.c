/* Copyright 2014 Joyent, Inc. */

#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bunyan.h"
#include "config.h"
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
		bunyan_debug("read_cfg_key: NULL arguments",
		    BUNYAN_NONE);
		return (NULL);
	}

	fp = fopen(file, "r");
	if (fp == NULL) {
		bunyan_error("could not open config file",
		    BUNYAN_STRING, "file", file,
		    BUNYAN_STRING, "error", strerror(errno),
		    BUNYAN_NONE);
		return (NULL);
	}

	while (fgets(buffer, sizeof (buffer), fp)) {
		ptr = buffer;
		while ((token = strtok_r(ptr, "=", &rest)) != NULL) {
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
	(void) fclose(fp);
	if (strcmp(CFG_CAPI_PW, key) != 0) {
		bunyan_info("config param",
		    BUNYAN_STRING, "key", key,
		    BUNYAN_STRING, "value", val,
		    BUNYAN_NONE);
	} else {
		bunyan_info("config param",
		    BUNYAN_STRING, "key", key,
		    BUNYAN_NONE);
	}
	return (val);
}
