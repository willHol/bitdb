/*
 * DESCRIPTION:
 *
 * 	Header file for the hash_map data structure.
 *
 * DETAILS:
 *
 * 	- The keys are strings, as are the values.
 * 	- The maximum length of the keys is 128 characters.
 *	- Values can be of an arbitrary length.
 *
 */
#pragma once
#include <stdio.h>
#include <sys/types.h>
#include "sl_list.h"

typedef struct {
	size_t dimension; /* The backing array is of size 2^dimension */
	int random_int;
	int num_elems;
	sl_list *values; /* An array of pointers to lists of key-value duples */
} hash_map;

/*
 * DESCRIPTION:
 * 	
 * 	Initialises a hash_map.
 *
 * PARAMS:
 * 	
 * 	map - A pointer to a hash map struct.
 * 	
 */
int
hash_map_init(hash_map *map);

int
hash_map_destroy(hash_map *map);

int
hash_map_write(FILE *tb, hash_map *map);

int
hash_map_read(FILE *fp, hash_map *map);

int
hash_map_put(hash_map *map, char *key, off_t *value);

int
hash_map_get(hash_map *map, char *key, off_t **value);
