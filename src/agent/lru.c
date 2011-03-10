/* Copyright 2011 Joyent, Inc. */
#include <assert.h>
#include "log.h"
#include "lru.h"
#include "util.h"

typedef struct lru_entry {
	char *key;
	void *value;
} lru_entry_t;

static lru_entry_t *
lru_entry_create(const char *key, void *value)
{
	lru_entry_t *entry = xmalloc(sizeof(lru_entry_t));
	if (entry != NULL) {
		entry->value = value;
		entry->key = xstrdup(key);
		if (entry->key == NULL)
			xfree(entry);
	}

	return entry;
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
	return value;
}


lru_cache_t *
lru_cache_create(size_t size)
{
	lru_cache_t *lru = NULL;

	if (size == 0)
		return NULL;

	lru = xmalloc(size);
	if (lru == NULL) {
		debug2("lru_cache_create: malloc failure\n");
		return NULL;
	}

	lru->size = size;
	lru->count = 0;
	lru->hash = hash_handle_create(size);
	if (lru->hash == NULL) {
		lru_cache_destroy(lru);
		return NULL;
	}
	lru->list = list_create();
	if (lru->list == NULL) {
		lru_cache_destroy(lru);
		lru = NULL;
	}

	debug2("lru_cache_create: returning %p\n", lru);
	return lru;
}


void
lru_cache_destroy(lru_cache_t *lru)
{
	if (lru == NULL)
		return;

	debug2("lru_cache_destroy: lru=%p\n", lru);
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
		debug2("lru_add: NULL arguments\n");
		return value;
	}
	debug("lru_add: lru=%p, key=%s, value=%p\n", lru, key, value);

	node = (list_node_t *)hash_get(lru->hash, key);
	if (node != NULL) {
		debug2("lru_add: %s already exists\n");
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
			debug("lru_add: at capacity(%d), evicting %s\n",
			      lru->size, entry->key);
			hash_del(lru->hash, entry->key);
			existing_value = lru_entry_destroy(entry);
			list_node_destroy(tmp);
		}
	}
out:
	debug("lru_add: returning: %p\n", existing_value);
	return existing_value;
}


void *
lru_get(lru_cache_t *lru, const char *key)
{
	list_node_t *node = NULL;
	lru_entry_t *entry = NULL;
	void *value = NULL;

	if (lru == NULL || key == NULL) {
		debug2("lru_get: NULL arguments\n");
	}
	debug("lru_get: lru=%p, key=%s\n", lru, key);

	node = (list_node_t *)hash_get(lru->hash, key);
	if (node == NULL) {
		debug2("lru_get: %s not in cache\n", key);
		goto out;
	}

	entry = (lru_entry_t *)node->data;
	assert(entry != NULL);
	value = entry->value;
	assert(value != NULL);

	list_del(lru->list, node);
	list_push(lru->list, node);

out:
	debug("lru_get: %s returning: \n", key, value);
	return value;
}
