CC = gcc
CFLAGS = -c -Wall -g -D TB_DEBUG -D _ALLOC_USE_DBGINFO -D PMEM_TEST -I.
SUBDIRS = examples
OBJS = buddy_alloc.o pmem_buddy.o region_alloc.o dstream.o iparam.o

all: $(OBJS)
	for dir in $(SUBDIRS); do \
		make -C $$dir all; \
	done


buddy_alloc.o: buddy_alloc.c
	$(CC) $(CFLAGS) $^

pmem_buddy.o: pmem_buddy.c
	$(CC) $(CFLAGS) $^

region_alloc.o: region_alloc.c
	$(CC) $(CFLAGS) $^

dstream.o: dstream.c
	$(CC) $(CFLAGS) $^

iparam.o: iparam.c
	$(CC) $(CFLAGS) $^

clean:
	rm *.o
	for dir in $(SUBDIRS); do \
		make -C $$dir clean; \
	done
