CC = gcc
CFLAGS = -Wall -g -I..
PROGS = example

all: $(PROGS)

example: example.c ../pmem_buddy.o ../buddy_alloc.o
	$(CC) $(CFLAGS) -lpthread -o $@ $^

clean:
	rm $(PROGS)