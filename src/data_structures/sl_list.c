#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "sl_list.h"

static sl_node *
new_node(key_value *kv)
{
	sl_node *node = malloc(sizeof(*node));
	
	/* For the sake of simplicity we will
	 * duplicate the data, to avoid potential hassle
	 * in the future
	 */
	char *key_cpy = malloc(strlen(kv->key));
	off_t *value_cpy = malloc(sizeof(kv->value));
	strcpy(key_cpy, kv->key);
	memcpy(value_cpy, kv->value, sizeof(kv->value));
	kv->key = key_cpy;
	kv->value = value_cpy;
	
	if (node == NULL)
		return NULL;

	node->next = NULL;
	node->kv = kv;

	return node;
}

int
sl_list_init(sl_list *list)
{
	*list = (sl_list) {
		.head = NULL,
		.tail = NULL,
		.num_elems = 0
	};

	return 0;
}

int
sl_list_destroy(sl_list *list)
{
	sl_node *u;

	for (u = list->head; u != NULL; u = u->next) {
		free(u);	
	}
	free(list);
	return 0;
}

int
sl_list_push(sl_list *list, key_value *kv)
{
	sl_node *u = new_node(kv);
	
	if (u == NULL)
		return -1;

	u->next = list->head;
	list->head = u;

	if (list->num_elems == 0)
		list->tail = u;

	list->num_elems++;
	return 0;
}

int
sl_list_pop(sl_list *list, key_value **kv)
{
	if (list->num_elems == 0) {
		*kv = NULL;
		return -1;
	}
	
	*kv = list->head->kv;

	sl_node *old_head = list->head;
	list->head = list->head->next;
	list->num_elems--;
	free(old_head);

	if (list->num_elems == 0)
		list->tail = NULL;

	return 0;
}

int
sl_list_find(sl_list *list, char *key, off_t **value)
{
	sl_node *u;

	if (list->num_elems == 0) {
		*value = NULL;
		return -1;
	}

	for (u = list->head; u != NULL; u = u->next) {
		if (strcmp(u->kv->key, key) == 0) {
			*value = u->kv->value;
			return 0;
		}
	}
	*value = NULL;
	return -1;
}

