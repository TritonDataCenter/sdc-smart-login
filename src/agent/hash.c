/* Copyright 2011 Joyent, Inc. */
#include <string.h>

#include "hash.h"
#include "list.h"
#include "log.h"
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

	return value;
}


static hash_entry_t *
hash_entry_create(const char *key, void *value)
{
	hash_entry_t *entry = NULL;

	if (key == NULL || value == NULL)
		return NULL;

	entry = xmalloc(sizeof(hash_entry_t));
	if (entry == NULL) {
		debug2("hash_entry_create: xmalloc failed\n");
		return NULL;
	}
	entry->value = value;
	entry->key = xstrdup(key);
	if (entry->key == NULL) {
		debug2("hash_entry_create: xstrdup failed\n");
		hash_entry_free(entry);
		entry = NULL;
	}

	return entry;
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

	return hash_val;
}


static boolean_t
is_prime (size_t candidate)
{
  /* No even number and none less than 10 will be passed here.  */
  size_t divn = 3;
  size_t sq = divn * divn;
  size_t old_sq = sq;

  while (sq < candidate && candidate % divn != 0)
    {
      old_sq = sq;
      ++divn;
      sq += 4 * divn;
      if (sq < old_sq)
        return B_TRUE;
      ++divn;
    }

  return candidate % divn != 0;
}


static size_t
next_prime(size_t seed)
{
	seed |= 1;
	while (!is_prime (seed))
		seed += 2;

	return seed;
}


hash_handle_t *
hash_handle_create(size_t size)
{
	hash_handle_t *handle = NULL;

	if (size == 0)
		return NULL;

	handle = (hash_handle_t *)xmalloc(sizeof(hash_handle_t));
	if (handle != NULL) {
		handle->size = next_prime(size);
		handle->table =
			(list_handle_t **)xcalloc(handle->size + 1,
						  sizeof(list_handle_t));
		if (handle->table == NULL) {
			debug2("Unable to create hash lists\n");
			hash_handle_destroy(handle);
			handle = NULL;
		}
	}

	return handle;
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
		debug2("hash_add: NULL arguments\n");
		return;
	}

	debug2("hash_add: handle=%p, key=%s, value=%p\n", handle, key, value);

	index = _hash(key, handle->size);
	list = handle->table[index];
	if (list == NULL) {
		list = list_create();
		if (list == NULL) {
			debug2("hash_add: unable to create list\n");
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
			debug2("hash_add: unable to create list node\n");
		}
	} else {
		debug2("hash_add: unable to create entry\n");
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
		debug2("hash_get: NULL arguments\n");
		return NULL;
	}

	debug2("hash_get: handle=%p, key=%s\n", handle, key);

	index = _hash(key, handle->size);
	list = handle->table[index];
	if (list == NULL) {
		debug2("hash_get: %s hashes to an empty chain\n", key);
		return NULL;
	}

	node = list->head;
	while (node != NULL) {
		entry = (hash_entry_t *)node->data;
		if (strcmp(key, entry->key) == 0) {
			debug2("hash_get: found our entry: %p\n", entry);
			value = entry->value;
			break;
		}
		node = node->next;
	}

	debug2("hash_get: returning %p\n", value);
	return value;
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
		debug2("hash_del: NULL arguments\n");
		return NULL;
	}

	debug2("hash_del: handle=%p, key=%s\n", handle, key);

	index = _hash(key, handle->size);
	list = handle->table[index];
	if (list == NULL) {
		debug2("hash_del: %s hashes to an empty chain\n");
		return NULL;
	}

	node = list->head;
	while (node != NULL) {
		entry = (hash_entry_t *)node->data;
		if (strcmp(key, entry->key) == 0) {
			debug2("hash_del: found our entry: %p\n", entry);
			list_del(list, node);
			list_node_destroy(node);
			break;
		}
		node = node->next;
	}

	if (entry != NULL)
		value = hash_entry_free(entry);

	debug2("hash_del: returning %p\n", value);
	return value;

}
