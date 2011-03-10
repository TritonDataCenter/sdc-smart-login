/* Copyright 2011 Joyent, Inc. */
#ifndef HASH_H_
#define HASH_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct list_handle;

typedef struct hash_entry {
	char *key;
	void *value;
} hash_entry_t;

/**
 * Handle for a chained hash table
 */
typedef struct hash_handle {
	struct list_handle **table;
	size_t size;
} hash_handle_t;

/**
 * Creates a hash table
 *
 * Note that we always make hash table sizes in prime numbers, so whatever
 * you pass in for size, the hashtable will be the next prime number larger.
 *
 * @param size
 * @return hash_handle_t on success, NULL on error
 */
hash_handle_t *hash_handle_create(size_t size);

/**
 * Destroys a hash table
 *
 * This method will not walk the table...
 *
 * @param handle
 */
void hash_handle_destroy(hash_handle_t *handle);

/**
 * Puts a new entry in the cache.
 *
 * Adds an entry if it doesn't exist.
 *
 * @param handle
 * @param key
 * @param value (can't be NULL)
 */
void hash_add(hash_handle_t *handle, const char *key, void *value);

/**
 * Retrieves an entry from the table, if it exists
 *
 * @param handle
 * @param key
 * @return the associated value if it exists, NULL if it doesn't.
 */
void *hash_get(hash_handle_t *handle, const char *key);

/**
 * Deletes an entry from the table
 *
 * @param handle
 * @param key
 * @return the associated value if key existed, NULL if it didn't.
 */
void *hash_del(hash_handle_t *handle, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* HASH_H_ */
