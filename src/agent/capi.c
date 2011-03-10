/* Copyright 2011 Joyent, Inc. */
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include "capi.h"
#include "log.h"
#include "util.h"

static const char *BASIC_AUTH = "%s:%s";
static const char *CAPI_URI = "http://%s:8080/customers/%s/ssh_sessions";
static const char *FORM_DATA = "fingerprint=%s&name=%s";


static char *
get_capi_url(const char *ip, const char *uuid)
{
	char *buf = NULL;
	int len = 0;
	len = snprintf(NULL, 0, CAPI_URI, ip, uuid) + 1;
	buf = xmalloc(len);
	if (buf == NULL) {
		debug2("unable to allocate %d bytes for url\n", len);
		return NULL;
	}
	snprintf(buf, len, CAPI_URI, ip, uuid);
	return buf;
}

static char *
get_capi_form_data(const char *fp, const char *user)
{
	char *buf = NULL;
	int len = 0;

	len = snprintf(NULL, 0, FORM_DATA, fp, user) + 1;
	buf = xmalloc(len);
	if (buf == NULL) {
		debug2("unable to allocate %d bytes for form_data\n", len);
		return NULL;
	}
	snprintf(buf, len, FORM_DATA, fp, user);
	return buf;
}


static size_t
curl_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	return (size * nmemb);
}


static CURL *
get_curl_handle(capi_handle_t *handle, const char *url, const char *form_data)
{
	CURL * curl = NULL;

	curl = curl_easy_init();
	if (curl == NULL)
		return NULL;

	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, handle->connect_timeout);
	curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, -1);
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 0);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_data);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, handle->timeout);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_USERPWD, handle->basic_auth_cred);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);

	return curl;
}


capi_handle_t *
capi_handle_create(const char *ip, const char *user, const char *pass)
{
	capi_handle_t *handle = NULL;
	int len = 0;

	if (ip == NULL || user == NULL || pass == NULL) {
		debug2("capi_handle_create: NULL arguments\n");
		return NULL;
	}

	debug2("capi_handle_create: ip=%s, user=%s, pass=%s\n", ip, user, pass);

	handle= xmalloc(sizeof(capi_handle_t));
	if (handle == NULL)
		return NULL;

	handle->connect_timeout = 1;
	handle->retries = 1;
	handle->retry_sleep = 1;
	handle->timeout = 3;

	handle->ip = xstrdup(ip);
	if (handle->ip == NULL) {
		capi_handle_destroy(handle);
		return NULL;
	}

	len = snprintf(NULL, 0, BASIC_AUTH, user, pass) + 1;
	handle->basic_auth_cred = xmalloc(len);
	if (handle->basic_auth_cred == NULL) {
		capi_handle_destroy(handle);
		return NULL;
	}
	snprintf(handle->basic_auth_cred, len, BASIC_AUTH, user, pass);

	debug2("capi_handle_create: returning %p\n", handle);
	return handle;
}


void
capi_handle_destroy(capi_handle_t *handle)
{
	debug2("capi_handle_destroy: handle=%p\n", handle);
	if (handle != NULL) {
		xfree(handle->ip);
		xfree(handle->basic_auth_cred);
		xfree(handle);
	}
}


boolean_t
capi_is_allowed(capi_handle_t *handle, const char *uuid,
		const char *ssh_fp, const char *user)
{
	boolean_t allowed = B_FALSE;
	char *form_data = NULL;
	char *url = NULL;
	CURL *curl = NULL;
	CURLcode res = 0;
	int attempts = 0;
	long start = 0;
	long end = 0;
	long http_code = 0;

	if (handle == NULL || uuid == NULL || ssh_fp == NULL || user == NULL) {
		debug2("capi_is_allowed: NULL arguments\n");
		return B_FALSE;
	}
	debug("capi_is_allowed: handle=%p, uuid=%s, ssh_fp=%s, user=%s\n",
	      handle, uuid, ssh_fp, user);

	url = get_capi_url(handle->ip, uuid);
	if (url == NULL)
		goto out;
	form_data = get_capi_form_data(ssh_fp, user);
	if (form_data == NULL)
		goto out;

	curl = get_curl_handle(handle, url, form_data);
	if (curl == NULL)
		goto out;

	do {
		debug2("capi_is_allowed: POSTing %s to %s\n", form_data, url);
		start = get_system_us();
		res = curl_easy_perform(curl);
		end = get_system_us();
		debug2("capi_is_allowed: reachable?=%s, timing(us)=%ld\n",
		      (res == 0 ? "yes" : "no"), (end - start));

		if (res == 0)
			break;

		info("CAPI network error:  %d:%s\n", res,
		     curl_easy_strerror(res));

		if (++attempts < handle->retries)
			sleep(handle->retry_sleep);
	} while (attempts < handle->retries);

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	debug2("capi_is_allowed: HTTP response code: %ld\n", http_code);
	allowed = (http_code == 201 ? B_TRUE : B_FALSE);

out:
	xfree(url);
	xfree(form_data);
	if (curl != NULL) {
		curl_easy_cleanup(curl);
		curl = NULL;
	}

	debug("capi_is_allowed: returning %s\n", allowed ? "true" : "false");
	return allowed;
}
