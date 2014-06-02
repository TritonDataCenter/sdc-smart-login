/* Copyright 2014 Joyent, Inc. */

#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <sys/varargs.h>
#include <unistd.h>
#include <netdb.h>

#include <libnvpair.h>

#include "nvpair_json.h"
#include "bunyan.h"
#include "util.h"

#define	LOGGER_NAME		"smart-login"
#define	ISO_TIME_BUF_LEN	26

/*
 * This file implements logging in the style of Bunyan[1], a Javascript
 * library for emitting log records as a stream of line-separated JSON
 * objects.
 *
 * The core fields we emit with every record are as follows:
 *
 *   v         integer      version string, fixed to 0
 *   level     integer      log level (e.g. INFO = 30, etc)
 *   name      string       the program/logger name
 *   hostname  string       the system hostname
 *   pid       integer      the current process id
 *   time      string       an ISO 8601 formatted date string
 *   msg       string       the descriptive log message
 *
 * We also include any named parameters provided in the varargs list
 * for the bunyan_*() logging functions, formatted as per the specified
 * type.
 *
 * [1] https://github.com/trentm/node-bunyan
 */


/*
 * The current global log level.  Log records of a lower level will not be
 * emitted to stdout.  Set via bunyan_level().
 */
static int _bunyan_level = BUNYAN_INFO;

/*
 * Format the current date/time as an ISO 8601 string in the provided buffer.
 * Buffer must be at least ISO_TIME_BUF_LEN bytes long.
 */
static int
bunyan_iso_time(char *buf)
{
	struct timeval tv;
	struct tm tm;

	if (gettimeofday(&tv, NULL) != 0)
		return (-1);

	if (gmtime_r(&tv.tv_sec, &tm) == NULL)
		return (-1);

	if (strftime(buf, ISO_TIME_BUF_LEN, "%FT%T", &tm) != 19) {
		return (-1);
	}

	(void) snprintf(&buf[19], 6, ".%03dZ", (int)(tv.tv_usec / 1000));

	return (0);
}

/*
 * Emit a bunyan log record.
 */
static int
bunyan_vlog(int level, char *msg, va_list *ap)
{
	int rc = -1;
	nvlist_t *nvl;
	char buf[ISO_TIME_BUF_LEN];
	char namebuf[MAXHOSTNAMELEN];
	int type;

	if ((rc = nvlist_alloc(&nvl, NV_UNIQUE_NAME, 0)) != 0) {
		fprintf(stderr, "could not allocate nvlist\n");
		return (-1);
	}

	/*
	 * Get hostname for log record.
	 */
	(void) gethostname(namebuf, sizeof (namebuf));
	namebuf[sizeof (namebuf) - 1] = '\0';

	if (bunyan_iso_time(buf) != 0 ||
	    nvlist_add_string(nvl, "time", buf) != 0 ||
	    nvlist_add_string(nvl, "hostname", namebuf) != 0 ||
	    nvlist_add_string(nvl, "name", LOGGER_NAME) != 0 ||
	    nvlist_add_int32(nvl, "pid", (int32_t)getpid()) != 0 ||
	    nvlist_add_int32(nvl, "level", level) != 0 ||
	    nvlist_add_int32(nvl, "v", 0) != 0 ||
	    nvlist_add_string(nvl, "msg", msg != NULL ? msg : "")) {
		goto out;
	}

	while ((type = va_arg(*ap, int)) != (int)BUNYAN_NONE) {
		char *name = va_arg(*ap, char *);

		switch (type) {
		case BUNYAN_POINTER: {
			void *ptr = va_arg(*ap, void *);
			char buf[100];
			snprintf(buf, sizeof (buf), "0x%p", ptr);
			if (nvlist_add_string(nvl, name, buf) != 0) {
				goto out;
			}
			break;
		}

		case BUNYAN_BOOLEAN: {
			int b = va_arg(*ap, boolean_t);
			if (nvlist_add_boolean_value(nvl, name, b ? B_TRUE :
			    B_FALSE) != 0) {
				goto out;
			}
			break;
		}

		case BUNYAN_STRING: {
			char *str = va_arg(*ap, char *);
			if (nvlist_add_string(nvl, name, str) != 0) {
				goto out;
			}
			break;
		}

		case BUNYAN_INT32: {
			int32_t i = va_arg(*ap, int32_t);
			if (nvlist_add_int32(nvl, name, i) != 0) {
				goto out;
			}
			break;
		}

		default:
			fprintf(stderr, "UNKNOWN TYPE: %u\n", type);
			abort();
			break;
		}
	}

	rc = bunyan_nvlist_print_json(stdout, nvl);
	fprintf(stdout, "\n");
	fflush(stdout);

out:
	(void) nvlist_free(nvl);

	return (rc);
}

int
bunyan_trace(char *msg, ...)
{
	va_list ap;
	int err;

	if (_bunyan_level > BUNYAN_TRACE)
		return (0);

	va_start(ap, msg);
	err = bunyan_vlog(BUNYAN_TRACE, msg, &ap);
	va_end(ap);

	return (err);
}

int
bunyan_debug(char *msg, ...)
{
	va_list ap;
	int err;

	if (_bunyan_level > BUNYAN_DEBUG)
		return (0);

	va_start(ap, msg);
	err = bunyan_vlog(BUNYAN_DEBUG, msg, &ap);
	va_end(ap);

	return (err);
}

int
bunyan_info(char *msg, ...)
{
	va_list ap;
	int err;

	if (_bunyan_level > BUNYAN_INFO)
		return (0);

	va_start(ap, msg);
	err = bunyan_vlog(BUNYAN_INFO, msg, &ap);
	va_end(ap);

	return (err);
}

int
bunyan_warn(char *msg, ...)
{
	va_list ap;
	int err;

	if (_bunyan_level > BUNYAN_WARN)
		return (0);

	va_start(ap, msg);
	err = bunyan_vlog(BUNYAN_WARN, msg, &ap);
	va_end(ap);

	return (err);
}

int
bunyan_error(char *msg, ...)
{
	va_list ap;
	int err;

	if (_bunyan_level > BUNYAN_ERROR)
		return (0);

	va_start(ap, msg);
	err = bunyan_vlog(BUNYAN_ERROR, msg, &ap);
	va_end(ap);

	return (err);
}

int
bunyan_fatal(char *msg, ...)
{
	va_list ap;
	int err;

	if (_bunyan_level > BUNYAN_FATAL)
		return (0);

	va_start(ap, msg);
	err = bunyan_vlog(BUNYAN_FATAL, msg, &ap);
	va_end(ap);

	return (err);
}

int
bunyan_level(int level)
{
	int old_level = _bunyan_level;

	_bunyan_level = level;

	return (old_level);
}
