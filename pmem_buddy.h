#include <stddef.h>
#include <stdio.h>

#include "buddy_alloc.h"

extern pbuddy_alloc_t *PBUDDY_ALLOC;

pbuddy_alloc_t *pbuddy_alloc_init(const char *dir, void *base_ptr, uint64_t max_size, uint64_t size);
int pbuddy_alloc_destroy();
void *pbuddy_malloc(size_t size);
void pbuddy_free(void *ptr, size_t size);

size_t get_pbuddy_alloc_size(size_t size);