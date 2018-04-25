CC=gcc
CFLAGS=-Iinclude -Ideps/Unity/src
LIBS = -lm
DEPS = bit_bd.h hash_map.h sl_list.h
OBJ = src/data_structures/hash_map.o src/data_structures/sl_list.o
OBJ += src/store/bit_db.o
TEST_OBJ = deps/Unity/src/unity.o

%.o: %.c $(DEPS) $(PROG)
	$(CC) -c -o $@ $< $(CFLAGS)

main: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(PROG) $(LIBS)

test: $(OBJ) $(TEST_OBJ)
	gcc -o $@ $^ $(CFLAGS) $(PROG) $(LIBS)

.PHONY: clean

clean:
	rm -f src/*/*.o src/*.o

