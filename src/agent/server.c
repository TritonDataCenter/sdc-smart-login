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

#include <unistd.h>

#include <curl/curl.h>
#include <curl/types.h>

#include "capi.h"
#include "config.h"
#include "log.h"
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
	time_t ctime;
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
		error("%s is a required config param", CFG_CAPI_URL);
		goto out;
	}

	g_capi_handle = capi_handle_create(url);
	if (g_capi_handle == NULL) {
		error("unable to create CAPI handle\n");
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
	time_t now = 0;

	if (uuid == NULL || user == NULL || fp == NULL) {
		debug("user_allowed_in_capi: NULL arguments\n");
	}

	cache_key = build_cache_key(uuid, user, fp);

	(void) pthread_mutex_lock(&g_cache_lock);
	cache_entry = (cache_entry_t *)lru_get(g_lru_cache, cache_key);
	if (cache_entry != NULL) {
		debug("cache hit: %p\n", cache_key);
		allowed = cache_entry->allowed;
		now = time(0);
		if ((now - cache_entry->ctime) >= g_cache_age) {
			debug("cache entry expired, checking CAPI\n");
		} else if (!allowed && g_recheck_denies) {
			debug("cache deny, config says to check CAPI again\n");
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
		cache_entry->ctime = time(0);
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
	long start = 0;
	long end = 0;

	start = get_system_us();

	if (cookie == NULL || argp == NULL || argp_sz == 0) {
		error("zdoor arguments NULL\n");
		return (NULL);
	}

	debug("_key_is_authorized entered: cookie=%p, argp=%s, argp_sz=%d\n",
		cookie, argp, argp_sz);

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
			error("processng %s\n", rest);
			goto out;
			break;
		}
		ptr = rest;
	}

	uuid = (const char *)cookie->zdc_biscuit;
	debug("login attempt from zone=%s, owner=%s, user=%s, fp=%s\n",
		cookie->zdc_zonename, uuid, name, fp);
	if (strcasecmp(name, "root") == 0 ||
	    strcasecmp(name, "admin") == 0 ||
	    strcasecmp(name, "node") == 0) {
		allowed = user_allowed_in_capi(uuid, name, fp);
	} else {
		allowed = B_FALSE;
	}
	debug("allowed?=%s\n", allowed ? "true" : "false");

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
	end = get_system_us();
	info("key_authorized?=%s: uuid=%s, user=%s, fp=%s, timing(us)=%ld\n",
		(allowed ? "yes" : "no"), uuid, name, fp, (end - start));

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
				g_debug_level = 0;
				break;
			case 1:
				g_debug_level = 1;
				break;
			case 2:
				g_debug_level = 2;
				break;
			case 3:
				g_debug_level = 2;
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
			g_debug_level = 0;
			g_error_enabled = B_FALSE;
			g_info_enabled = B_FALSE;
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
			error("neither -f <file> nor %s were specified\n",
				CFGFILE_ENV_VAR);
			exit(1);
		}
	}

	build_capi_handle_from_config(cfg_file);
	if (g_capi_handle == NULL) {
		error("Unable to create CAPI handle from %s\n", cfg_file);
		exit(1);
	}

	build_lru_cache_from_config(cfg_file);
	if (g_lru_cache == NULL) {
		info("CAPI caching disabled\n");
	}

	if (!register_zmon(KEY_SVC_NAME, _key_is_authorized)) {
		error("FATAL: unable to setup zone monitoring\n");
		exit(1);
	}

	zones = list_all_zones();
	z = zones != NULL ? zones[0] : NULL;
	while (z != NULL) {
		open_zdoor(z);
		z = zones[++i];
	}

	info("smartlogin running...\n");
	(void) pause();

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
