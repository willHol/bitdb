CC=gcc
CFLAGS=-Iinclude -Ideps/Unity/src -Ideps/crypto-algorithms -g
LIBS = -lm
DEPS = bit_bd.h hash_map.h sl_list.h dl_list.h error_functions.h
OBJ = src/data_structures/hash_map.o src/data_structures/sl_list.o
OBJ += src/data_structures/dl_list.o
OBJ += src/store/bit_db.o src/util/error_functions.o src/util/inet_sockets.o
OBJ += deps/crypto-algorithms/sha256.o
TEST_OBJ = deps/Unity/src/unity.o tests/breakable_malloc.o
TEST_CFLAGS = -Wl,-wrap,malloc -Wl,-wrap,calloc

%.o: %.c $(DEPS) $(PROG)
	$(CC) -c -o $@ $< $(CFLAGS)

main: $(OBJ) $(PROG)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

test: $(OBJ) $(TEST_OBJ) $(PROG)
	gcc -o $@ $^ $(CFLAGS) $(TEST_CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f src/*/*.o src/*.o

