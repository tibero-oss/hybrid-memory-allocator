/**
 * @file    buddy_alloc.c
 * @brief   Buddy allocator
 *
 *
 * buddy 방식이기 때문에, 2^n 크기로만 메모리를 할당할 수 있고, 할당할 수 있는
 * 최소값은 BUDDY_PAGESIZE로 4096 (4kb) 이다.
 * buddy 방식은 간단하고, 효율도 좋고, external fragmentation이 없다는 장점이
 * 있으나, 할당하는 양이 2^n 이라 internal fragmentation이 늘어나게 될 수 있다.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <sys/mman.h>
#include "list.h"
#include "buddy_alloc.h"

#define _CHUNKSIZE(bin_idx) (BUDDY_PAGESIZE << (bin_idx))

#define _CHUNK2BITMAP(chunk, bin_idx) \
    (((char *)(chunk)-alloc->page_start) >> ((bin_idx) + BUDDY_PAGE_SHIFT))

#define _CHUNK_AT_OFFSET(chunk, offset) \
    ((buddy_chunk_t *)((char *)(chunk) + offset))

#define _BITMAP_BYTE(bin_idx, bitmap_idx) \
    (&(alloc->bitmap[bin_idx][(bitmap_idx) / 8]))

#define _BITMASK(bitmap_idx) (1 << ((bitmap_idx)&7))

static void buddy_free_internal(pbuddy_alloc_t *alloc, void *page, uint64_t size,
                                bool use_mutex);

/**
 * @brief         buddy_allocator 생성
 *
 * @param[in]   size     : allocator에서 사용할 memory의 전체 크기
 *
 * fixed memory allocator를 위한 buddy allocator를 작성한다.
 * fixed memory allocator 이므로 처음에 할당받은 memory만 가지고 작업하게 된다.
 *
 */
pbuddy_alloc_t *
buddy_allocator_new(void *page_start, uint64_t max_size, uint64_t size, char *file_fullpath)
{
    pbuddy_alloc_t *alloc; // 헤더
    char *bitmap;          // bitmap 영역
    int i;
    int bytes, bits;
    int available_bits;
    char *page;

    int byte_sum = 0;
    bits = max_size / BUDDY_PAGESIZE;
    for (i = 0; i < BUDDY_BINS_CNT; i++)
    {
        bytes = bits / 8 + 1; /* 마지막에 sentinel bit 필요 */
        byte_sum += bytes;
        bits = (bits + 1) / 2;
    }

    uint64_t reserved = sizeof(pbuddy_alloc_t) + byte_sum * sizeof(char);
    alloc = (pbuddy_alloc_t *)mmap(NULL, reserved, PROT_READ | PROT_WRITE,
                                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&alloc->mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    for (i = 0; i < BUDDY_BINS_CNT; i++)
        INIT_LIST_HEAD(&alloc->bins[i]);

    bitmap = (char *)alloc + sizeof(pbuddy_alloc_t);

    bits = max_size / BUDDY_PAGESIZE;
    for (i = 0; i < BUDDY_BINS_CNT; i++)
    {
        bytes = bits / 8 + 1; /* 마지막에 sentinel bit 필요 */
        alloc->bitmap[i] = bitmap;
        alloc->bitmap_size[i] = bytes;
        memset(bitmap, 0xff, bytes);

        bitmap += bytes;
        bits = (bits + 1) / 2;
    }

    alloc->page_start = (char *)page_start;
    alloc->reserved = reserved;

    /* 추후 expand 고려해서 쓸 수 있는 전체 buddy page 개수 */
    bits = max_size / BUDDY_PAGESIZE;
    /* 현재 buddy_malloc으로 쓸수 있는 buddy page 개수 */
    available_bits = size / BUDDY_PAGESIZE;

    /* page #0..#(bits - 1) : free pages
     * page #bits           : sentinal page
     */

    alloc->file_fullpath = file_fullpath;
    alloc->alloc_size = max_size;
    alloc->max_available_size = bits * BUDDY_PAGESIZE;
    alloc->available_size = available_bits * BUDDY_PAGESIZE;
    alloc->total_used = available_bits * BUDDY_PAGESIZE;
    alloc->periodic_total_used_max = 0;

    /*
     * buddy allocator 구조 다 만든 후 buddy_malloc으로 받을 수 있게
     * 처음 부터 끝까지 BUDDY_PAGESIZE 단위로 free 해 주는 과정 중에
     * 여기서! 실제 쓸 수 있는 곳 까지만 free 해 준다 그러면 아직
     * 뒷 부분은 free가 안 되서 malloc 받을 수 없다! 추후에 resize 때
     * TOTAL_SHM_SIZE가 늘어나면 그 때 늘어난 만큼 더 free 해 준다!
     */

    for (i = 0, page = page_start; i < available_bits; i++, page += BUDDY_PAGESIZE)
        buddy_free_internal(alloc, page, BUDDY_PAGESIZE, false);

    return alloc;
} /* buddy_allocator_new */

void buddy_allocator_expand(pbuddy_alloc_t *alloc, uint64_t old_size, uint64_t new_size)
{
    int i, old_bits, new_bits;
    char *page_start, *page;

    assert(old_size < new_size);

    /* 새로운 size만큼 뒷 부분 free 해주면 이후에 buddy malloc 가능 */
    old_bits = (old_size - alloc->reserved) / BUDDY_PAGESIZE;
    new_bits = (new_size - alloc->reserved) / BUDDY_PAGESIZE;

    alloc->available_size = new_bits * BUDDY_PAGESIZE;
    alloc->total_used += (new_bits - old_bits) * BUDDY_PAGESIZE;
    alloc->periodic_total_used_max = MAX(alloc->total_used,
                                         alloc->periodic_total_used_max);

    page_start = alloc->page_start + old_bits * BUDDY_PAGESIZE;

    for (i = old_bits, page = page_start; i < new_bits; i++, page += BUDDY_PAGESIZE)
        buddy_free_internal(alloc, page, BUDDY_PAGESIZE, false);
}

/**
 * @brief        buddy memory allocator
 *
 * @param[in]   alloc
 * @param[in]   size
 *
 * buddy allocator에서 메모리를 받아가는 함수.
 * 반드시 2^n 크기로 요청해야만 한다.
 */
void *
buddy_malloc(pbuddy_alloc_t *alloc, uint64_t size)
{
    buddy_chunk_t *chunk, *buddy;
    region_t *region;
    uint64_t tmp_size;
    int i;
    int bin_idx;             /* bitmap 몇 번째 레벨 (ex. 8192 -> 0) */
    int bitmap_idx, bitmask; /* 그 레벨 bitmap 중 몇 번째 chunk */
    char *bitmap_byte;

    /*
     * bin_idx는 요구하는 size가 몇 레벨 chunk에 해당되는지 나타냄
     * bitmap_idx는 해당 레벨 chunk 중 앞에서부터 몇 번째 chunk인지 나타냄
     * 예) size = 8192, 앞에서부터 3번째까지 8192 chunk 모두 사용 중이면
     * bin_idx = 1 (2번째로 작은 chunk, bitmap_idx = 3 (앞에서부터 4번째)
     */

    /* 2의 제곱수 확인 */
    size = get_buddy_alloc_size(size);
    assert(size <= BUDDY_MAX_CHUNKSIZE && (size & (size - 1)) == 0);

    pthread_mutex_lock(&alloc->mutex);

    tmp_size = size >> BUDDY_PAGE_SHIFT;
    bin_idx = 0;
    while (tmp_size > 1)
    {
        bin_idx++;
        tmp_size = tmp_size >> 1;
    }

    for (i = 0;; i++)
    {
        if (bin_idx >= BUDDY_BINS_CNT)
        {
            pthread_mutex_unlock(&alloc->mutex);
            return NULL;
        }

        if (!list_empty(&(alloc->bins[bin_idx])))
            break;

        bin_idx++;
    }

    chunk = list_entry(alloc->bins[bin_idx].next, buddy_chunk_t, link);
    list_del(&chunk->link);

    bitmap_idx = _CHUNK2BITMAP(chunk, bin_idx);
    bitmap_byte = _BITMAP_BYTE(bin_idx, bitmap_idx);
    bitmask = _BITMASK(bitmap_idx);

    *bitmap_byte |= bitmask;

    for (; i > 0; i--)
    {
        bin_idx--;

        buddy = _CHUNK_AT_OFFSET(chunk, (ptrdiff_t)_CHUNKSIZE(bin_idx));

        list_add_tail(&buddy->link, &(alloc->bins[bin_idx]));

        bitmap_idx = _CHUNK2BITMAP(buddy, bin_idx);
        bitmap_byte = _BITMAP_BYTE(bin_idx, bitmap_idx);
        bitmask = _BITMASK(bitmap_idx);

        *bitmap_byte &= ~bitmask;
    }

    alloc->total_used += size;
    alloc->periodic_total_used_max = MAX(alloc->total_used,
                                         alloc->periodic_total_used_max);

    pthread_mutex_unlock(&alloc->mutex);

    /* buddy chunk에서 region으로 변환 */
    region = (region_t *)chunk;
    region->prev = region->next = NULL;
    region->size = size;

    return region;
} /* buddy_malloc */

/**
 * @brief   buddy memory deallocator
 *
 * @param[in]      alloc
 * @param[in,out]  page
 * @param[in]      size
 *
 * buddy malloc으로 받은 메모리를 반납하는 함수
 *
 * @warning 일반 free와는 달리, 반드시 할당받았던 size도 적어줘야 한다!
 */
void buddy_free(pbuddy_alloc_t *alloc, void *page, uint64_t size)
{
    buddy_free_internal(alloc, page, size, true);
} /* buddy_free */

static void
buddy_free_internal(pbuddy_alloc_t *alloc, void *page, uint64_t size,
                    bool use_mutex)
{
    buddy_chunk_t *chunk, *buddy;
    uint64_t tmp_size;
    int bin_idx;
    int bitmap_idx, bitmask, buddy_bitmask;
    char *bitmap_byte;

    /* 2의 제곱수 확인 */
    assert(size <= BUDDY_MAX_CHUNKSIZE && (size & (size - 1)) == 0);

    if (use_mutex)
        pthread_mutex_lock(&alloc->mutex);

    alloc->total_used -= size;

    tmp_size = size >> BUDDY_PAGE_SHIFT;
    bin_idx = 0;
    while (tmp_size > 1)
    {
        bin_idx++;
        tmp_size = tmp_size >> 1;
    }

    bitmap_idx = _CHUNK2BITMAP(page, bin_idx);

    chunk = page;

    while (1)
    {
        bitmap_byte = _BITMAP_BYTE(bin_idx, bitmap_idx);
        bitmask = _BITMASK(bitmap_idx);
        buddy_bitmask = _BITMASK(bitmap_idx ^ 1);

        if ((*bitmap_byte & buddy_bitmask) || bin_idx == BUDDY_BINS_CNT - 1)
            break; /* Buddy is allocated or has maximum size. */

        /* Buddy is free: coalesce. */
        *bitmap_byte |= buddy_bitmask;

        if ((bitmap_idx & 1) == 1)
            buddy = _CHUNK_AT_OFFSET(chunk, -size);
        else
            buddy = _CHUNK_AT_OFFSET(chunk, size);

        list_del(&buddy->link);

        if ((bitmap_idx & 1) == 1)
            chunk = buddy;

        size *= 2;
        bin_idx++;
        bitmap_idx /= 2;
    }

    *bitmap_byte &= ~bitmask;

    /* free list에 추가 */
    list_add_tail(&chunk->link, &(alloc->bins[bin_idx]));

    if (use_mutex)
        pthread_mutex_unlock(&alloc->mutex);
} /* buddy_free_internal */

/**
 * @brief       buddy allocator 확인을 위한 함수
 *
 * @param[in]   alloc
 *
 * allocator의 bitmap 구조와, 각 size에 남아있는 메모리 chunk를 출력해 준다.
 */
void buddy_dbg_print(pbuddy_alloc_t *alloc)
{
    int i;
    buddy_chunk_t *ptr;

    printf("alloc = %p, total size = %zd, max_available = %zd, available = %zd, used = %zd, page_start = %p\n",
           alloc, alloc->alloc_size, alloc->max_available_size,
           alloc->available_size, alloc->total_used, alloc->page_start);
    for (i = 0; i < BUDDY_BINS_CNT; i++)
    {
        printf("#%d chunk :\n", i);
        // dump_data(dstream, alloc->bitmap[i], alloc->bitmap_size[i]);

        if (list_empty(&(alloc->bins[i])))
            continue;
        list_for_each_entry(ptr, &alloc->bins[i], link, buddy_chunk_t)
            printf("%p -> ", ptr);

        printf("\n");
    }
}

void get_buddy_alloc_state(pbuddy_alloc_t *alloc, uint64_t *total_size, uint64_t *used_size)
{
    *total_size = alloc->available_size;
    *used_size = alloc->total_used;
}

uint64_t
get_buddy_alloc_total_size(pbuddy_alloc_t *alloc)
{
    return alloc->available_size;
}

uint64_t
get_buddy_alloc_size(uint64_t size)
{
    uint64_t uint64_to_alloc = BUDDY_PAGESIZE;
    while (uint64_to_alloc < size)
    {
        uint64_to_alloc = 2 * uint64_to_alloc;
    }
    return uint64_to_alloc;
}

uint64_t
get_buddy_alloc_size_rounddown(uint64_t size)
{
    uint64_t uint64_to_alloc = BUDDY_PAGESIZE;
    uint64_t temp_size = BUDDY_PAGESIZE;

    while (temp_size <= size)
    {
        uint64_to_alloc = temp_size;
        temp_size = 2 * temp_size;
    }
    return uint64_to_alloc;
}

/* end of buddy_alloc.c */
