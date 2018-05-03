#include <sys/types.h>
#include <stdbool.h>
#include <string.h>
#include "unity.h"
#include "dl_list.h"

extern bool alloc_works;

void
test_init(void)
{
	int result;
	dl_list list;

	result = dl_list_init(&list, 0);
	TEST_ASSERT_NULL(list.head);
	TEST_ASSERT_NULL(list.tail);
	TEST_ASSERT_EQUAL(0, list.num_elems);

	dl_list_destroy(&list);
}

void
test_add_remove(void)
{
	int result;
	char key1[] = "key1";
	char key2[] = "key2";
	char *out;
	dl_list list;
	dl_list_init(&list, sizeof(char *));

	result = dl_list_add(&list, 1, key1);
	TEST_ASSERT_EQUAL(-1, result);
	TEST_ASSERT_EQUAL(0, list.num_elems);

	result = dl_list_add(&list, 0, key1);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(1, list.num_elems);

	result = dl_list_add(&list, 0, key2);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(2, list.num_elems);

	// "key2" <=> "key1"
	result = dl_list_remove(&list, 0, (void **)&out);
       	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(1, list.num_elems);
	TEST_ASSERT_EQUAL_PTR(out, key2);

	// "key1"
	result = dl_list_remove(&list, 0, (void **)&out);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL(0, list.num_elems);
        TEST_ASSERT_EQUAL_PTR(out, key1);

	// Test destroying the list when it's not empty
	result = dl_list_add(&list, 0, key1);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL(1, list.num_elems);

        result = dl_list_add(&list, 0, key2);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL(2, list.num_elems);

	result = dl_list_destroy(&list);	
	TEST_ASSERT_EQUAL(0, result);
}

void
test_get_set(void)
{
	int result;
	char key1[] = "key1";
	char key2[] = "key2";
	char key3[] = "key3";
	char *out;
	dl_list list;
	dl_list_init(&list, sizeof(char *));

	// List has no elements so should fail
	result = dl_list_set(&list, 1, key1);
	TEST_ASSERT_EQUAL(-1, result);

	result = dl_list_add(&list, 0, key1);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL(1, list.num_elems);

        result = dl_list_add(&list, 0, key2);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL(2, list.num_elems);

	// set
	result = dl_list_set(&list, 1, key3);
	TEST_ASSERT_EQUAL(0, result);
	result = dl_list_get(&list, 1, (void **)&out);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_PTR(key3, out);
	
	dl_list_destroy(&list);
}

void
test_enqueue_dequeue(void)
{
	int result;
        char key1[] = "key1";
        char key2[] = "key2";
        char key3[] = "key3";
        char *out;
        dl_list list;
        dl_list_init(&list, sizeof(char *));

	result = dl_list_enqueue(&list, key1);
	TEST_ASSERT_EQUAL(0, result);
	
	result = dl_list_enqueue(&list, key2);
        TEST_ASSERT_EQUAL(0, result);
	
	result = dl_list_enqueue(&list, key3);
        TEST_ASSERT_EQUAL(0, result);

	result = dl_list_dequeue(&list, (void **)&out);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_PTR(key1, out);

	result = dl_list_dequeue(&list, (void **)&out);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL_PTR(key2, out);

	result = dl_list_dequeue(&list, (void **)&out);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL_PTR(key3, out);
}
void
test_push_pop(void)
{
	int result;
        char key1[] = "key1";
        char key2[] = "key2";
        char key3[] = "key3";
        char *out;
        dl_list list;
        dl_list_init(&list, sizeof(char *));

	result = dl_list_push(&list, key1);
        TEST_ASSERT_EQUAL(0, result);

        result = dl_list_push(&list, key2);
        TEST_ASSERT_EQUAL(0, result);

        result = dl_list_push(&list, key3);
        TEST_ASSERT_EQUAL(0, result);

        result = dl_list_pop(&list, (void **)&out);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL_PTR(key3, out);

        result = dl_list_pop(&list, (void **)&out);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL_PTR(key2, out);

        result = dl_list_pop(&list, (void **)&out);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL_PTR(key1, out);
}

int
main(void)
{
	UNITY_BEGIN();
		RUN_TEST(test_init);
		RUN_TEST(test_add_remove);
		RUN_TEST(test_get_set);
		RUN_TEST(test_enqueue_dequeue);
		RUN_TEST(test_push_pop);
	return UNITY_END();
}

