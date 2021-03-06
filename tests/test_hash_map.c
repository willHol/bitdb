#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "unity.h"
#include "hash_map.h"

extern bool alloc_works;

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

	hash_map_destroy(&map);
}

void
test_put_and_get(void)
{
	int result;
	off_t value1 = 1234, value2 = 678;
	off_t *get_value;
	hash_map map;
	hash_map_init(&map);

	result = hash_map_put(&map, "test1", &value1);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(1, map.num_elems);

	result = hash_map_put(&map, "test2", &value2);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(2, map.num_elems);

	result = hash_map_get(&map, "test1", &get_value);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(value1, *get_value);
	TEST_ASSERT_NOT_EQUAL(&value1, get_value); /* Data is copied */

	result = hash_map_get(&map, "test2", &get_value);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(value2, *get_value);
	TEST_ASSERT_NOT_EQUAL(&value2, get_value);

	hash_map_destroy(&map);
}

static void
generate_key(char key[], size_t i)
{
	char num[8] = "";
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
	char key[256] = "";
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

	hash_map_destroy(&map);	
}

void
test_keys_overwritten(void)
{
	int result;
	off_t value_1 = 1, value_2 = 2;
	off_t *value;
	hash_map map;
	hash_map_init(&map);

	result = hash_map_put(&map, "key", &value_1);
	TEST_ASSERT_EQUAL(0, result);

	result = hash_map_put(&map, "key", &value_2);
	TEST_ASSERT_EQUAL(0, result);

	result = hash_map_get(&map, "key", &value);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(value_2, *value);

	hash_map_destroy(&map);
}

void
test_get_non_existent_key(void)
{
	int result;
	off_t *value;
	hash_map map;
	hash_map_init(&map);

	result = hash_map_get(&map, "key", &value);
	TEST_ASSERT_EQUAL(-1, result);
	TEST_ASSERT_NULL(value);

	hash_map_destroy(&map);
}

void
test_init_malloc_fail(void)
{
	alloc_works = false;

	int result;
	hash_map map;
	
	result = hash_map_init(&map);
	TEST_ASSERT_EQUAL(-1, result);
	TEST_ASSERT_NULL(map.values);

	alloc_works = true;
	hash_map_destroy(&map);
}

int
main(void)
{
	UNITY_BEGIN();
		/* Basic functionality */
		RUN_TEST(test_init);
		RUN_TEST(test_put_and_get);
		RUN_TEST(test_resize_works);
		RUN_TEST(test_keys_overwritten);
		RUN_TEST(test_get_non_existent_key);
		/* Edge cases */
		RUN_TEST(test_init_malloc_fail);
	return UNITY_END();
}

