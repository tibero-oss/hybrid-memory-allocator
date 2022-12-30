CC = gcc
CFLAGS = -Wall -g -D TB_DEBUG -D _ALLOC_USE_DBGINFO -D PMEM_TEST -I..
PROGS = test1
#PROGS2 = test1

all: $(PROGS)

test1 : test1.c ../pmem_buddy.o ../buddy_alloc.o ../dstream.o ../iparam.o ../region_alloc.o
	$(CC) $(CFLAGS) -lpthread -o $@ $^

clean:
	rm $(PROGS)
