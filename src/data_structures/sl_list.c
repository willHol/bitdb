#include <stdlib.h>
#include "sl_list.h"

static sl_node *
new_node(key_value *kv)
{
	sl_node *node = malloc(sizeof(*node));
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
		return 0;
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

