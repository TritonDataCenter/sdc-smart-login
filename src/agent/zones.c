/* Copyright 2011 Joyent, Inc. */
#include <errno.h>
#include <libzonecfg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zone.h>

#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif


char **
list_all_zones()
{
	int i, j = 0;
	char buf[ZONENAME_MAX] = {0};
	char **zones = NULL;
	uint_t nzones, save = 0;
	zoneid_t *zids = NULL;

	again:
	(void)zone_list(NULL, &nzones);
	save = nzones;

	zids = (zoneid_t *)calloc(nzones, sizeof(zoneid_t));
	if (zids == NULL) {
		LOG_OOM();
		exit(1);
	}

	zones = (char **)calloc(nzones + 1, sizeof(char *));
	if (zones == NULL) {
		free(zids);
		LOG_OOM();
		exit(1);
	}

	(void) zone_list(zids, &nzones);
	if (nzones > save) {
		free(zids);
		free(zones);
		goto again;
	}


	for (i = 0; i < nzones; i++) {
		if (zids[i] == GLOBAL_ZONEID)
			continue;

		if (getzonenamebyid(zids[i], buf, ZONENAME_MAX) < 0) {
			error("getzonename for %ld failed: %s\n", zids[i],
			      strerror(errno));
		}

		zones[j] = strdup(buf);
		if(zones[j++] == NULL) {
			LOG_OOM();
			exit(1);
		}
	}

	free(zids);
	return zones;
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
		error("Unable to init zonecfg handle\n");
		exit(1);
	}

	if ((err = zonecfg_setattrent(handle)) != Z_OK) {
		error("zonecfg_setattrent: %s\n", zonecfg_strerror(err));
		exit(1);
        }
        while ((err = zonecfg_getattrent(handle, &attrtab)) == Z_OK) {
		if (strcasecmp("owner-uuid", attrtab.zone_attr_name) != 0)
			continue;

		if (strcasecmp("string", attrtab.zone_attr_type) != 0) {
			error("zone_attr %s is of type %s (not string)\n",
			      attrtab.zone_attr_name, attrtab.zone_attr_type);
			continue;
		}

		owner = strdup(attrtab.zone_attr_value);
		if (owner == NULL) {
			LOG_OOM();
			exit(1);
		}
		break;
        }
        (void) zonecfg_endattrent(handle);
	(void) zonecfg_fini_handle(handle);

	return owner;
}

#ifdef __cplusplus
}
#endif
