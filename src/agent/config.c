/* Copyright 2011 Joyent, Inc. */
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif


static const char *CFG_ENV_VAR = "SMARTLOGIN_CONFIG";
static const char *CFG_CAPI_IP = "capi-ip";
static const char *CFG_CAPI_LOGIN = "capi-login";
static const char *CFG_CAPI_PW = "capi-pw";
static const char *BASIC_AUTH = "%s:%s";
static const char *CFG_CAPI_CACHE_SIZE = "capi-cache-size";
static const char *CFG_CAPI_CACHE_AGE = "capi-cache-age";
static const char *CFG_CAPI_RETRIES = "capi-retry-attempts";
static const char *CFG_CAPI_RETRY_SLEEP = "capi-retry-sleep";
static const char *CFG_CAPI_RECHECK_DENIES = "capi-recheck-denies";
static const char *CFG_CAPI_CONNECT_TIMEOUT = "capi-connect-timeout";
static const char *CFG_CAPI_TIMEOUT = "capi-timeout";

char *g_capi_userpass = NULL;
char *g_capi_ip = NULL;
int g_capi_cache_size = 0;
int g_capi_cache_age = 0;
int g_capi_retries = 3;
int g_capi_retry_sleep = 1;
boolean_t g_capi_recheck_denies = B_TRUE;
int g_capi_connect_timeout = 200;
int g_capi_timeout = 500;

static void
chomp(char *s) {
	while(*s && *s != '\n' && *s != '\r') s++;
	*s = 0;
}

static char *
get_cfg_value(const char *key)
{
	boolean_t next_is_val = B_FALSE;
	char buffer[BUFSIZ] = {0};
	char *cfg_file = NULL;
	char *rest = NULL;
        char *token = NULL;
        char *ptr = NULL;
	char *val = NULL;
	FILE *fp = NULL;

	cfg_file = getenv(CFG_ENV_VAR);
	if (cfg_file == NULL) {
		error("%s env var not set\n", CFG_ENV_VAR);
		exit(1);
	}

	fp = fopen(cfg_file, "r");
	if (fp == NULL) {
		error("Couldn't open %s\n", cfg_file);
		exit(1);
	}

	while (fgets(buffer, sizeof (buffer), fp)) {
		ptr = buffer;
		while((token = strtok_r(ptr, "=", &rest)) != NULL) {
			if (next_is_val) {
				chomp(token);
				val = strdup(token);
				if (val == NULL)
					LOG_OOM();
				return val;
			}

			if (strcasecmp(key, token) == 0)
				next_is_val = B_TRUE;

			ptr = rest;
		}
	}

	fclose(fp);
	return NULL;
}


boolean_t
read_config()
{
	boolean_t success = B_FALSE;
	char *login = NULL;
	char *pw = NULL;
	int buf_len = 0;
	char *cache_size = NULL;
	char *cache_age = NULL;
	char *retries = NULL;
	char *sleep_time = NULL;
	char *recheck_denies = NULL;
	char *connect_timeout = NULL;
	char *timeout = NULL;

	login = get_cfg_value(CFG_CAPI_LOGIN);
	pw = get_cfg_value(CFG_CAPI_PW);
	g_capi_ip = get_cfg_value(CFG_CAPI_IP);
	cache_size = get_cfg_value(CFG_CAPI_CACHE_SIZE);
	cache_age = get_cfg_value(CFG_CAPI_CACHE_AGE);
	retries = get_cfg_value(CFG_CAPI_RETRIES);
	sleep_time = get_cfg_value(CFG_CAPI_RETRY_SLEEP);
	recheck_denies = get_cfg_value(CFG_CAPI_RECHECK_DENIES);
	connect_timeout = get_cfg_value(CFG_CAPI_CONNECT_TIMEOUT);
	timeout = get_cfg_value(CFG_CAPI_TIMEOUT);

	if (login == NULL || pw == NULL || g_capi_ip == NULL ||
	    cache_size == NULL || cache_age == NULL)
		return (success);

	g_capi_cache_size = atoi(cache_size);
	g_capi_cache_age = atoi(cache_age);

	if (retries != NULL) {
		g_capi_retries = atoi(retries);
		debug("config: CAPI retries=%d\n", g_capi_retries);
	}
	if (sleep_time != NULL) {
		g_capi_retry_sleep = atoi(sleep_time);
		debug("config: CAPI retry sleep=%d\n", g_capi_retry_sleep);
	}
	if (recheck_denies != NULL) {
		g_capi_recheck_denies = strcasecmp("yes", recheck_denies) == 0;
		debug("config: CAPI recheck denies=%s\n",
		      g_capi_recheck_denies ? "yes" : "no");
	}
	if (connect_timeout != NULL) {
		g_capi_connect_timeout = atoi(connect_timeout);
		debug("config: CAPI connect timeout %d\n",
		      g_capi_connect_timeout);
	}
	if (timeout != NULL) {
		g_capi_timeout = atoi(timeout);
		debug("config: CAPI timeout %d\n", g_capi_timeout);
	}


	buf_len = snprintf(NULL, 0, BASIC_AUTH, login, pw) + 1;
	g_capi_userpass = (char *)calloc(1, buf_len);
	if (g_capi_userpass == NULL) {
		LOG_OOM();
		goto out;
	}
	snprintf(g_capi_userpass, buf_len, BASIC_AUTH, login, pw);

	success = B_TRUE;

  out:
	if (login != NULL) {
		free(login);
		login = NULL;
	}
	if (pw != NULL) {
		free(pw);
		pw = NULL;
	}
	if (cache_size != NULL) {
		free(cache_size);
		cache_size = NULL;
	}
	if (cache_age != NULL) {
		free(cache_age);
		cache_age = NULL;
	}
	if (retries != NULL) {
		free(retries);
		retries = NULL;
	}
	if (sleep_time != NULL) {
		free(sleep_time);
		sleep_time = NULL;
	}
	if (recheck_denies != NULL) {
		free(recheck_denies);
		recheck_denies = NULL;
	}
	if (connect_timeout != NULL) {
		free(connect_timeout);
		connect_timeout = NULL;
	}
	if (timeout != NULL) {
		free(timeout);
		timeout = NULL;
	}
	
	return (success);
}

#ifdef __cplusplus
}
#endif
