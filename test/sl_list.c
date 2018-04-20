#include <stdio.h>
#include "sl_list.h"

int
main(void)
{
	sl_list list;
	key_value *popped;
	key_value kv_1, kv_2;
	char *key_1 = "hey", *value_1 = "bro";
	char *key_2 = "what's", *value_2 = "up";

	kv_1.kv[0] = key_1;
	kv_1.kv[1] = value_1;
	kv_2.kv[0] = key_2;
	kv_2.kv[1] = value_2;

	sl_list_init(&list);
	sl_list_push(&list, &kv_1);
	sl_list_push(&list, &kv_2);

	sl_list_pop(&list, &popped);
	printf("popped key: %s\npopped value: %s\n", popped->kv[0], popped->kv[1]);
	sl_list_pop(&list, &popped);
	printf("popped key: %s\npopped value: %s\n", popped->kv[0], popped->kv[1]);
}

