/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2014, Joyent, Inc.
 */

#ifndef	BUNYAN_H_
#define	BUNYAN_H_

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	BUNYAN_TRACE	10
#define	BUNYAN_DEBUG	20
#define	BUNYAN_INFO	30
#define	BUNYAN_WARN	40
#define	BUNYAN_ERROR	50
#define	BUNYAN_FATAL	60

#define	BUNYAN_NONE	((void *)0)
#define	BUNYAN_POINTER	((int)1)
#define	BUNYAN_STRING	((int)2)
#define	BUNYAN_INT32	((int)3)
#define	BUNYAN_BOOLEAN	((int)4)

extern int bunyan_trace(char *msg, ...) __SENTINEL;
extern int bunyan_debug(char *msg, ...) __SENTINEL;
extern int bunyan_info(char *msg, ...) __SENTINEL;
extern int bunyan_warn(char *msg, ...) __SENTINEL;
extern int bunyan_error(char *msg, ...) __SENTINEL;
extern int bunyan_fatal(char *msg, ...) __SENTINEL;

extern int bunyan_level(int level);

#ifdef __cplusplus
}
#endif

#endif /* BUNYAN_H_ */
