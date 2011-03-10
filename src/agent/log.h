/* Copyright 2011 Joyent, Inc. */
#ifndef LOG_H_
#define LOG_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int g_debug_level;
extern boolean_t g_info_enabled;
extern boolean_t g_error_enabled;

extern void debug(const char *fmt, ...);
extern void debug2(const char *fmt, ...);
extern void info(const char *fmt, ...);
extern void error(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif   /* LOG_H_ */
