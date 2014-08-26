/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2014, Joyent, Inc.
 */

#ifndef LRU_H_
#define	LRU_H_

#include "hash.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * An LRU cache is an O(1) cache of a fixed size.
 *
 * It's just a hashtable with a "queue" for choosing what to evict. Time is
 * not used for eviction, so if a caller wants anything to do with time, it
 * needs to be done outside this LRU logic.
 */
typedef struct lru_cache {
	hash_handle_t *hash;
	list_handle_t *list;
	size_t size;
	size_t count;
} lru_cache_t;

/**
 * Creates a new LRU cache of the given size.
 *
 * @param size
 * @return LRU cache on success, NULL on error.
 */
extern lru_cache_t *lru_cache_create(size_t size);

/**
 * Cleans up cache handle memory
 *
 * This method doesn't flush the cache and clean up, so only call this in error
 * cases.
 *
 * @param lru
 */
extern void lru_cache_destroy(lru_cache_t *lru);


/**
 * Adds a new entry to the cache, if it doesn't exist.
 *
 * This method auto-evicts if the cache was at capacity and returns the
 * evicted "value".  Caller needs to do whatever they need with that (like free)
 *
 * @param lru
 * @param key
 * @param value
 * @return the evicted value, if any
 */
extern void *lru_add(lru_cache_t *lru, const char *key, void *value);

/**
 * Retrieves an entry from the cache
 *
 * This method also marks the entry as most recently used.
 *
 * @param lru
 * @param key
 * @return the value associated to key, or NULL if non-existent
 */
extern void *lru_get(lru_cache_t *lru, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* LRU_H_ */
