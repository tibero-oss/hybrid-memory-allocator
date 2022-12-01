/**
 * @file    alloc_efence.h
 * @brief   alloc_alg.h 에서만 사용된다! (다른데서 부르면 안됨!)
 *
 * @author
 * @version $Id$
 */

/* Internal header라 두 번 include하면 버그 */
#ifdef _ALLOC_EFENCE_H
#   error "Cannot include alloc_efence.h twice!!!"
#else
#define _ALLOC_EFENCE_H

#include "tb_common.h"
#include <sys/mman.h>

/*
 * 386 BSD has MAP_ANON instead of MAP_ANONYMOUS.
 */
#if ( !defined(MAP_ANONYMOUS) && defined(MAP_ANON) )
#define MAP_ANONYMOUS   MAP_ANON
#endif

/*
 * Create memory.
 */
static inline void *
get_new_page(size_t size)
{
    void *ptr;
#if !defined(MAP_ANONYMOUS)
    static int fd = -1;
#endif

    if (use_root_allocator && !force_malloc_use) {
#if !defined(MAP_ANONYMOUS)
        if (fd == -1) {
            fd = open("/dev/zero", O_RDWR);
            TB_THR_ASSERT(fd != -1);
        }

        ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
#else /* MAP_ANONYMOUNS */
        ptr = mmap(0, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif /* MAP_ANONYMOUNS */
        if (ptr == (void *) -1)
            ptr = NULL;
    }
    else
        ptr = malloc(size);
    return ptr;
}


static inline void
free_page(void *ptr, size_t size)
{
    if (use_root_allocator && !force_malloc_use)
        munmap(ptr, size);
    else
        free(ptr);
}

#endif /* no _ALLOC_EFENCE_H */
