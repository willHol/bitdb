#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include "hash_map.h"

static size_t
sdbm(char str[])
{
	size_t hash = 0;
	int c;

	while (c = *str++)
		hash = c + (hash << 6) + (hash << 16) - hash;
	
	return hash;
}

static size_t
get_index(hash_map *map, char *key)
{
	return sdbm(key) % ((int)pow(2, map->dimension));
}

static void
resize(hash_map *map, size_t dim_change)
{
	size_t dim_new = map->dimension + dim_change;
	sl_list *old_values = map->values;
	sl_list *new_values = calloc(pow(2,dim_new), sizeof(sl_list));
	sl_list list;
	sl_node *prev_node, *node;
	
	map->values = new_values;
	map->dimension = dim_new;
	map->num_elems = 0;
	for (size_t i = 0; i < pow(2,map->dimension - dim_change); i++) {
		list = old_values[i];
		for (node = list.head; node != NULL; node = node->next) {
			hash_map_put(map, node->kv->key, node->kv->value);
		}
		// TODO: Maybe optimise this, because it's iterating twice
		sl_list_destroy(&list);
	}
	free(old_values);
}

static int
find(hash_map *map, char *key, off_t **value)
{
	return sl_list_find(&(map->values[get_index(map, key)]), key, value);
}

int
hash_map_init(hash_map *map)
{
	map->values = calloc(2, sizeof(sl_list));
	if (map->values == NULL)
		return -1;

	map->dimension = 1;
	map->random_int = rand();
	map->num_elems = 0;

	return 0;
}

int
hash_map_destroy(hash_map *map)
{
	if (map->values == NULL)
		return 0;

	for (size_t i = 0; i < pow(2,map->dimension); i++) {
		sl_list_destroy(&(map->values[i]));
	}
	free(map->values);
}

// TODO: Optimise this, alignment and not persisting unecessary fields
int
hash_map_write(FILE *tb, hash_map *map)
{
	sl_list list;
	sl_node *node;
	char *key;
	off_t *value;

	if (fwrite(map, sizeof(hash_map), 1, tb) == 0)
		return -1;

	for (size_t i = 0; i < pow(2,map->dimension); i++) {
		list = map->values[i];

		if (fwrite(&list, sizeof(sl_list), 1, tb) == 0)
			return -1;
		
		for (node = list.head; node != NULL; node = node->next) {
			/* sl_node and key_value only contain pointers so ignore them */
			key = node->kv->key;
			value = node->kv->value;

			if (fwrite(key, strlen(key) + 1, 1, tb) == 0)
				return -1;

			if (fwrite(value, sizeof(off_t), 1, tb) == 0)
				return -1;
		}
	}
	return 0;
}

/*
 * Values are copied
 */
int
hash_map_put(hash_map *map, char *key, off_t *value)
{
	int status;
	off_t *f_value;
	key_value kv = {.key = key, .value = value};

	if (find(map, key, &f_value) == 0) {
		*f_value = *value;
		return 0;
	}

	if ((map->num_elems + 1) > (pow(2,map->dimension)))
		resize(map, 1);

	status = sl_list_push(&(map->values[get_index(map, key)]), &kv);
	if (status == 0)
		map->num_elems++;
	return status;
}

int
hash_map_get(hash_map *map, char *key, off_t **value)
{
	size_t index = get_index(map, key);
	return sl_list_find(&(map->values[index]), key, value);
}

