/* Copyright 2011 Joyent, Inc. */
#ifndef CACHE_H_
#define CACHE_H_

#include <sys/types.h>

#include "uthash.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct smartlogin_entry {
        char *slc_hash_key;
        char *slc_owner_uuid;
        int slc_uid;
        char *slc_fp;
        boolean_t slc_is_allowed;
        time_t slc_ctime;
        struct smartlogin_entry *slc_prev;
        struct smartlogin_entry *slc_next;
} slc_entry_t;

typedef struct smart_login_cache_hash_key {
        char *key;
        slc_entry_t *entry;
        UT_hash_handle hh;
} slc_key_t;

typedef struct smartlogin_cache_handle {
        slc_key_t *slc_table;
        slc_entry_t *slc_head;
        slc_entry_t *slc_tail;
        int slc_current_size;
        int slc_max_size;
} slc_handle_t;

slc_handle_t *slc_handle_init(int size);
void slc_handle_destroy(slc_handle_t *handle);

slc_entry_t *slc_entry_create(const char *uuid, const char *fp, int uid,
boolean_t allowed);
void slc_entry_free(slc_entry_t *entry);

boolean_t slc_add_entry(slc_handle_t *handle, slc_entry_t *e);
slc_entry_t *slc_get_entry(slc_handle_t *handle, const char *uuid,
const char *fp, int uid);
void slc_del_entry(slc_handle_t *handle, slc_entry_t *e);

#ifdef __cplusplus
}
#endif

#endif /* CACHE_H_ */
