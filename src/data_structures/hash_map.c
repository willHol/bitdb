#include <stdlib.h>
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
	return sdbm(key) % ((int)pow(2, map->dimension)) - 1;
}

static void
resize(hash_map *map, size_t dim_change)
{
	size_t dim_new = map->dimension + dim_change;
	sl_list *old_values = map->values;
	sl_list *new_values = calloc(pow(2,dim_new), sizeof(*new_values));
	memcpy(new_values, map->values, pow(2,map->dimension) * sizeof(sl_list));
	map->values = new_values;
	map->dimension = dim_new;

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

/*
 * Values are copied
 */
int
hash_map_put(hash_map *map, char *key, off_t *value)
{
	int status;
	off_t *unchanged_value = value;
	key_value *kv = malloc(sizeof(*kv));

	if (find(map, key, &value) == 0)
		return 0;
	value = unchanged_value;

	if ((map->num_elems + 1) > (pow(2,map->dimension)))
		resize(map, 1);

	kv->key = key;
	kv->value = value;

	status = sl_list_push(&(map->values[get_index(map, key)]), kv);
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

