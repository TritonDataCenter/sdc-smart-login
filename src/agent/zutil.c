/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2014, Joyent, Inc.
 */

#include <errno.h>
#include <libzonecfg.h>
#include <pthread.h>
#include <search.h>
#include <string.h>
#include <zdoor.h>
#include <zone.h>

#include "bunyan.h"
#include "util.h"
#include "zutil.h"

extern void * zonecfg_notify_bind(int(*func)(const char *zonename,
					zoneid_t zid,
					const char *newstate,
					const char *oldstate,
					hrtime_t when,
					void *p),
    void *p);

extern void zonecfg_notify_unbind(void *handle);


static char *g_zdoor_service_name = NULL;
static zdoor_callback g_zdoor_callback = NULL;
static zdoor_handle_t g_zdoor_handle = 0;
static pthread_mutex_t g_zdoor_lock = PTHREAD_MUTEX_INITIALIZER;
static void *g_zdoor_tree = NULL;
static void *g_zonecfg_handle = NULL;


static int
_tsearch_compare(const void *pa, const void *pb) {
	const char *p1 = (const char *)pa;
	const char *p2 = (const char *)pb;

	if (p1 == NULL && p2 == NULL)
		return (0);
	if (p1 != NULL && p2 == NULL)
		return (1);
	if (p1 == NULL && p2 != NULL)
		return (-1);

	return (strcmp(p1, p2));
}


static int
zone_monitor(const char *zonename, zoneid_t zid, const char *newstate,
		const char *oldstate, hrtime_t when, void *p)
{
	if (strcmp("running", newstate) == 0) {
		if (strcmp("ready", oldstate) == 0) {
			bunyan_debug("zone_monitor: zone running",
			    BUNYAN_STRING, "zone", zonename,
			    BUNYAN_NONE);
			(void) open_zdoor(zonename);
		}
	} else if (strcmp("shutting_down", newstate) == 0) {
		if (strcmp("running", oldstate) == 0) {
			bunyan_debug("zone_monitor: zone stopping",
			    BUNYAN_STRING, "zone", zonename,
			    BUNYAN_NONE);
			(void) close_zdoor(zonename);
		}
	}

	return (0);
}


boolean_t
open_zdoor(const char *zone)
{
	boolean_t success = B_FALSE;
	char *owner = NULL;
	char *entry = NULL;

	if (zone == NULL)
		return (B_FALSE);

	(void) pthread_mutex_lock(&g_zdoor_lock);

	if (tfind(zone, &g_zdoor_tree, _tsearch_compare) != NULL) {
		bunyan_debug("zone already has an open door",
		    BUNYAN_STRING, "zone", zone,
		    BUNYAN_NONE);
		success = B_TRUE;
		goto out;
	}

	owner = get_owner_uuid(zone);
	if (owner != NULL) {
		if (zdoor_open(g_zdoor_handle, zone, g_zdoor_service_name,
				owner, g_zdoor_callback) != ZDOOR_OK) {

			bunyan_error("failed to open door to zone",
			    BUNYAN_STRING, "service_name",
			    g_zdoor_service_name,
			    BUNYAN_STRING, "zone", zone,
			    BUNYAN_NONE);
			goto out;
		} else {
			bunyan_debug("opened door",
			    BUNYAN_STRING, "service_name",
			    g_zdoor_service_name,
			    BUNYAN_STRING, "zone", zone,
			    BUNYAN_NONE);
			entry = xstrdup(zone);
			if (entry == NULL) {
				zdoor_close(g_zdoor_handle, zone,
					    g_zdoor_service_name);
				xfree(owner);
				goto out;
			}
			(void) tsearch(entry, &g_zdoor_tree, _tsearch_compare);
			success = B_TRUE;
		}
	}
out:
	(void) pthread_mutex_unlock(&g_zdoor_lock);
	return (success);
}


boolean_t
close_zdoor(const char *zone)
{
	boolean_t success = B_FALSE;
	char **entry = NULL;
	char *owner = NULL;

	if (zone == NULL)
		return (B_FALSE);

	(void) pthread_mutex_lock(&g_zdoor_lock);
	entry = (char **)tfind(zone, &g_zdoor_tree, _tsearch_compare);
	if (entry != NULL && *entry != NULL) {
		owner = zdoor_close(g_zdoor_handle, zone, g_zdoor_service_name);
		if (owner != NULL)
			xfree(owner);

		xfree(*entry);
		(void) tdelete(zone, &g_zdoor_tree, _tsearch_compare);
		success = B_TRUE;
	}

	(void) pthread_mutex_unlock(&g_zdoor_lock);
	return (success);
}


boolean_t
register_zmon(const char *service_name, zdoor_callback callback)
{
	if (service_name == NULL || callback == NULL) {
		bunyan_error("register_zmon: NULL arguments", BUNYAN_NONE);
		return (B_FALSE);
	}
	g_zdoor_service_name = xstrdup(service_name);
	if (g_zdoor_service_name == NULL)
		return (B_FALSE);

	g_zdoor_callback = callback;

	g_zdoor_handle = zdoor_handle_init();
	if (g_zdoor_handle == NULL) {
		bunyan_error("zdoor_handle_init failed", BUNYAN_NONE);
		xfree(g_zdoor_service_name);
		return (B_FALSE);
	}

	g_zonecfg_handle = zonecfg_notify_bind(zone_monitor, NULL);
	return (B_TRUE);
}


void
unregister_zmon()
{
	zonecfg_notify_unbind(g_zonecfg_handle);
	zdoor_handle_destroy(g_zdoor_handle);
	xfree(g_zdoor_service_name);
}



char **
list_all_zones()
{
	int i, j = 0;
	char buf[ZONENAME_MAX] = {0};
	char **zones = NULL;
	uint_t nzones, save = 0;
	zoneid_t *zids = NULL;

again:
	(void) zone_list(NULL, &nzones);
	save = nzones;

	zids = (zoneid_t *)xcalloc(nzones, sizeof (zoneid_t));
	if (zids == NULL)
		return (NULL);

	zones = (char **)xcalloc(nzones + 1, sizeof (char *));
	if (zones == NULL) {
		xfree(zids);
		return (NULL);
	}

	(void) zone_list(zids, &nzones);
	if (nzones > save) {
		xfree(zids);
		xfree(zones);
		goto again;
	}


	for (i = 0; i < nzones; i++) {
		if (zids[i] == GLOBAL_ZONEID)
			continue;

		if (getzonenamebyid(zids[i], buf, ZONENAME_MAX) < 0) {
			bunyan_error("getzonename failed",
			    BUNYAN_INT32, "zone", zids[i],
			    BUNYAN_STRING, "error", strerror(errno),
			    BUNYAN_NONE);
		}

		zones[j] = xstrdup(buf);
		if (zones[j++] == NULL) {
			xfree(zids);
			xfree(zones);
			return (NULL);
		}
	}

	xfree(zids);
	return (zones);
}


char *
get_owner_uuid(const char *zone)
{
	char *owner = NULL;
	int err = 0;
	struct zone_attrtab attrtab = {};
	zone_dochandle_t handle = 0;

	handle = zonecfg_init_handle();

	if ((err = zonecfg_get_handle(zone, handle)) != Z_OK) {
		bunyan_error("unable to init zonecfg handle",
		    BUNYAN_STRING, "error", zonecfg_strerror(errno),
		    BUNYAN_NONE);
		goto out;
	}

	if ((err = zonecfg_setattrent(handle)) != Z_OK) {
		bunyan_error("unable to zonecfg_setattrent",
		    BUNYAN_STRING, "error", zonecfg_strerror(errno),
		    BUNYAN_NONE);
		goto out;
	}

	while ((err = zonecfg_getattrent(handle, &attrtab)) == Z_OK) {
		if (strcasecmp("owner-uuid", attrtab.zone_attr_name) != 0)
			continue;

		if (strcasecmp("string", attrtab.zone_attr_type) != 0) {
			bunyan_error("zone_attr wrong type (not string)",
			     BUNYAN_STRING, "attr_name",
			     attrtab.zone_attr_name,
			     BUNYAN_STRING, "attr_type",
			     attrtab.zone_attr_type,
			     BUNYAN_NONE);
			continue;
		}

		owner = xstrdup(attrtab.zone_attr_value);
		break;
	}
out:
	(void) zonecfg_endattrent(handle);
	(void) zonecfg_fini_handle(handle);

	return (owner);
}
