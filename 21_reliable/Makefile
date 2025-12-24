CC = gcc
CFLAGS = -Wall -Werror -lrt -O3
DFLAGS = -g $(CFLAGS)

.PHONY: all
all:
	$(CC) $(CFLAGS) *.c -o reliable

.PHONY: debug
debug:
	$(CC) $(DFLAGS) *.c -o reliable

.PHONY: clean
clean:
	rm -rf reliable