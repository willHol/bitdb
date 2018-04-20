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

#include <sys/types.h>

typedef struct {
	size_t dimension; /* The backing array is of size 2^dimension */
	const sl_list *values[]; /* An array of pointers to lists of key-value duples */
	int random_int;
	int num_elems;
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

