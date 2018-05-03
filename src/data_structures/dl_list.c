#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "error_functions.h"
#include "dl_list.h"

static dl_node *
new_node(void *x)
{
	dl_node *node = malloc(sizeof(dl_node));
	if (node == NULL) {
		errMsg("malloc() dl_node");
		return NULL;	
	}

	node->elem = x;
	node->prev = NULL;
	node->next = NULL;

	return node;
}

static dl_node *
get_node(dl_list *list, size_t i)
{
        dl_node *p;

	if (list->num_elems < i)
		return NULL;

        if (i < list->num_elems/2) {
                p = list->dummy->next; 
                for (size_t j = 0; j < i; j++) {
                        p = p->next;
                }
        }
        else {
                p = list->dummy;
                for (size_t j = 0; j < list->num_elems - i; j++) {
                        p = p->prev;
                }
        }
        return p;
}

static int
add_before(dl_list *list, dl_node *w, void *x)
{
	dl_node *u = new_node(x);
	if (u == NULL)
		return -1;

	u->prev = w->prev;
	u->next = w;
	u->next->prev = u;
	u->prev->next = u;
	list->num_elems++;
	return 0;
}

static void *
remove_node(dl_list *list, dl_node *w)
{
	void *x;
	
	x = w->elem;
	w->prev->next = w->next;
	w->next->prev = w->prev;
	free(w);
	
	list->num_elems--;
	return x;
}

int
dl_list_init(dl_list *list, size_t elem_size)
{
	dl_node *dummy = new_node(NULL);
	if (dummy == NULL)
		return -1;

	dummy->prev = dummy;
	dummy->next = dummy;
	
	*list = (dl_list) {
		.num_elems = 0,
		.elem_size = elem_size,
		.dummy = dummy,
		.head = NULL,
		.tail = NULL
	};
	return 0;
}

int
dl_list_get(dl_list *list, size_t i, void **out)
{
	dl_node *u = get_node(list, i);
	if (u == NULL)
		return -1;

	*out = u->elem;
	return 0;
}

int
dl_list_set(dl_list *list, size_t i, void *x)
{
	dl_node *u = get_node(list, i);
	if (u == NULL)
		return -1;

	u->elem = x;
	return 0;	
}

int
dl_list_add(dl_list *list, size_t i, void *x)
{
	dl_node *u = get_node(list, i);
	if (u == NULL)
		return -1;

	return add_before(list, u, x);
}

int
dl_list_remove(dl_list *list, size_t i, void **x)
{
	dl_node *u = get_node(list, i);
	if (u == NULL)
		return -1;
	
	*x = remove_node(list, u);
	
	return (x != NULL) ? 0 : -1;	
}


int
dl_list_push(dl_list *list, void *x)
{
	return dl_list_add(list, 0, x);
}

int
dl_list_pop(dl_list *list, void **x)
{
	return dl_list_remove(list, 0, x);
}

int
dl_list_enqueue(dl_list *list, void *x)
{
	return dl_list_add(list, 0, x);
}

int
dl_list_dequeue(dl_list *list, void **x)
{
	return dl_list_remove(list, list->num_elems - 1, x);
}

int
dl_list_destroy(dl_list *list)
{
	dl_node *u, *next;

	u = list->dummy->next;
	while (u != list->dummy) {
		next = u->next;
		free(u);
		u = next;	
	}
	free(list->dummy);
	return 0;
}

