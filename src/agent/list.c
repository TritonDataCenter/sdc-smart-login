/* Copyright 2014 Joyent, Inc. */

#include <assert.h>
#include "list.h"
#include "bunyan.h"
#include "util.h"

list_handle_t *
list_create()
{
	return (list_handle_t *)xmalloc(sizeof (list_handle_t));
}


void
list_destroy(list_handle_t *handle)
{
	xfree(handle);
}


list_node_t *
list_node_create(void *data)
{
	list_node_t *node = NULL;

	if (data == NULL) {
		bunyan_debug("list_node_create: NULL arguments", BUNYAN_NONE);
		return (NULL);
	}

	node = xmalloc(sizeof (list_node_t));
	if (node != NULL)
		node->data = data;

	return (node);
}


void *
list_node_destroy(list_node_t *node)
{
	void *ptr = NULL;

	if (node == NULL) {
		bunyan_debug("list_node_destroy: NULL arguments",
		    BUNYAN_NONE);
		return (NULL);
	}

	ptr = node->data;
	xfree(node);

	return (ptr);
}


void
list_push(list_handle_t *handle, list_node_t *node)
{
	if (handle == NULL || node == NULL) {
		bunyan_debug("list_push: NULL arguments", BUNYAN_NONE);
		return;
	}

	if (handle->head == NULL) {
		assert(handle->tail == NULL);
		handle->head = node;
		handle->tail = node;
	} else {
		handle->head->prev = node;
		node->next = handle->head;
		handle->head = node;
	}

}


list_node_t *
list_pop(list_handle_t *handle)
{
	list_node_t *node = NULL;

	if (handle == NULL) {
		bunyan_debug("list_pop: NULL arguments", BUNYAN_NONE);
		return (NULL);
	}

	node = handle->tail;
	list_del(handle, handle->tail);

	return (node);
}


void
list_del(list_handle_t *handle, list_node_t *node)
{
	if (handle == NULL || node == NULL) {
		bunyan_debug("list_del_node: NULL arguments", BUNYAN_NONE);
		return;
	}

	if (node->prev != NULL)
		node->prev->next = node->next;
	if (node->next != NULL)
		node->next->prev = node->prev;

	if (node == handle->tail)
		handle->tail = node->prev;
	if (node == handle->head)
		handle->head = node->next;

	node->prev = NULL;
	node->next = NULL;
}
