/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2014, Joyent, Inc.
 */

#ifndef CONFIG_H_
#define	CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define	CFG_CAPI_URL			"capi-url"
#define	CFG_CAPI_LOGIN			"capi-login"
#define	CFG_CAPI_PW			"capi-pw"
#define	CFG_CAPI_CACHE_SIZE		"capi-cache-size"
#define	CFG_CAPI_CACHE_AGE		"capi-cache-age"
#define	CFG_CAPI_RETRIES		"capi-retry-attempts"
#define	CFG_CAPI_RETRY_SLEEP		"capi-retry-sleep"
#define	CFG_CAPI_RECHECK_DENIES		"capi-recheck-denies"
#define	CFG_CAPI_CONNECT_TIMEOUT	"capi-connect-timeout"
#define	CFG_CAPI_TIMEOUT		"capi-timeout"

/**
 * Reads the value for the given key out of the specified file
 *
 * @param file
 * @param key
 * @return value if existed, NULL otherwise
 */
extern char *read_cfg_key(const char *file, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H_ */
