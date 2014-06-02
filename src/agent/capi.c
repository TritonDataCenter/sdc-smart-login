/* Copyright 2014 Joyent, Inc. */

#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include "bunyan.h"
#include "capi.h"
#include "util.h"

static const char *CAPI_URI = "%s/customers/%s/ssh_sessions";
static const char *FORM_DATA = "fingerprint=%s&name=%s";


static char *
get_capi_url(const char *url, const char *uuid)
{
	char *buf = NULL;
	int len = 0;
	len = snprintf(NULL, 0, CAPI_URI, url, uuid) + 1;
	buf = xmalloc(len);
	if (buf == NULL) {
		return (NULL);
	}
	(void) snprintf(buf, len, CAPI_URI, url, uuid);
	return (buf);
}

static char *
get_capi_form_data(const char *fp, const char *user)
{
	char *buf = NULL;
	int len = 0;

	len = snprintf(NULL, 0, FORM_DATA, fp, user) + 1;
	buf = xmalloc(len);
	if (buf == NULL) {
		return (NULL);
	}
	(void) snprintf(buf, len, FORM_DATA, fp, user);
	return (buf);
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
		return (NULL);

	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, handle->connect_timeout);
	curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, -1);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 0);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_data);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, handle->timeout);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

	return (curl);
}


capi_handle_t *
capi_handle_create(const char *url)
{
	capi_handle_t *handle = NULL;

	if (url == NULL) {
		bunyan_debug("capi_handle_create: NULL arguments",
		    BUNYAN_NONE);
		return (NULL);
	}

	handle = xmalloc(sizeof (capi_handle_t));
	if (handle == NULL)
		return (NULL);

	handle->connect_timeout = 1;
	handle->retries = 1;
	handle->retry_sleep = 1;
	handle->timeout = 3;

	handle->url = xstrdup(url);
	if (handle->url == NULL) {
		capi_handle_destroy(handle);
		return (NULL);
	}

	return (handle);
}


void
capi_handle_destroy(capi_handle_t *handle)
{
	if (handle != NULL) {
		xfree(handle->url);
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
	hrtime_t start, end;
	long http_code = 0;

	if (handle == NULL || uuid == NULL || ssh_fp == NULL || user == NULL) {
		bunyan_debug("capi_is_allowed: NULL arguments",
		    BUNYAN_NONE);
		return (B_FALSE);
	}

	bunyan_debug("capi_is_allowed",
	    BUNYAN_POINTER, "handle", handle,
	    BUNYAN_STRING, "uuid", uuid,
	    BUNYAN_STRING, "ssh_fp", ssh_fp,
	    BUNYAN_STRING, "user", user,
	    BUNYAN_NONE);

	url = get_capi_url(handle->url, uuid);
	if (url == NULL)
		goto out;
	form_data = get_capi_form_data(ssh_fp, user);
	if (form_data == NULL)
		goto out;

	curl = get_curl_handle(handle, url, form_data);
	if (curl == NULL)
		goto out;

	do {
		bunyan_trace("capi_is_allowed: POSTing",
		    BUNYAN_STRING, "form_data", form_data,
		    BUNYAN_STRING, "url", url,
		    BUNYAN_NONE);

		start = gethrtime();
		res = curl_easy_perform(curl);
		end = gethrtime();

		bunyan_trace("capi_is_allowed request performed",
		    BUNYAN_STRING, "reachable?", (res == 0 ? "yes" : "no"),
		    BUNYAN_INT32, "timing_us", HR_USEC(end - start),
		    BUNYAN_NONE);

		if (res == 0)
			break;

		bunyan_info("CAPI network error",
		    BUNYAN_INT32, "res", res,
		    BUNYAN_STRING, "error", curl_easy_strerror(res),
		    BUNYAN_NONE);

		if (++attempts < handle->retries)
			(void) sleep(handle->retry_sleep);
	} while (attempts < handle->retries);

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	allowed = (http_code == 201 ? B_TRUE : B_FALSE);
	bunyan_debug("capi_is_allowed HTTP response",
	    BUNYAN_INT32, "http_code", http_code,
	    BUNYAN_BOOLEAN, "allowed", allowed,
	    BUNYAN_NONE);

out:
	xfree(url);
	xfree(form_data);
	if (curl != NULL) {
		curl_easy_cleanup(curl);
		curl = NULL;
	}

	bunyan_debug("capi_is_allowed return",
	    BUNYAN_BOOLEAN, "allowed", allowed,
	    BUNYAN_NONE);

	return (allowed);
}
