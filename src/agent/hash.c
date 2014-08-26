/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2014, Joyent, Inc.
 */

#include <string.h>

#include "bunyan.h"
#include "hash.h"
#include "list.h"
#include "util.h"

static void *
hash_entry_free(hash_entry_t *entry)
{
	void *value = NULL;

	if (entry != NULL) {
		xfree(entry->key);
		value = entry->value;
		entry->value = NULL;
	}
	xfree(entry);

	return (value);
}


static hash_entry_t *
hash_entry_create(const char *key, void *value)
{
	hash_entry_t *entry = NULL;

	if (key == NULL || value == NULL)
		return (NULL);

	entry = xmalloc(sizeof (hash_entry_t));
	if (entry == NULL) {
		return (NULL);
	}
	entry->value = value;
	entry->key = xstrdup(key);
	if (entry->key == NULL) {
		hash_entry_free(entry);
		entry = NULL;
	}

	return (entry);
}


static int
_hash(const char *key, size_t size)
{
	int hash_val = 0;
	int i = 0;

	for (i = 0; i < strlen(key); i++) {
		hash_val = 37 * hash_val + key[i];
	}

	hash_val %= size;
	if (hash_val < 0)
		hash_val += size;

	return (hash_val);
}


static boolean_t
is_prime(size_t candidate)
{
	/* No even number and none less than 10 will be passed here.  */
	size_t divn = 3;
	size_t sq = divn * divn;
	size_t old_sq = sq;

	while (sq < candidate && candidate % divn != 0) {
		old_sq = sq;
		++divn;
		sq += 4 * divn;
		if (sq < old_sq)
			return (B_TRUE);
		++divn;
	}

	return (candidate % divn != 0);
}


static size_t
next_prime(size_t seed)
{
	seed |= 1;
	while (!is_prime(seed))
		seed += 2;

	return (seed);
}


hash_handle_t *
hash_handle_create(size_t size)
{
	hash_handle_t *handle = NULL;

	if (size == 0)
		return (NULL);

	handle = (hash_handle_t *)xmalloc(sizeof (hash_handle_t));
	if (handle != NULL) {
		handle->size = next_prime(size);
		handle->table =
			(list_handle_t **)xcalloc(handle->size + 1,
						sizeof (list_handle_t));
		if (handle->table == NULL) {
			hash_handle_destroy(handle);
			handle = NULL;
		}
	}

	return (handle);
}


void
hash_handle_destroy(hash_handle_t *handle)
{
	xfree(handle);
}


void
hash_add(hash_handle_t *handle, const char *key, void *value)
{
	int index = 0;
	hash_entry_t *entry = NULL;
	list_handle_t *list = NULL;
	list_node_t *node = NULL;

	if (handle == NULL || key == NULL || value == NULL) {
		bunyan_debug("hash_add: NULL arguments", BUNYAN_NONE);
		return;
	}

	index = _hash(key, handle->size);
	list = handle->table[index];
	if (list == NULL) {
		list = list_create();
		if (list == NULL) {
			return;
		}
		handle->table[index] = list;
	}

	entry = hash_entry_create(key, value);
	if (entry != NULL) {
		node = list_node_create(entry);
		if (node != NULL) {
			list_push(list, node);
		} else {
			hash_entry_free(entry);
		}
	}
}


void *
hash_get(hash_handle_t *handle, const char *key)
{
	int index = 0;
	hash_entry_t *entry = NULL;
	list_handle_t *list = NULL;
	list_node_t *node = NULL;
	void *value = NULL;

	if (handle == NULL || key == NULL) {
		bunyan_debug("hash_get: NULL arguments", BUNYAN_NONE);
		return (NULL);
	}

	index = _hash(key, handle->size);
	list = handle->table[index];
	if (list == NULL) {
		bunyan_trace("hash_get: key hashes to empty chain",
		    BUNYAN_STRING, "key", key,
		    BUNYAN_NONE);
		return (NULL);
	}

	node = list->head;
	while (node != NULL) {
		entry = (hash_entry_t *)node->data;
		if (strcmp(key, entry->key) == 0) {
			bunyan_trace("hash_get: found our entry",
			    BUNYAN_POINTER, "entry", entry,
			    BUNYAN_NONE);
			value = entry->value;
			break;
		}
		node = node->next;
	}

	return (value);
}


void *
hash_del(hash_handle_t *handle, const char *key)
{
	int index = 0;
	hash_entry_t *entry = NULL;
	list_handle_t *list = NULL;
	list_node_t *node = NULL;
	void *value = NULL;

	if (handle == NULL || key == NULL) {
		bunyan_debug("hash_del: NULL arguments", BUNYAN_NONE);
		return (NULL);
	}

	index = _hash(key, handle->size);
	list = handle->table[index];
	if (list == NULL) {
		bunyan_trace("hash_del: key hashes to empty chain",
		    BUNYAN_STRING, "key", key,
		    BUNYAN_NONE);
		return (NULL);
	}

	node = list->head;
	while (node != NULL) {
		entry = (hash_entry_t *)node->data;
		if (strcmp(key, entry->key) == 0) {
			bunyan_trace("hash_get: found our entry",
			    BUNYAN_POINTER, "entry", entry,
			    BUNYAN_NONE);
			list_del(list, node);
			list_node_destroy(node);
			break;
		}
		node = node->next;
	}

	if (entry != NULL)
		value = hash_entry_free(entry);

	return (value);
}
