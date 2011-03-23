/* Copyright 2011 Joyent, Inc. */
#ifndef CAPI_H_
#define	CAPI_H_

#include <curl/curl.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Holder for CAPI connection information.
 *
 * Every time CAPI is invoked a new CURL handle is setup/destroyed.
 */
typedef struct capi_handle {
	char *url;
	char *basic_auth_cred;
	unsigned int connect_timeout;
	unsigned int retries;
	unsigned int retry_sleep;
	unsigned int timeout;
} capi_handle_t;

/**
 * Creates a CAPI handle
 *
 * Caller needs to read the params in out of config.
 *
 * @param url
 * @param user
 * @param pass
 * @return capi_handle_t
 */
extern capi_handle_t *capi_handle_create(const char *url, const char *user,
				const char*pass);

/**
 * Frees up memory associated to CAPI handle.
 *
 * @param handle
 */
extern void capi_handle_destroy(capi_handle_t *handle);

/**
 * Wrapper over an HTTP call to CAPI
 *
 * This method checks the fingerprint'd SSH key in CAPI, and determines
 * whether or not the user is allowed to use that SSH key.  CAPI returns
 * 201 on success, 403/409 on failure, so we don't have to do anything silly
 * like parse JSON in C.  This method will redrive failed calls to CAPI using
 * the params set up in the handle.
 *
 * @param handle
 * @param uuid (owner_uuid -> customer-uuid in CAPI)
 * @param ssh_fp the MD5 fingerprint of an SSH key
 * @param user the current unix user trying to log in
 * @return boolean
 */
extern boolean_t capi_is_allowed(capi_handle_t *handle, const char *uuid,
			const char *ssh_fp, const char *user);

#ifdef __cplusplus
}
#endif

#endif /* CAPI_H_ */
