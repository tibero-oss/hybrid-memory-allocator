CC = gcc
#CFLAGS = -Wall -g -I..
CFLAGS = -Wall -g -DTB_DEBUG=1 -D_ALLOC_USE_GDBINFO=1 -I..
PROGS = example
PROGS2 = test1

all: $(PROGS2)

example: example.c ../pmem_buddy.o ../buddy_alloc.o
	$(CC) $(CFLAGS) -lpthread -o $@ $^

test1 : test1.c ../pmem_buddy.o ../buddy_alloc.o ../dstream.o ../iparam.o ../region_alloc.o
	$(CC) $(CFLAGS) -lpthread -o $@ $^

clean:
	rm $(PROGS)