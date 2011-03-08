/* Copyright 2011 Joyent, Inc. */
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "cache.h"

#define LOG_OOM() fprintf(stderr, "Out of Memory @%s:%u\n", __FILE__, __LINE__)

static char *
build_key(const char *uuid, int uid, const char *fp)
{
	char *key = NULL;
	int buf_len = NULL;

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

	handle = (slc_handle_t *)calloc(1, sizeof (slc_handle_t));
	if (handle == NULL) {
		LOG_OOM();
		return (NULL);
	}
	handle->slc_max_size = size;
	return (handle);
}

void
slc_handle_destroy(slc_handle_t *handle)
{
	if (handle == NULL)
		return;

	free(handle);
}


slc_entry_t *
slc_entry_create(const char *uuid, const char *fp, int uid, boolean_t allowed)
{
	slc_entry_t *entry = NULL;

	if (uuid == NULL || fp == NULL) {
		fprintf(stderr, "slc_entry_create: NULL arguments\n");
		return (NULL);
	}

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

	return (entry);
}

void
slc_entry_free(slc_entry_t *entry)
{
	if (entry == NULL)
		return;

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
		fprintf(stderr, "slc_add_entry: NULL arguments\n");
		return (B_FALSE);
	}

	HASH_FIND_STR(handle->slc_table, e->slc_hash_key, key);
	if (key != NULL) {
		fprintf(stderr, "T%d: %s already in the cache, not adding:\n",
			pthread_self(), e->slc_hash_key);
		return (B_FALSE);
	}

	key = slc_key_create(e->slc_owner_uuid, e->slc_uid, e->slc_fp, e);
	HASH_ADD_KEYPTR(hh, handle->slc_table, key->key, strlen(key->key), key);

	if (handle->slc_head != NULL) {
		handle->slc_head->slc_prev = e;
		e->slc_next = handle->slc_head;
		handle->slc_head = e;
	} else {
		handle->slc_head = e;
		handle->slc_tail = e;
	}

	if (++handle->slc_current_size > handle->slc_max_size) {
		tmp = handle->slc_tail;
		if (tmp != NULL) {
			slc_del_entry(handle, tmp);
			slc_entry_free(tmp);
		}
	}
	fprintf(stderr, "T%d: slc_add_entry added: %p\n", pthread_self(), e);
	return (B_TRUE);
}

slc_entry_t *
slc_get_entry(slc_handle_t *handle, const char *uuid, const char *fp, int uid)
{
	char *tmp = NULL;
	slc_key_t *key = NULL;
	slc_entry_t *e = NULL;
	if (handle == NULL || uuid == NULL || fp == NULL) {
		fprintf(stderr, "slc_get_entry: NULL arguments\n");
		return (NULL);
	}

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
	fprintf(stderr, "T%d: slc_get_entry got: %p\n", pthread_self(), e);
	return (e);
}

void
slc_del_entry(slc_handle_t *handle, slc_entry_t *e)
{
	slc_key_t *key = NULL;

	if (handle == NULL || e == NULL)
		return;

	HASH_FIND_STR(handle->slc_table, e->slc_hash_key, key);
	if (key == NULL) {
		fprintf(stderr, "T%d: %s was not in cache, unable to delete\n",
			pthread_self(), e->slc_hash_key);
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
	fprintf(stderr, "T%d: slc_del_entry deleted: %p\n", pthread_self(), e);
}
