#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "hash_map.h"

#define HASH_MAP_INIT() {hash_map map; hash_map_init(&map);}

void
test_init(void)
{
	int result;
	hash_map map;
	
	result = hash_map_init(&map);
	TEST_ASSERT_EQUAL(0, result);
	
	TEST_ASSERT_NOT_NULL(map.values);
	TEST_ASSERT_EQUAL(1, map.dimension);
	TEST_ASSERT_NOT_EQUAL(0, map.random_int);
	TEST_ASSERT_EQUAL(0, map.num_elems);
}

void
test_put_and_get(void)
{
	int result;
	off_t value = 1234;
	off_t *get_value;
	hash_map map;
	hash_map_init(&map);

	result = hash_map_put(&map, "test", &value);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(1, map.num_elems);

	result = hash_map_get(&map, "test", &get_value);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(value, *get_value);
	TEST_ASSERT_NOT_EQUAL(&value, get_value); /* Data is copied */
}

static void
generate_key(char key[], size_t i)
{
	char num[8];
	memset(key, '\0', strlen(key));
	sprintf(num, "%ld", (long)i);
	strcpy(key, "key");
	strcat(key, num); /* "key${i}" */
}

/*
 * Insert many elements and test if both
 * the first and last are still in the map
 */ 
void
test_resize_works(void)
{
	int result;
	off_t first_value = 123456;
	off_t last_value  = 789;
	off_t *value;
	char key[256];
	hash_map map;
	hash_map_init(&map);

#define GENERATE_MAX 128

	hash_map_put(&map, "first", &first_value);
	for (size_t i = 0; i < GENERATE_MAX; i++) {
		generate_key(key, i);
		result = hash_map_put(&map, key, &i);
		TEST_ASSERT_EQUAL(0, result);
	}
	hash_map_put(&map, "last", &last_value);
	
	result = hash_map_get(&map, "first", &value);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(first_value, *value);

	result = hash_map_get(&map, "last", &value);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(last_value, *value);

	/* Also try getting one of the inserted key${i} values */
	size_t index = rand() % GENERATE_MAX;
	generate_key(key, index);
	result = hash_map_get(&map, key, &value);
       	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(index, *value);	
}

int
main(void)
{
	UNITY_BEGIN();
		RUN_TEST(test_init);
		RUN_TEST(test_put_and_get);
		RUN_TEST(test_resize_works);
	return UNITY_END();
}
