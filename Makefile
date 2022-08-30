CC = gcc
CFLAGS = -c -Wall -g -I.
SUBDIRS = examples
OBJS = buddy_alloc.o pmem_buddy.o

all: $(OBJS)
	for dir in $(SUBDIRS); do \
		make -C $$dir all; \
	done


buddy_alloc.o: buddy_alloc.c
	$(CC) $(CFLAGS) $^

pmem_buddy.o: pmem_buddy.c
	$(CC) $(CFLAGS) $^

clean:
	rm *.o
	for dir in $(SUBDIRS); do \
		make -C $$dir clean; \
	done