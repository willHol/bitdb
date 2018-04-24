#include <stdlib.h>
#include <stdio.h>
#include "bit_db.h"
	
int
main(void)
{
	char value[3], data_back[4];
	size_t data = 100321123;

	bit_db_init(NULL);
	bit_db_conn *conn = bit_db_connect(NULL);
	

	bit_db_put(conn, "hey", "bro", 4);
	bit_db_put(conn, "data", &data, sizeof(data));
	bit_db_put(conn, "data", "data", 5);
	bit_db_get(conn, "hey", value);
	bit_db_get(conn, "data", data_back);

	printf("%s\n", value);
	printf("%s\n", data_back);
}
