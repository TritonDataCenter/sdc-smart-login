#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_OOM() fprintf(stderr, "Out of Memory @%s:%u\n", __FILE__, __LINE__)

static const char *CFG_ENV_VAR = "SMARTLOGIN_CONFIG";
static const char *CFG_CAPI_IP = "capi-ip";
static const char *CFG_CAPI_LOGIN = "capi-login";
static const char *CFG_CAPI_PW = "capi-pw";
static const char *BASIC_AUTH = "%s:%s";
static const char *CFG_CAPI_CACHE_SIZE = "capi-cache-size";
static const char *CFG_CAPI_CACHE_AGE = "capi-cache-age";

char *g_capi_userpass = NULL;
char *g_capi_ip = NULL;
int g_capi_cache_size = 0;
int g_capi_cache_age = 0;

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
		fprintf(stderr, "%s env var not set\n", CFG_ENV_VAR);
		exit(1);
	}

	fp = fopen(cfg_file, "r");
	if (fp == NULL) {
		fprintf(stderr, "Couldn't open %s\n", cfg_file);
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

	login = get_cfg_value(CFG_CAPI_LOGIN);
	pw = get_cfg_value(CFG_CAPI_PW);
	g_capi_ip = get_cfg_value(CFG_CAPI_IP);
	cache_size = get_cfg_value(CFG_CAPI_CACHE_SIZE);
	cache_age = get_cfg_value(CFG_CAPI_CACHE_AGE);

	if (login == NULL || pw == NULL || g_capi_ip == NULL ||
	    cache_size == NULL || cache_age == NULL)
		return (success);

	g_capi_cache_size = atoi(cache_size);
	g_capi_cache_age = atoi(cache_age);

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

	return (success);
}

#ifdef __cplusplus
}
#endif
