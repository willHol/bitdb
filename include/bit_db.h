typedef struct {
	int fd;
} bit_db_conn;

int
bit_db_init(const char *pathname);

bit_db_conn *
bit_db_connect(const char *pathname);

int
bit_db_put(bit_db_conn *conn, const char *key, void *value, size_t bytes);

