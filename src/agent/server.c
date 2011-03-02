#include <alloca.h>
#include <door.h>
#include <errno.h>
#include <fcntl.h>
#include <libzonecfg.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <zdoor.h>
#include <zone.h>
#include <unistd.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#define LOG_OOM() fprintf(stderr, "Out of Memory @%s:%u\n", __FILE__, __LINE__)

static const char *DOOR_FILE = "/var/tmp/._joyent_smartlogin_new_zone";
static const char *CAPI_URI = "http://%s:8080/customers/%s/ssh_sessions";
static const char *POST_DATA = "fingerprint=%s&name=%s&uid=%d";
static const char *KEY_SVC_NAME = "_joyent_sshd_key_is_authorized";

static zdoor_handle_t g_zdoor_handle = 0;

/* config.c */
extern boolean_t read_config();
extern char *g_capi_userpass;
extern char *g_capi_ip;

/* zone.c */
extern char **list_all_zones();
extern char *get_owner_uuid(const char *);

static size_t
curl_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	/* We don't care about the body, so we just chuck it */
	return size * nmemb;
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

	handle = curl_easy_init();
	if (handle == NULL) {
		LOG_OOM();
		fprintf(stderr, "unable to get CURL handle\n");
		goto out;
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
	res = curl_easy_perform(handle);
	if (res != 0) {
		fprintf(stderr, "curl returned %d:%s\n", res,
		    curl_easy_strerror(res));
	} else {
		curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);
		if (http_code == 201) {
			allowed = B_TRUE;
		} else {
			fprintf(stderr, "CAPI wasn't happy: %ld\n", http_code);
		}
	}

  out:
	curl_easy_cleanup(handle);
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
	int uid = -1;
	char *fp = NULL;

	if (cookie == NULL || argp == NULL || argp_sz == 0) {
		fprintf(stderr, "ERROR: zdoor arguments NULL\n");
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
			fprintf(stderr, "ERROR processing %s\n", rest);
			goto out;
			break;
		}
		ptr = rest;
	}

	printf("T%d: login from zone=%s, customer=%s, user=%s, fp=%s\n",
	    pthread_self(), cookie->zdc_zonename, (char *)cookie->zdc_biscuit,
	    name, fp);
	allowed =
	    user_allowed_in_capi((char *)cookie->zdc_biscuit, name, uid, fp);
	printf("T%d: login allowed?=%s\n", pthread_self(),
	    allowed ? "true" : "false");

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
	uid = -1;
	if (name != NULL) {
		free(name);
		name = NULL;
	}
	if (fp != NULL) {
		free(fp);
		fp = NULL;
	}
	return result;
}


static boolean_t
open_zdoor(const char *zone)
{
	char *owner = NULL;
	if (zone == NULL)
		return (B_FALSE);

	owner = get_owner_uuid(zone);
	if (owner != NULL) {
		if (zdoor_open(g_zdoor_handle, zone, KEY_SVC_NAME, owner,
			_key_is_authorized) != ZDOOR_OK) {
			fprintf(stderr, "Failed to open %s in %s\n",
			    KEY_SVC_NAME, zone);
			return (B_FALSE);
		} else {
			fprintf(stderr, "Opened %s in %s\n",
			    KEY_SVC_NAME, zone);
		}
	}
	return (B_TRUE);
}

static void
_new_zone(void *cookie, char *argp, size_t arg_size, door_desc_t *dp,
	uint_t n_desc)
{
	if (argp != NULL) {
		printf("new zone(%s) created, calling zdoor_open\n", argp);
		open_zdoor(argp);
	}
	door_return(NULL, 0, NULL, 0);
}

int
main(int argc, char **argv)
{
	int i = 0;
	int did = 0;
	int newfd = 0;
	struct stat buf = {};
	char *z = NULL;
	char **zones = NULL;
	void *cookie = NULL;

	curl_global_init(CURL_GLOBAL_ALL);

	if (!read_config()) {
		fprintf(stderr, "Unable to read config file!\n");
		exit(1);
	}

	g_zdoor_handle = zdoor_handle_init();
	if (g_zdoor_handle == NULL) {
		fprintf(stderr, "FATAL: zdoor_handle_init failed\n");
		exit(1);
	}

	zones = list_all_zones();
	if(zones == NULL || zones[0] == NULL) {
		fprintf(stderr, "FATAL: found no zones\n");
		exit(1);
	}
	z = zones[0];
	while(z != NULL) {
		open_zdoor(z);
		z = zones[++i];
	}

	if ((did = door_create(_new_zone, 0, 0)) < 0) {
		perror("FATAL: unable to door_create provisioner door");
		exit(1);
	}
	if (stat(DOOR_FILE, &buf) < 0) {
		if ((newfd = creat(DOOR_FILE, 0444)) < 0) {
			perror("FATAL: unable to creat provisioner door file");
			exit(1);
		}
		(void) close(newfd);
	}
	(void) fdetach(DOOR_FILE);
	if (fattach(did, DOOR_FILE) < 0) {
		perror("FATAL: unable to fattach provisioner door");
		exit(2);
	}

	fprintf(stderr, "smartlogin running...\n");
	pause();

	door_revoke(did);

	i = 0;
	z = zones[0];
	while(z != NULL) {
		fprintf(stderr, "zone: %s\n", z);
		cookie = zdoor_close(g_zdoor_handle, z, KEY_SVC_NAME);
		if (cookie != NULL)
			free(cookie);
		free(z);
		z = zones[++i];
	}
	free(zones);
	zdoor_handle_destroy(g_zdoor_handle);
	curl_global_cleanup();

	return 0;
}
