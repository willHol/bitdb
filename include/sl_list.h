/*
 * DESCRIPTION:
 * 	
 * 	A singly linked list struct. Stores a tuple of key-value strings.
 *
 */
#include <sys/types.h>

typedef struct {
	char *kv[2]; /* 0 - key, 1 - value */
} key_value;

typedef struct sl_node {
	struct sl_node *next;
	key_value *kv;
} sl_node;

typedef struct {
	sl_node *head;
	sl_node *tail;
	size_t  num_elems;
} sl_list;

int
sl_list_init(sl_list *list);

int
sl_list_push(sl_list *list, key_value *kv);

int
sl_list_pop(sl_list *list, key_value **kv);

