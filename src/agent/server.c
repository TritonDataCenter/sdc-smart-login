/* Copyright 2011 Joyent, Inc. */
/*
 *  Smartlogin is an SMF service, installed with the other
 *  agents on head/compute nodes.  That service is started before the zone
 *  SMF service(s), and listens for any zone events (i.e., start/stop).
 *  If a zone is a "tenant" zone (which to this service means it has an
 *  owner_uuid attributee), it opens a door in that zone for SSH to
 *  contact for SSH key authorization.	It calls a new API on CAPI brock
 *  slapped in for me that checks whether a given ssh key fingerprint
 *  belongs to an account.  The daemon maintains an in-memory LRU cache
 *  (i.e., cache goes away if the service bounces).
 *
 *  For more information: https://hub.joyent.com/wiki/display/dev/SmartLogin
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <unistd.h>

#include <curl/curl.h>
#include <curl/types.h>

#include "bunyan.h"
#include "capi.h"
#include "config.h"
#include "lru.h"
#include "util.h"
#include "zutil.h"

/* Our static variables */
static const char *KEY_SVC_NAME = "_joyent_sshd_key_is_authorized";
static const char *CFGFILE_ENV_VAR = "SMARTLOGIN_CONFIG";

/* Global handles */
static capi_handle_t *g_capi_handle = NULL;
static lru_cache_t *g_lru_cache = NULL;
static boolean_t g_recheck_denies = B_TRUE;
static unsigned int g_cache_age = 600;
static pthread_mutex_t g_cache_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct cache_entry {
	boolean_t allowed;
	hrtime_t ctime;
} cache_entry_t;

static char *
build_cache_key(const char *uuid, const char *user, const char *fp)
{
	char *buf = NULL;
	int len = 0;

	len = snprintf(NULL, 0, "%s|%s|%s", uuid, user, fp) + 1;
	buf = xmalloc(len);
	if (buf == NULL)
		return (NULL);

	(void) snprintf(buf, len, "%s|%s|%s", uuid, user, fp);
	return (buf);
}

static void
build_lru_cache_from_config(const char *file)
{

	char *cache_size = NULL;
	char *cache_age = NULL;
	char *recheck_denies = NULL;

	cache_size = read_cfg_key(file, CFG_CAPI_CACHE_SIZE);
	if (cache_size != NULL) {
		g_lru_cache = lru_cache_create(atoi(cache_size));
	}

	cache_age = read_cfg_key(file, CFG_CAPI_CACHE_AGE);
	if (cache_age != NULL) {
		g_cache_age = atoi(cache_age);
	}

	recheck_denies = read_cfg_key(file, CFG_CAPI_RECHECK_DENIES);
	if (recheck_denies != NULL) {
		g_recheck_denies = strcmp("yes", recheck_denies) == 0;
	}

	xfree(cache_size);
	xfree(cache_age);
	xfree(recheck_denies);
}


static void
build_capi_handle_from_config(const char *file)
{
	/* required fields */
	char *url = NULL;

	/* optional fields */
	char *connect_timeout = NULL;
	char *retries = NULL;
	char *retry_sleep = NULL;
	char *timeout = NULL;

	url = read_cfg_key(file, CFG_CAPI_URL);
	if (url == NULL) {
		bunyan_error("missing required config param",
		    BUNYAN_STRING, "param", CFG_CAPI_URL,
		    BUNYAN_NONE);
		goto out;
	}

	g_capi_handle = capi_handle_create(url);
	if (g_capi_handle == NULL) {
		bunyan_error("unable to create CAPI handle", BUNYAN_NONE);
		goto out;
	}

	connect_timeout = read_cfg_key(file, CFG_CAPI_CONNECT_TIMEOUT);
	if (connect_timeout != NULL)
		g_capi_handle->connect_timeout = atoi(connect_timeout);

	retries = read_cfg_key(file, CFG_CAPI_RETRIES);
	if (retries != NULL)
		g_capi_handle->retries = atoi(retries);

	retry_sleep = read_cfg_key(file, CFG_CAPI_RETRY_SLEEP);
	if (retry_sleep != NULL)
		g_capi_handle->retry_sleep = atoi(retry_sleep);

	timeout = read_cfg_key(file, CFG_CAPI_TIMEOUT);
	if (timeout != NULL)
		g_capi_handle->timeout = atoi(timeout);

out:
	xfree(url);
	xfree(connect_timeout);
	xfree(retries);
	xfree(retry_sleep);
	xfree(timeout);
}


static boolean_t
user_allowed_in_capi(const char *uuid, const char *user, const char *fp)
{
	boolean_t allowed = B_FALSE;
	cache_entry_t *cache_entry = NULL;
	cache_entry_t *existing = NULL;
	char *cache_key = NULL;

	if (uuid == NULL || user == NULL || fp == NULL) {
		bunyan_debug("user_allowed_in_capi: NULL arguments",
		    BUNYAN_NONE);
	}

	cache_key = build_cache_key(uuid, user, fp);

	(void) pthread_mutex_lock(&g_cache_lock);
	cache_entry = (cache_entry_t *)lru_get(g_lru_cache, cache_key);
	if (cache_entry != NULL) {
		int age;
		bunyan_debug("cache hit",
		    BUNYAN_STRING, "cache_key", cache_key,
		    BUNYAN_NONE);
		allowed = cache_entry->allowed;
		age = HR_SEC(gethrtime() - cache_entry->ctime);
		if (age >= g_cache_age) {
			bunyan_debug("cache entry expired, checking CAPI",
			    BUNYAN_INT32, "cache_age", age,
			    BUNYAN_NONE);
		} else if (!allowed && g_recheck_denies) {
			bunyan_debug("cache deny, config says to check "
			    "CAPI again", BUNYAN_NONE);
		} else {
			goto out;
		}
	}
	cache_entry = NULL;
	(void) pthread_mutex_unlock(&g_cache_lock);

	allowed = capi_is_allowed(g_capi_handle, uuid, fp, user);

	(void) pthread_mutex_lock(&g_cache_lock);
	cache_entry = (cache_entry_t *)lru_get(g_lru_cache, cache_key);
	if (cache_entry == NULL) {
		cache_entry = (cache_entry_t *)xmalloc(sizeof (cache_entry_t));
		if (cache_entry == NULL)
			goto out;

		existing = lru_add(g_lru_cache, cache_key, cache_entry);
		xfree(existing);
	}

	if (cache_entry != NULL) {
		cache_entry->ctime = gethrtime();
		cache_entry->allowed = allowed;
	}

out:
	(void) pthread_mutex_unlock(&g_cache_lock);
	xfree(cache_key);
	return (allowed);
}


static zdoor_result_t *
_key_is_authorized(zdoor_cookie_t *cookie, char *argp, size_t argp_sz)
{
	zdoor_result_t *result = NULL;
	boolean_t allowed = B_FALSE;
	int i = 0;
	char *ptr = NULL;
	char *rest = NULL;
	char *token = NULL;
	char *name = NULL;
	char *fp = NULL;
	const char *uuid = NULL;
	hrtime_t start, end;

	start = gethrtime();

	if (cookie == NULL || argp == NULL || argp_sz == 0) {
		bunyan_error("zdoor arguments NULL", BUNYAN_NONE);
		return (NULL);
	}

	ptr = argp;
	while ((token = strtok_r(ptr, " ", &rest)) != NULL) {
		switch (i++) {
		case 0:
			name = xstrdup(token);
			if (name == NULL)
				goto out;
			break;
		case 1:
			/* noop, this is the uid field */
			break;
		case 2:
			fp = xstrdup(token);
			if (fp == NULL)
				goto out;
			break;
		default:
			bunyan_error("processing",
			    BUNYAN_STRING, "remainder", rest,
			    BUNYAN_NONE);
			goto out;
			break;
		}
		ptr = rest;
	}

	uuid = (const char *)cookie->zdc_biscuit;
	bunyan_debug("login attempt",
	    BUNYAN_STRING, "zone", cookie->zdc_zonename,
	    BUNYAN_STRING, "uuid", uuid,
	    BUNYAN_STRING, "user", name,
	    BUNYAN_STRING, "ssh_fp", fp,
	    BUNYAN_NONE);
	if (strcasecmp(name, "root") == 0 ||
	    strcasecmp(name, "admin") == 0 ||
	    strcasecmp(name, "node") == 0) {
		allowed = user_allowed_in_capi(uuid, name, fp);
	} else {
		allowed = B_FALSE;
	}
	bunyan_debug("login response",
	    BUNYAN_BOOLEAN, "allowed", allowed,
	    BUNYAN_NONE);

	result = (zdoor_result_t *)xmalloc(sizeof (zdoor_result_t));
	if (result == NULL)
		goto out;

	result->zdr_size = 2;
	result->zdr_data = (char *)xmalloc(result->zdr_size);
	if (result->zdr_data != NULL) {
		(void) sprintf(result->zdr_data, "%d", allowed);
	} else {
		xfree(result);
		result = NULL;
	}
out:
	end = gethrtime();
	bunyan_info("_key_is_authorized end",
	    BUNYAN_STRING, "authorized", (allowed ? "yes" : "no"),
	    BUNYAN_STRING, "uuid", uuid,
	    BUNYAN_STRING, "user", name,
	    BUNYAN_STRING, "ssh_fp", fp,
	    BUNYAN_INT32, "timing_us", HR_USEC(end - start),
	    BUNYAN_NONE);

	xfree(name);
	xfree(fp);

	return (result);
}



int
main(int argc, char **argv)
{
	int i = 0;
	int c = 0;
	char *cfg_file = NULL;
	char *z = NULL;
	char **zones = NULL;

	opterr = 0;
	while ((c = getopt(argc, argv, "sf:d:")) != -1) {
		switch (c) {
		case 'd':
			switch (atoi(optarg)) {
			case 0:
				bunyan_level(BUNYAN_INFO);
				break;
			case 1:
				bunyan_level(BUNYAN_DEBUG);
				break;
			case 2:
				bunyan_level(BUNYAN_TRACE);
				break;
			case 3:
				bunyan_level(BUNYAN_TRACE);
				(void) setenv("ZDOOR_TRACE", "3", 1);
				break;
			default:
				(void) fprintf(stderr, "Invalid debug level\n");
				break;
			}
			break;
		case 'f':
			cfg_file = xstrdup(optarg);
			break;
		case 's':
			/*
			 * Completely disable logging output:
			 */
			bunyan_level(100);
			break;
		default:
			(void) fprintf(stderr, "USAGE: %s [OPTION]\n", argv[0]);
			(void) fprintf(stderr, "\t-d=[LEVEL]\t (level= 0|1|2)\n");
			(void) fprintf(stderr, "\t-s\tsilent (no logging)\n");
			break;
		}
	}

	curl_global_init(CURL_GLOBAL_ALL);

	if (cfg_file == NULL) {
		cfg_file = getenv(CFGFILE_ENV_VAR);
		if (cfg_file == NULL) {
			bunyan_fatal("no configuration file specified",
			    BUNYAN_NONE);
			exit(1);
		}
	}

	build_capi_handle_from_config(cfg_file);
	if (g_capi_handle == NULL) {
		bunyan_fatal("Unable to create CAPI handle",
		    BUNYAN_STRING, "config_file", cfg_file,
		    BUNYAN_NONE);
		exit(1);
	}

	build_lru_cache_from_config(cfg_file);
	if (g_lru_cache == NULL) {
		bunyan_info("CAPI caching disabled", BUNYAN_NONE);
	}

	if (!register_zmon(KEY_SVC_NAME, _key_is_authorized)) {
		bunyan_fatal("unable to setup zone monitoring", BUNYAN_NONE);
		exit(1);
	}

	zones = list_all_zones();
	z = zones != NULL ? zones[0] : NULL;
	while (z != NULL) {
		open_zdoor(z);
		z = zones[++i];
	}

	bunyan_info("smart-login started", BUNYAN_NONE);
	(void) pause();
	bunyan_info("smart-login shutting down", BUNYAN_NONE);

	i = 0;
	z = zones[0];
	while (z != NULL) {
		close_zdoor(z);
		xfree(z);
		z = zones[++i];
	}
	xfree(zones);
	lru_cache_destroy(g_lru_cache);
	curl_global_cleanup();

	return (0);
}
