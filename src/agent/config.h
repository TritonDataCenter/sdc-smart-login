/* Copyright 2011 Joyent, Inc. */
#ifndef CONFIG_H_
#define CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

extern char *g_capi_userpass;
extern char *g_capi_ip;
extern int g_capi_retries;
extern int g_capi_retry_sleep;
extern int g_capi_connect_timeout;
extern int g_capi_timeout;
extern boolean_t g_capi_recheck_denies;
extern int g_capi_cache_size;
extern int g_capi_cache_age;


boolean_t read_config();

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H_ */
