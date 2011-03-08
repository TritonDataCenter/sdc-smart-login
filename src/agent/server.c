/* Copyright 2011 Joyent, Inc. */
#include <alloca.h>
#include <door.h>
#include <errno.h>
#include <fcntl.h>
#include <libzonecfg.h>
#include <pthread.h>
#include <search.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zdoor.h>
#include <zone.h>
#include <unistd.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include "cache.h"
#include "config.h"
#include "log.h"

extern void *
zonecfg_notify_bind(int(*func)(const char *zonename, zoneid_t zid,
			const char *newstate, const char *oldstate,
			hrtime_t when, void *p), void *p);


extern void
zonecfg_notify_unbind(void *handle);


static const char *DOOR_FILE = "/var/tmp/._joyent_smartlogin_new_zone";
static const char *CAPI_URI = "http://%s:8080/customers/%s/ssh_sessions";
static const char *POST_DATA = "fingerprint=%s&name=%s&uid=%d";
static const char *KEY_SVC_NAME = "_joyent_sshd_key_is_authorized";

static zdoor_handle_t g_zdoor_handle = 0;
static void *g_zonecfg_handle = NULL;
static void *g_zdoor_tree = NULL;
static pthread_mutex_t g_zdoor_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static slc_handle_t *g_cache_handle = NULL;


/* zone.c */
extern char **list_all_zones();
extern char *get_owner_uuid(const char *);

static long
get_system_us() {
	struct timeval tp = {0};
	gettimeofday(&tp, NULL);
	return tp.tv_usec;
}


static int
_tsearch_compare(const void *pa, const void *pb) {
	const char *p1 = (const char *)pa;
	const char *p2 = (const char *)pb;

	if (p1 == NULL && p2 == NULL) return 0;
	if (p1 != NULL && p2 == NULL) return 1;
	if (p1 == NULL && p2 != NULL) return 11;

	return (strcmp(p1, p2));
}


static size_t
curl_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	/* We don't care about the body, so we just chuck it */
	return (size * nmemb);
}

static boolean_t
user_allowed_in_capi(const char *customer_uuid, const char *name, int uid,
			const char *fp)
{
	boolean_t allowed = B_FALSE;
	char *post_data = NULL;
	char *url = NULL;
	int buf_len = 0;
	CURL *handle = NULL;
	CURLcode res = 0;
	long http_code = 0;
	slc_entry_t *entry = NULL;
	time_t now = 0;
	int attempts = 0;
	long start = 0;
	long end = 0;


	pthread_mutex_lock(&g_cache_lock);

	handle = curl_easy_init();
	if (handle == NULL) {
		LOG_OOM();
		error("unable to get CURL handle\n");
		goto out;
	}

	entry = slc_get_entry(g_cache_handle, customer_uuid, fp, uid);
	if (entry != NULL) {
		allowed = entry->slc_is_allowed;
		now = time(0);
		if ((now - entry->slc_ctime) >= g_capi_cache_age) {
			debug("cache entry expired, checking CAPI\n");
		} else if (!allowed && g_capi_recheck_denies) {
			debug("cache deny, config says to check CAPI again\n");
		} else {
			goto out;
		}
	}

	buf_len = snprintf(NULL, 0, CAPI_URI, g_capi_ip, customer_uuid) + 1;
	url = (char *)alloca(buf_len);
	if (url == NULL) {
		LOG_OOM();
		goto out;
	}
	snprintf(url, buf_len, CAPI_URI, g_capi_ip, customer_uuid);

	buf_len = snprintf(NULL, 0, POST_DATA, fp, name, uid) + 1;
	post_data = (char *)alloca(buf_len);
	if (url == NULL) {
		LOG_OOM();
		goto out;
	}
	snprintf(post_data, buf_len, POST_DATA, fp, name, uid);

	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(handle, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
	curl_easy_setopt(handle, CURLOPT_USERPWD, g_capi_userpass);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_callback);
	curl_easy_setopt(handle, CURLOPT_POSTFIELDS, post_data);
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT,
			 g_capi_connect_timeout);
	curl_easy_setopt(handle, CURLOPT_TIMEOUT, g_capi_timeout);

	do {
		debug("attempting capi call...\n");
		start = get_system_us();
		res = curl_easy_perform(handle);
		end = get_system_us();
		debug("capi call took %ld us. Reachable?=%s\n", end - start,
		      res == 0 ? "yes" : "no");

		if (res == 0)
			break;

		info("CAPI call failed:  %d:%s\n", res,
		     curl_easy_strerror(res));
		if (++attempts < g_capi_retries)
			sleep(g_capi_retry_sleep);
	} while(attempts < g_capi_retries);

	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code == 201) {
		debug("CAPI returned 201, allowing login\n");
		allowed = B_TRUE;
	} else {
		debug("CAPI returned non-201: %ld\n", http_code);
	}

	if (res == 0) {
		if (entry != NULL) {
			debug("cache refresh, deleting entry\n");
			slc_del_entry(g_cache_handle, entry);
			slc_entry_free(entry);
			entry = NULL;
		}

		entry = slc_entry_create(customer_uuid, fp, uid, allowed);
		if (entry != NULL) {
			if (!slc_add_entry(g_cache_handle, entry)) {
				debug("Unable to cache response\n");
				slc_entry_free(entry);
			} else {
				debug("CAPI response cached\n");
			}
		} else {
			debug("slc_entry_create failed\n");
		}
	}

  out:
	pthread_mutex_unlock(&g_cache_lock);
	curl_easy_cleanup(handle);
	return (allowed);
}


static zdoor_result_t *
_key_is_authorized(zdoor_cookie_t *cookie, char *argp, size_t argp_sz)
{
	zdoor_result_t *result = NULL;
	boolean_t allowed = B_FALSE;
	int i = 0;
	int uid = -1;
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

	ptr = argp;
	while ((token = strtok_r(ptr, " ", &rest)) != NULL) {
		switch (i++) {
		case 0:
			name = strdup(token);
			if (name == NULL) {
				LOG_OOM();
				goto out;
			}
			break;
		case 1:
			uid = atoi(token);
			break;
		case 2:
			fp = strdup(token);
			if (fp == NULL) {
				LOG_OOM();
				goto out;
			}
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
	allowed = user_allowed_in_capi(uuid, name, uid, fp);
	debug("allowed?=%s\n", allowed ? "true" : "false");

	result = (zdoor_result_t *)calloc(1, sizeof (zdoor_result_t));
	if (result == NULL) {
		LOG_OOM();
		goto out;
	}
	result->zdr_size = 2;
	result->zdr_data = (char *)calloc(1, result->zdr_size);
	if (result->zdr_data == NULL) {
		LOG_OOM();
		free(result);
		result = NULL;
	}
	sprintf(result->zdr_data, "%d", allowed);
out:
	end = get_system_us();
	info("key_authorized?=%s: uuid=%s, uid=%d, fp=%s, timing(us)=%ld\n",
	     (allowed ? "yes" : "no"), uuid, uid, fp, (end - start));
	uid = -1;

	if (name != NULL) {
		free(name);
		name = NULL;
	}
	if (fp != NULL) {
		free(fp);
		fp = NULL;
	}

	return (result);
}


static boolean_t
open_zdoor(const char *zone)
{
	char *owner = NULL;
	char *entry = NULL;
	if (zone == NULL)
		return (B_FALSE);

	pthread_mutex_lock(&g_zdoor_lock);
	entry = tfind(zone, &g_zdoor_tree, _tsearch_compare);
	pthread_mutex_unlock(&g_zdoor_lock);
	if (entry != NULL) {
		debug("%s already has an open zdoor\n", zone);
		return (B_TRUE);
	}

	owner = get_owner_uuid(zone);
	if (owner != NULL) {
		if (zdoor_open(g_zdoor_handle, zone, KEY_SVC_NAME, owner,
			_key_is_authorized) != ZDOOR_OK) {
			debug("Failed to open %s in %s\n", KEY_SVC_NAME, zone);
			return (B_FALSE);
		} else {
			debug("Opened %s in %s\n", KEY_SVC_NAME, zone);
			pthread_mutex_lock(&g_zdoor_lock);
			tsearch(zone, &g_zdoor_tree, _tsearch_compare);
			pthread_mutex_unlock(&g_zdoor_lock);
		}
	}
	return (B_TRUE);
}


static void
_new_zone(void *cookie, char *argp, size_t arg_size, door_desc_t *dp,
	uint_t n_desc)
{
	if (argp != NULL) {
		info("new_zone(%s) invoked (operator)\n", argp);
		open_zdoor(argp);
	}
	door_return(NULL, 0, NULL, 0);
}


/* Note that libzdoor maintains its own zone monitor, which will restart
   any zdoors we've previously opened if this was a reboot.  We have this
   solely to open zdoors in new zones.  We just let this silently fail */
static int
zone_monitor(const char *zonename, zoneid_t zid, const char *newstate,
		const char *oldstate, hrtime_t when, void *p)
{
	if (strcmp("running", newstate) == 0) {
		if (strcmp("ready", oldstate) == 0) {
			debug("zone %s now running...\n", zonename);
			open_zdoor(zonename);
		}
	}

	return (0);
}



int
main(int argc, char **argv)
{
	int i = 0;
	int c = 0;
	int did = 0;
	int newfd = 0;
	struct stat buf = {};
	char *z = NULL;
	char **zones = NULL;
	void *cookie = NULL;

	opterr = 0;
	while ((c = getopt (argc, argv, "sd:")) != -1) {
		switch(c) {
		case 'd':
			switch(atoi(optarg)) {
			case 0:
				g_debug_enabled = B_FALSE;
				g_info_enabled = B_FALSE;
				g_info_enabled = B_FALSE;
				break;
			case 1:
				g_debug_enabled = B_TRUE;
				break;
			case 2:
				g_debug_enabled = B_TRUE;
				setenv("ZDOOR_TRACE", "3", 1);
				break;
			default:
				fprintf(stderr, "Invalid debug level\n");
				break;
			}
			break;
		case 's':
			g_debug_enabled = B_FALSE;
			g_info_enabled = B_FALSE;
			g_info_enabled = B_FALSE;
			break;
		default:
			fprintf(stderr, "USAGE: %s [OPTION]\n", argv[0]);
			fprintf(stderr, "\t-d=[LEVEL]\t (level= 0|1|2)\n");
			fprintf(stderr, "\t-s\tsilent (no logging)\n");
			break;
		}
	}


	g_zonecfg_handle = zonecfg_notify_bind(zone_monitor, NULL);

	curl_global_init(CURL_GLOBAL_ALL);

	if (!read_config()) {
		error("Unable to read config file!\n");
		exit(1);
	}

	g_cache_handle = slc_handle_init(g_capi_cache_size);
	if (g_cache_handle == NULL) {
		error("slc_handle_init failed\n");
		exit(1);
	}

	g_zdoor_handle = zdoor_handle_init();
	if (g_zdoor_handle == NULL) {
		error("zdoor_handle_init failed\n");
		exit(1);
	}

	zones = list_all_zones();
	z = zones != NULL ? zones[0] : NULL;
	while(z != NULL) {
		open_zdoor(z);
		z = zones[++i];
	}

	if ((did = door_create(_new_zone, 0, 0)) < 0) {
		error("unable to door_create provisioner door: %s\n",
		      strerror(errno));
		exit(1);
	}
	if (stat(DOOR_FILE, &buf) < 0) {
		if ((newfd = creat(DOOR_FILE, 0444)) < 0) {
			error("unable to creat provisioner door file: %s\n",
			      strerror(errno));
			exit(1);
		}
		(void) close(newfd);
	}
	(void) fdetach(DOOR_FILE);
	if (fattach(did, DOOR_FILE) < 0) {
		error("unable to fattach provisioner door: %s\n",
		      strerror(errno));
		exit(2);
	}

	info("smartlogin running...\n");
	pause();

	door_revoke(did);

	i = 0;
	z = zones[0];
	while(z != NULL) {
		cookie = zdoor_close(g_zdoor_handle, z, KEY_SVC_NAME);
		if (cookie != NULL)
			free(cookie);
		free(z);
		z = zones[++i];
	}
	free(zones);
	slc_handle_destroy(g_cache_handle);
	zdoor_handle_destroy(g_zdoor_handle);
	curl_global_cleanup();

	zonecfg_notify_unbind(g_zonecfg_handle);

	return 0;
}
