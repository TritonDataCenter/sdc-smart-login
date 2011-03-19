/* Copyright 2011 Joyent, Inc. */
#ifndef ZUTIL_H_
#define	ZUTIL_H_

#include <sys/types.h>
#include <zdoor.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The set of APIs in this header all operate on static variables, which is sort
 * of shitty, but we only have one zdoor/zone for the process, so for now it's
 * good enough.
 */

/**
 * Sets up a zone_monitor that auto opens/closes zdoors
 *
 * Note that libzdoor maintains its own zone monitor, which will restart
 * any zdoors we've previously opened if this was a reboot.  We have this
 * solely to open zdoors in new zones, and to ensure permanently deleted zones
 * don't result in leaked resources.
 *
 * @param name of the zdoor to open
 * @param callback the zdoor callback
 * @return boolean
 */
extern boolean_t register_zmon(const char *service_name,
			zdoor_callback callback);

/**
 * Unbinds the zone monitor
 */
extern void unregister_zmon();

/**
 * Opens zdoor in the given zone that was setup with register_zmon
 *
 * @param zone
 * @return boolean
 */
extern boolean_t open_zdoor(const char *zone);

/**
 * Closes zdoor in the given zone that was setup with register_zmon
 *
 * @param zone
 * @return boolean
 */
extern boolean_t close_zdoor(const char *zone);

/**
 * Reads in zonecfg property for owner_uuid of the given zone
 *
 * @param zone (name)
 * @return allocated copy of the owner_uuid for that zone, or NULL
 */
extern char *get_owner_uuid(const char *zone);

/**
 * Lists all current zones in the system
 *
 * @return zones + 1 NULL entry
 */
extern char **list_all_zones();

#ifdef __cplusplus
}
#endif

#endif /* ZUTIL_H_ */
