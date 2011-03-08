/* Copyright 2011 Joyent, Inc. */
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "cache.h"
#include "log.h"

static char *
build_key(const char *uuid, int uid, const char *fp)
{
	char *key = NULL;
	int buf_len = 0;

	buf_len = snprintf(NULL, 0, "%s%d%s", uuid, uid, fp) + 1;

	key = (char *)calloc(1, buf_len);
	if (key == NULL) {
		LOG_OOM();
		return (NULL);
	}

	(void) snprintf(key, buf_len, "%s%d%s", uuid, uid, fp);
	return (key);
}

static slc_key_t *
slc_key_create(const char *uuid, int uid, const char *fp, slc_entry_t *e)
{
	slc_key_t *key = NULL;

	key = (slc_key_t *)calloc(1, sizeof (slc_key_t));
	if (key == NULL) {
		LOG_OOM();
		return (NULL);
	}

	key->key = build_key(uuid, uid, fp);
	if (key->key == NULL) {
		LOG_OOM();
		free(key);
		return (NULL);
	}

	key->entry = e;

	return (key);
}

static void
slc_key_destroy(slc_key_t *key)
{
	if (key == NULL)
		return;

	if (key->key != NULL) {
		free(key->key);
		key->key = NULL;
	}

	key->entry = NULL;
	free(key);
}

slc_handle_t *
slc_handle_init(int size)
{
	slc_handle_t *handle = NULL;
	debug("slc_handle_init: size=%d\n", size);

	handle = (slc_handle_t *)calloc(1, sizeof (slc_handle_t));
	if (handle == NULL) {
		LOG_OOM();
		return (NULL);
	}
	handle->slc_max_size = size;
	debug("slc_handle_init: returning handle=%p\n", handle);
	return (handle);
}

void
slc_handle_destroy(slc_handle_t *handle)
{
	if (handle == NULL)
		return;
	debug("slc_handle_destroy: handle=%p\n", handle);
	free(handle);
}


slc_entry_t *
slc_entry_create(const char *uuid, const char *fp, int uid, boolean_t allowed)
{
	slc_entry_t *entry = NULL;

	if (uuid == NULL || fp == NULL) {
		debug("slc_entry_create: NULL arguments\n");
		return (NULL);
	}
	debug("slc_entry_create: uuid=%s, fp=%s, uid=%d, allowed=%d\n",
	      uuid, fp, uid, allowed);
	entry = (slc_entry_t *)calloc(1, sizeof (slc_entry_t));
	if (entry == NULL) {
		LOG_OOM();
		return (NULL);
	}
	entry->slc_hash_key = build_key(uuid, uid, fp);
	if (entry->slc_hash_key == NULL) {
		LOG_OOM();
		slc_entry_free(entry);
		return (NULL);
	}
	entry->slc_owner_uuid = strdup(uuid);
	if (entry->slc_owner_uuid == NULL) {
		LOG_OOM();
		slc_entry_free(entry);
		return (NULL);
	}

	entry->slc_fp = strdup(fp);
	if (entry->slc_fp == NULL) {
		LOG_OOM();
		slc_entry_free(entry);
		return (NULL);
	}

	entry->slc_uid = uid;
	entry->slc_is_allowed = allowed;
	entry->slc_ctime = time(NULL);
	entry->slc_prev = NULL;
	entry->slc_next = NULL;
	debug("slc_entry_create: returning entry=%p\n", entry);
	return (entry);
}

void
slc_entry_free(slc_entry_t *entry)
{
	if (entry == NULL)
		return;
	debug("slc_entry_free: entry=%p\n", entry);
	if (entry->slc_hash_key != NULL) {
		free(entry->slc_hash_key);
		entry->slc_hash_key = NULL;
	}
	if (entry->slc_owner_uuid != NULL) {
		free(entry->slc_owner_uuid);
		entry->slc_owner_uuid = NULL;
	}
	if (entry->slc_fp != NULL) {
		free(entry->slc_fp);
		entry->slc_fp = NULL;
	}
	entry->slc_uid = -1;
	entry->slc_is_allowed = B_FALSE;
	entry->slc_prev = NULL;
	entry->slc_next = NULL;
	free(entry);
}

boolean_t
slc_add_entry(slc_handle_t *handle, slc_entry_t *e)
{
	slc_key_t *key = NULL;
	slc_entry_t *tmp = NULL;

	if (handle == NULL || e == NULL) {
		debug("slc_add_entry: NULL arguments\n");
		return (B_FALSE);
	}
	debug("slc_add_entry: handle=%p, entry=%p\n", handle, e);

	HASH_FIND_STR(handle->slc_table, e->slc_hash_key, key);
	if (key != NULL) {
		debug("%s already in the cache (skip):\n", e->slc_hash_key);
		return (B_FALSE);
	}

	key = slc_key_create(e->slc_owner_uuid, e->slc_uid, e->slc_fp, e);
	HASH_ADD_KEYPTR(hh, handle->slc_table, key->key, strlen(key->key), key);

	if (handle->slc_head != NULL) {
		debug("list isn't null, making this new head\n");
		handle->slc_head->slc_prev = e;
		e->slc_next = handle->slc_head;
		handle->slc_head = e;
	} else {
		debug("list is null, making this new head and tail\n");
		handle->slc_head = e;
		handle->slc_tail = e;
	}

	if (++handle->slc_current_size > handle->slc_max_size) {
		debug("cache size %d exceeds max(%d)\n",
		      handle->slc_current_size, handle->slc_max_size);
		tmp = handle->slc_tail;
		if (tmp != NULL) {
			debug("evicting %p\n", tmp);
			slc_del_entry(handle, tmp);
			slc_entry_free(tmp);
		} else {
			error("tried to evict, but tail was NULL\n");
		}
	}
	debug("slc_add_entry added: %p\n", e);
	return (B_TRUE);
}

slc_entry_t *
slc_get_entry(slc_handle_t *handle, const char *uuid, const char *fp, int uid)
{
	char *tmp = NULL;
	slc_key_t *key = NULL;
	slc_entry_t *e = NULL;

	if (handle == NULL || uuid == NULL || fp == NULL) {
		debug("slc_get_entry: NULL arguments\n");
		return (NULL);
	}
	debug("slc_get_entry: handle=%p, uuid=%s, fp=%s, uid=%d\n",
	      handle, uuid, fp, uid);

	tmp = build_key(uuid, uid, fp);
	if (tmp == NULL)
		return (NULL);

	HASH_FIND_STR(handle->slc_table, tmp, key);
	free(tmp);
	if (key != NULL) {
		e = key->entry;
		if (e->slc_prev)
			e->slc_prev->slc_next = e->slc_next;
		if (e->slc_next)
			e->slc_next->slc_prev = e->slc_prev;
		if (handle->slc_head)
			handle->slc_head->slc_prev = e;
		e->slc_next = handle->slc_head;
		e->slc_prev = NULL;
		handle->slc_head = e;
	}
	debug("slc_get_entry got: %p\n", e);
	return (e);
}

void
slc_del_entry(slc_handle_t *handle, slc_entry_t *e)
{
	slc_key_t *key = NULL;

	if (handle == NULL || e == NULL)
		return;

	debug("slc_del_entry: handle=%p, entry=%p\n", handle, e);

	HASH_FIND_STR(handle->slc_table, e->slc_hash_key, key);
	if (key == NULL) {
		debug("%s was not in cache\n", e->slc_hash_key);
		return;
	}

	HASH_DEL(handle->slc_table, key);
	slc_key_destroy(key);
	handle->slc_current_size--;
	if (e->slc_prev) {
		e->slc_prev->slc_next = e->slc_next;
	}
	if (e->slc_next) {
		e->slc_next->slc_prev = e->slc_prev;
	}
	if (handle->slc_tail == e) {
		handle->slc_tail = e->slc_prev;
	}
	if (handle->slc_head == e) {
		handle->slc_head = e->slc_next;
	}
	debug("slc_del_entry deleted: %p\n", e);
}
