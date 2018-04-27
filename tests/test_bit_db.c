#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "unity.h"
#include "bit_db.h"

#define NAME_LEN 14

extern bool alloc_works;

static void
rand_db_name(char name[])
{
	char numString[3] = "";
	strcpy(name, "bit_db_test");
	sprintf(numString, "%d", rand() % 1000);
	strcat(name, numString);
}

void
test_init(void)
{
	int result;
	char name[NAME_LEN];
	rand_db_name(name);
	
	result = bit_db_init(name);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL(0, access(name, F_OK));

	bit_db_destroy(name);
}

void
test_connect(void)
{
	int result;
	char name[NAME_LEN];
	bit_db_conn conn;
	rand_db_name(name);
	bit_db_init(name);

	result = bit_db_connect(&conn, name);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_NOT_NULL(&conn.map);
	TEST_ASSERT_EQUAL(0, fcntl(conn.fd, F_GETFD));

	bit_db_destroy(name);
	bit_db_destroy_conn(&conn);
}

void
test_put_get(void)
{
	int result;
	char name[NAME_LEN];
	char data[] = "somefuckingdata";
	char *retr_data = malloc(strlen(data) + 1);
	bit_db_conn conn;
	rand_db_name(name);
	bit_db_init(name);
	bit_db_connect(&conn, name);

	result = bit_db_put(&conn, "test", data, strlen(data) + 1);
	TEST_ASSERT_EQUAL(0, result);
	
	result = bit_db_get(&conn, "test", retr_data);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_STRING(data, retr_data);

	bit_db_destroy(name);
	bit_db_destroy_conn(&conn);
}

void
test_get_non_existent_key(void)
{
	int result;
        size_t value;
	char name[NAME_LEN];
        bit_db_conn conn;
        rand_db_name(name);
        bit_db_init(name);
	bit_db_connect(&conn, name);

	result = bit_db_get(&conn, "test", &value);
	TEST_ASSERT_EQUAL(result, EKEYNOTFOUND);	

	bit_db_destroy(name);
        bit_db_destroy_conn(&conn);
}

void
test_wrong_magic_seq(void)
{
	int result;
	char name[NAME_LEN];
        char cmd[NAME_LEN + 6];
	bit_db_conn conn;
        rand_db_name(name);
	bit_db_init(name);

	result = bit_db_connect(&conn, name);
	TEST_ASSERT_EQUAL(-1, result);
	TEST_ASSERT_EQUAL(EMAGICSEQ, errno);
	
	bit_db_destroy_conn(&conn);
}

int
main(void)
{
	srand(time(NULL));

	UNITY_BEGIN();
		RUN_TEST(test_init);
		RUN_TEST(test_connect);
		RUN_TEST(test_put_get);
		RUN_TEST(test_get_non_existent_key);
		//RUN_TEST(test_wrong_magic_seq);	
	return UNITY_END();
}

