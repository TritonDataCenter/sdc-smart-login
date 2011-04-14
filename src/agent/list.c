/* Copyright 2011 Joyent, Inc. */
#include <assert.h>
#include "list.h"
#include "log.h"
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
		debug2("list_node_create: NULL arguments\n");
		return (NULL);
	}

	node = xmalloc(sizeof (list_node_t));
	if (node != NULL)
		node->data = data;

	debug2("list_node_create: returning %p\n", node);
	return (node);
}


void *
list_node_destroy(list_node_t *node)
{
	void *ptr = NULL;

	if (node == NULL) {
		debug2("list_node_destroy: NULL arguments\n");
		return (NULL);
	}

	debug2("list_node_destroy: node=%p\n", node);
	ptr = node->data;
	xfree(node);

	debug2("list_node_destroy: returning %p\n", ptr);
	return (ptr);
}


void
list_push(list_handle_t *handle, list_node_t *node)
{
	if (handle == NULL || node == NULL) {
		debug2("list_push: NULL arguments");
		return;
	}

	debug2("list_push: handle=%p, node=%p\n", handle, node);

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
		debug2("list_pop: NULL arguments\n");
		return (NULL);
	}

	debug2("list_pop: handle=%p\n", handle);

	node = handle->tail;
	list_del(handle, handle->tail);

	debug2("list_pop: returning node=%p\n", node);
	return (node);
}


void
list_del(list_handle_t *handle, list_node_t *node)
{
	if (handle == NULL || node == NULL) {
		debug2("list_del_node: NULL arguments\n");
		return;
	}

	debug2("list_del_node: handle=%p, node=%p\n", handle, node);

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
