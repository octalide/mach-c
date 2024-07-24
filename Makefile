CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -g
SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)

all: cmach

cmach: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f build/*.o cmach
