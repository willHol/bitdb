#include <sys/types.h>
#include <stdbool.h>
#include "unity.h"
#include "sl_list.h"

extern bool alloc_works;

void
test_init(void)
{
	int result;
	sl_list list;

	result = sl_list_init(&list);
	TEST_ASSERT_NULL(list.head);
	TEST_ASSERT_NULL(list.tail);
	TEST_ASSERT_EQUAL(0, list.num_elems);
}

void
test_push_pop(void)
{
	int result;
	char key[] = "key";
	off_t value = 123;
	key_value kv;
	key_value *kv_ptr;
	sl_list list;
	sl_list_init(&list);

	kv = (key_value) {
		.key = key,
		.value = &value
	};

	result = sl_list_push(&list, &kv);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(1, list.num_elems);
	
	result = sl_list_pop(&list, &kv_ptr);
	TEST_ASSERT_EQUAL(0, result);
	/* Keys and values only should be copied */
	TEST_ASSERT_NOT_EQUAL(&kv, kv_ptr);
	TEST_ASSERT_NOT_EQUAL(key, kv_ptr->key);
	TEST_ASSERT_NOT_EQUAL(&value, kv_ptr->value);
	TEST_ASSERT_EQUAL_STRING(kv.key, kv_ptr->key);
	TEST_ASSERT_EQUAL_STRING(kv.value, kv_ptr->value);

	sl_list_destroy_kv(kv_ptr); /* kv struct is orphaned */
	sl_list_destroy(&list);
}

int
main(void)
{
	UNITY_BEGIN();
		RUN_TEST(test_init);
		RUN_TEST(test_push_pop);
	return UNITY_END();
}

