/* Copyright 2011 Joyent, Inc. */
#ifndef LIST_H_
#define LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Doubly-linked list node.
 */
typedef struct list_node {
	void *data;
	struct list_node *prev;
	struct list_node *next;
} list_node_t;

/**
 * Handle to a doubly linked list.
 */
typedef struct list_handle {
	list_node_t *head;
	list_node_t *tail;
} list_handle_t;

/**
 * Creates a new list.
 *
 * @return an empty list on success, NULL on error.
 */
list_handle_t *list_create();

/**
 * Destroys a list handle.
 *
 * Does not walk the list and free anny nodes!
 *
 * @param handle
 */
void list_destroy(list_handle_t *handle);

/**
 * Creates a new node to be inserted into a list.
 *
 * @param data (CANNOT BE NULL)
 * @return new node, or NULL on error
 */
list_node_t *list_node_create(void *data);

/**
 * Frees memory associated with a node.
 *
 * @param node
 * @return data that was inside the node.
 */
void *list_node_destroy(list_node_t *node);

/**
 * Pushes a node onto the head of the list.
 *
 * @param handle
 * @param node
 */
void list_push(list_handle_t *handle, list_node_t *node);

/**
 * Removes a node from the tail of the list.
 *
 * @param handle
 * @return the old tail
 */
list_node_t *list_pop(list_handle_t *handle);

/**
 * Fixes up the pointers around node in the list, so that node isn't
 * in the list anymore.
 *
 * @param handle
 * @param node
 */
void list_del(list_handle_t *handle, list_node_t *node);

#ifdef __cplusplus
}
#endif

#endif /* LIST_H_ */
