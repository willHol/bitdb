#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include "hash_map.h"

int
main(void)
{
	srand(time(NULL));
	
	off_t *value;
	off_t val = 10000000;


	hash_map map;
	hash_map_init(&map);

	hash_map_put(&map, "hey", &val);
	hash_map_put(&map, "mush", &val);
	hash_map_put(&map, "life", &val);

	hash_map_get(&map, "life", &value);
	printf("VALUE: %lld\n", (long long)val);
	hash_map_get(&map, "hey", &value);
	printf("VALUE: %lld\n", (long long)val);
}

