#include <stdlib.h>
#include "bit_db.h"
	
int
main(void)
{
	bit_db_conn *conn;
	
	bit_db_init(NULL);
	conn = bit_db_connect(NULL);
}
