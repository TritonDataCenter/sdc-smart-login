/* Copyright 2014 Joyent, Inc. */

#include <assert.h>
#include "bunyan.h"
#include "lru.h"
#include "util.h"

typedef struct lru_entry {
	char *key;
	void *value;
} lru_entry_t;

static lru_entry_t *
lru_entry_create(const char *key, void *value)
{
	lru_entry_t *entry = xmalloc(sizeof (lru_entry_t));
	if (entry != NULL) {
		entry->value = value;
		entry->key = xstrdup(key);
		if (entry->key == NULL)
			xfree(entry);
	}

	return (entry);
}


static void *
lru_entry_destroy(lru_entry_t *entry)
{
	void *value = NULL;
	if (entry != NULL) {
		value = entry->value;
		xfree(entry->key);
		xfree(entry);
	}
	return (value);
}


lru_cache_t *
lru_cache_create(size_t size)
{
	lru_cache_t *lru = NULL;

	if (size == 0)
		return (NULL);

	lru = xmalloc(size);
	if (lru == NULL) {
		return (NULL);
	}

	lru->size = size;
	lru->count = 0;
	lru->hash = hash_handle_create(size);
	if (lru->hash == NULL) {
		lru_cache_destroy(lru);
		return (NULL);
	}
	lru->list = list_create();
	if (lru->list == NULL) {
		lru_cache_destroy(lru);
		lru = NULL;
	}

	return (lru);
}


void
lru_cache_destroy(lru_cache_t *lru)
{
	if (lru == NULL)
		return;

	list_destroy(lru->list);
	hash_handle_destroy(lru->hash);
	lru->count = 0;
	lru->size = 0;
	xfree(lru);
}

void *
lru_add(lru_cache_t *lru, const char *key, void *value)
{
	list_node_t *node = NULL;
	list_node_t *tmp = NULL;
	lru_entry_t *entry = NULL;
	void *existing_value = NULL;

	if (lru == NULL || key == NULL || value == NULL) {
		bunyan_debug("lru_add: NULL arguments", BUNYAN_NONE);
		return (value);
	}

	node = (list_node_t *)hash_get(lru->hash, key);
	if (node != NULL) {
		bunyan_trace("lru_add key already exists",
		    BUNYAN_STRING, "key", key,
		    BUNYAN_NONE);
		goto out;
	}

	entry = lru_entry_create(key, value);
	if (entry == NULL)
		goto out;

	node = list_node_create(entry);
	if (node == NULL) {
		lru_entry_destroy(entry);
		goto out;
	}

	hash_add(lru->hash, key, node);
	list_push(lru->list, node);

	if (lru->count++ >= lru->size) {
		tmp = list_pop(lru->list);
		if (tmp != NULL) {
			entry = (lru_entry_t *)tmp->data;
			assert(entry != NULL);
			bunyan_debug("lru_add: at capacity, evicting key",
			    BUNYAN_INT32, "capacity", lru->size,
			    BUNYAN_STRING, "key", key,
			    BUNYAN_NONE);
			hash_del(lru->hash, entry->key);
			existing_value = lru_entry_destroy(entry);
			list_node_destroy(tmp);
		}
	}
out:
	return (existing_value);
}


void *
lru_get(lru_cache_t *lru, const char *key)
{
	list_node_t *node = NULL;
	lru_entry_t *entry = NULL;
	void *value = NULL;

	if (lru == NULL || key == NULL) {
		bunyan_debug("lru_get: NULL arguments", BUNYAN_NONE);
	}

	node = (list_node_t *)hash_get(lru->hash, key);
	if (node == NULL) {
		bunyan_debug("lru_get: key not in cache",
		    BUNYAN_STRING, "key", key,
		    BUNYAN_NONE);
		goto out;
	}

	entry = (lru_entry_t *)node->data;
	assert(entry != NULL);
	value = entry->value;
	assert(value != NULL);

	list_del(lru->list, node);
	list_push(lru->list, node);

out:
	return (value);
}
