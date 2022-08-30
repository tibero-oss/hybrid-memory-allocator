#include <stddef.h>
#include <stdio.h>

#include "buddy_alloc.h"

pbuddy_alloc_t *pbuddy_alloc_init(const char *dir, void *base_ptr, uint64_t max_size, uint64_t size);
int pbuddy_alloc_destroy(pbuddy_alloc_t **alloc_ptr);
void *pbuddy_malloc(pbuddy_alloc_t *allocator, uint64_t size);
void pbuddy_free(pbuddy_alloc_t *allocator, void *ptr, uint64_t size);