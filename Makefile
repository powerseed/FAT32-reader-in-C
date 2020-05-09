CC = clang
CFLAGS = -Wall -Wpedantic -Wextra -Werror -g
LDFLAGS = -lpthread -lm

all: fat32

fat32: fat32.c
	$(CC) $(CFLAGS) $(LDFLAGS) fat32.c -o fat32

clean:
	rm -f fat32