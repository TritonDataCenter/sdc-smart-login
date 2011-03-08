/* Copyright 2011 Joyent, Inc. */
#ifndef LOG_H_
#define LOG_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern boolean_t g_debug_enabled;
extern boolean_t g_info_enabled;
extern boolean_t g_error_enabled;

extern void debug(const char *fmt, ...);
extern void info(const char *fmt, ...);
extern void error(const char *fmt, ...);


#define LOG_OOM() error("Out of Memory at %s:%d\n", __FILE__, __LINE__);

#ifdef __cplusplus
}
#endif

#endif   /* LOG_H_ */
