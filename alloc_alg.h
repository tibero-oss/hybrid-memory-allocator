/**
 * @file    alloc_alg.h
 * @brief   Allocator algorithm.
 *
 * Based on Doug Lea's malloc v2.8.2.
 *
 * @author
 * @version \$Id$
 */
/*******************************************************************************
  This is a version (aka dlmalloc) of malloc/free/realloc written by
  Doug Lea and released to the public domain, as explained at
  http://creativecommons.org/licenses/publicdomain.  Send questions,
  comments, complaints, performance data, etc to dl@cs.oswego.edu

* Version 2.8.2 Sun Jun 12 16:05:14 2005  Doug Lea  (dl at gee)

   Note: There may be an updated version of this malloc obtainable at
           ftp://gee.cs.oswego.edu/pub/misc/malloc.c
         Check before installing!
*******************************************************************************/

/*************************************************************************
 * {{{ malloc helper functions
 *************************************************************************/
/* allocate a large request from the best fitting chunk in a treebin */
static inline chunk_t *
tmalloc_large(alloc_t *alloc, csize_t reqsize, csize_t chunkbits)
{
    treechunk_t *tchunk, *victim;
    csize_t rsize = -reqsize; /* Unsigned negation */
    chunk_t *remainder;
    bindex_t idx;
    COMPUTE_TREE_INDEX(reqsize, idx);

    victim = NULL;

    if ((tchunk = *TREEBIN_AT(alloc, idx)) != 0) {
        /* Traverse tree for this bin looking for node with size == reqsize */
        csize_t sizebits = reqsize << LEFTSHIFT_FOR_TREE_INDEX(idx);
        treechunk_t *right_subtree = NULL;
            /* The deepest untaken right subtree */

        for (;;) {
            treechunk_t *rt;
            csize_t trem = GET_FREECHUNKSIZE(tchunk) - reqsize;

            if (trem < rsize) {
                victim = tchunk;
                if ((rsize = trem) == 0)
                    break;
            }

            rt = tchunk->child[1];
            tchunk = tchunk->child[(sizebits >> (CSIZE_T_BITSIZE-1)) & 1];

            if (rt != 0 && rt != tchunk)
                right_subtree = rt;

            if (tchunk == 0) {
                /* set tchunk to least subtree holding sizes > reqsize */
                tchunk = right_subtree;
                break;
            }

            sizebits <<= 1;
        }
    }

    if (tchunk == 0 && victim == 0) {
        /* set tchunk to root of next non-empty treebin */
        binmap_t leftbits = LEFT_BITS(IDX2BIT(idx)) & alloc->treemap;

        if (leftbits != 0) {
            bindex_t i;
            binmap_t leastbit = LEAST_BIT(leftbits);

            compute_bit2idx(leastbit, i);
            tchunk = *TREEBIN_AT(alloc, i);
        }
    }

    while (tchunk != 0) { /* find smallest of tree or subtree */
        csize_t trem = GET_FREECHUNKSIZE(tchunk) - reqsize;

        if (trem < rsize) {
            rsize = trem;
            victim = tchunk;
        }
        tchunk = LEFTMOST_CHILD(tchunk);
    }

    /*  If dv is a better fit, return 0 so malloc will use it */
    if (victim == 0 || rsize >= (csize_t) (alloc->dvsize - reqsize))
        return NULL;

    remainder = CHUNK_PLUS_OFFSET(victim, reqsize);
    /* assert(GET_CHUNKSIZE(victim) == rsize + reqsize); */

    unlink_large_chunk(alloc, victim);
    if (rsize < MIN_CHUNK_SIZE)
        SET_INUSE_AND_PINUSE(victim, (rsize + reqsize), chunkbits);
    else {
        SET_SIZE_AND_PINUSE_OF_INUSE_CHUNK(victim, reqsize, chunkbits);
        SET_SIZE_AND_PINUSE_OF_FREE_CHUNK(remainder, rsize);
        insert_chunk(alloc, remainder, rsize);
    }

    return (chunk_t *) victim;
} /* tmalloc_large */


/* allocate a small request from the best fitting chunk in a treebin */
static inline chunk_t *
tmalloc_small(alloc_t *alloc, csize_t reqsize, csize_t chunkbits)
{
    treechunk_t *tchunk, *victim;
    chunk_t *remainder;
    csize_t rsize;
    bindex_t idx;
    binmap_t leastbit = LEAST_BIT(alloc->treemap);

    compute_bit2idx(leastbit, idx);

    victim = tchunk = *TREEBIN_AT(alloc, idx);
    rsize = GET_FREECHUNKSIZE(tchunk) - reqsize;

    while ((tchunk = LEFTMOST_CHILD(tchunk)) != 0) {
        csize_t trem = GET_FREECHUNKSIZE(tchunk) - reqsize;

        if (trem < rsize) {
            rsize = trem;
            victim = tchunk;
        }
    }

    remainder = CHUNK_PLUS_OFFSET(victim, reqsize);
    /* assert(GET_CHUNKSIZE(victim) == rsize + reqsize); */

    unlink_large_chunk(alloc, victim);
    if (rsize < MIN_CHUNK_SIZE)
        SET_INUSE_AND_PINUSE(victim, (rsize + reqsize), chunkbits);
    else {
        SET_SIZE_AND_PINUSE_OF_INUSE_CHUNK(victim, reqsize, chunkbits);
        SET_SIZE_AND_PINUSE_OF_FREE_CHUNK(remainder, rsize);
        replace_dv(alloc, remainder, rsize);
    }

    return (chunk_t *) victim;
} /* tmalloc_small */


static chunk_t *
init_new_region(
    alloc_t *alloc, region_t *region,
    csize_t region_size, csize_t chunksize, csize_t chunkbits)
{
    chunk_t *chunk1, *chunk2, *chunk3;
    csize_t chunksize2;

    /* Region을 chunk1 (allocated), chunk2 (free), chunk3 (footer) 로 분할. */
    chunk1 = REGION2CHUNK(region);

    chunk2 = CHUNK_PLUS_OFFSET(chunk1, chunksize);
    chunk3 = REGION2FOOTER(region, region_size);

    region->size = region_size;

    chunk1->head = chunksize | PINUSE_BIT | CINUSE_BIT | chunkbits;

    chunk3->head = region_size | PINUSE_BIT | CINUSE_BIT | FOOTER_BIT;

    chunksize2 = (char *) chunk3 - (char *) chunk2;
    if (chunksize2 >= MIN_CHUNK_SIZE) {
        SET_SIZE_AND_PINUSE_OF_FREE_CHUNK(chunk2, chunksize2);

        /* chunk2는 free_internal을 부르면 알아서 맞는 bin에 들어간다.
         *
         * free 상태로 만들어서 부르는 이유는, shared pool allocator의 경우
         * allocated chunk는 1M 크기 제한이 있는데 chunksize2는 더 클 수
         * 있기 때문이다.
         */
        free_internal(alloc, chunk2, false);
    }
    else {
        /*
        printf("Cannot split new region! region %p size1 %d size2 %d\n",
               region, chunksize, chunksize2);
        */
        chunk1->head = (chunksize + chunksize2) | PINUSE_BIT | CINUSE_BIT |
                       chunkbits;
    }

    return chunk1;
} /* init_new_region */
/*************************************************************************
 * }}} malloc helper functions
 *************************************************************************/

/*************************************************************************
 * {{{ malloc algorithm
 *************************************************************************/
chunk_t *
malloc_internal(
    alloc_t *alloc, uint64_t reqsize
    )
{
    /*
       Basic algorithm:
       If a small request (< 256 bytes minus per-chunk overhead):
       1. If one exists, use a remainderless chunk in associated smallbin.
       (Remainderless means that there are too few excess bytes to
       represent as a chunk.)
       2. If it is big enough, use the dv chunk, which is normally the
       chunk adjacent to the one used for the most recent small request.
       3. If one exists, split the smallest available chunk in a bin,
       saving remainder in dv.
       4. If it is big enough, use the top chunk.
       5. If available, get memory from system and use it
       Otherwise, for a large request:
       1. Find the smallest available binned chunk that fits, and use it
       if it is better fitting than dv chunk, splitting if necessary.
       2. If better fitting than any binned chunk, use the dv chunk.
       3. If it is big enough, use the top chunk.
       4. If request size >= mmap threshold, try to directly mmap this chunk.
       5. If available, get memory from system and use it

       The ugly goto's here ensure that postaction occurs along all paths.
     */

    csize_t size;
    chunk_t *bin;
    chunk_t *chunk, *next;
    csize_t chunkbits = 0; /* used only for shared pool allocators */

    chunkbits = ALLOC_CHUNK_BITS(alloc->alloc_idx);

    if (reqsize <= MAX_SMALL_SIZE) {
        bindex_t idx;
        binmap_t smallbits;

        idx = SMALL_INDEX(reqsize);
        smallbits = alloc->smallmap >> idx;

        if ((smallbits & 0x3) != 0) {   /* Remainderless fit to a smallbin. */
            idx += ~smallbits & 1;      /* Uses next bin if idx empty */
            bin = SMALLBIN_AT(alloc, idx);
            chunk = bin->fd;
            /* assert(GET_CHUNKSIZE(chunk) == SMALL_INDEX2SIZE(idx)); */
            unlink_first_small_chunk(alloc, bin, chunk, idx);
            SET_INUSE_AND_PINUSE(chunk,
                                 (int)SMALL_INDEX2SIZE(idx), chunkbits);
            return chunk;
        }

        if (reqsize > alloc->dvsize) {
            if (smallbits != 0) { /* Use chunk in next nonempty smallbin */
                csize_t rsize;
                bindex_t i;
                binmap_t leftbits =
                    (smallbits << idx) & LEFT_BITS(IDX2BIT(idx));
                binmap_t leastbit = LEAST_BIT(leftbits);

                compute_bit2idx(leastbit, i);
                bin = SMALLBIN_AT(alloc, i);
                chunk = bin->fd;
                /* assert(GET_CHUNKSIZE(chunk) == SMALL_INDEX2SIZE(i)); */
                unlink_first_small_chunk(alloc, bin, chunk, i);
                rsize = SMALL_INDEX2SIZE(i) - reqsize;
                /* Fit here cannot be remainderless if 4byte sizes */
                if (rsize < MIN_CHUNK_SIZE) {
                    SET_INUSE_AND_PINUSE(chunk,
                                         (int)SMALL_INDEX2SIZE(i), chunkbits);
                    return chunk;
                }

                next = CHUNK_PLUS_OFFSET(chunk, reqsize);
                SET_SIZE_AND_PINUSE_OF_INUSE_CHUNK(chunk, reqsize, chunkbits);
                SET_SIZE_AND_PINUSE_OF_FREE_CHUNK(next, rsize);
                replace_dv(alloc, next, rsize);

                return chunk;
            }

            if (alloc->treemap != 0 &&
                (chunk = tmalloc_small(alloc, reqsize, chunkbits)) != 0) {
                return chunk;
            }
        }
    }
    else if (alloc->treemap != 0 &&
             (chunk = tmalloc_large(alloc, reqsize, chunkbits)) != 0) {
        return chunk;
    }

    if (reqsize <= alloc->dvsize) {
        csize_t rsize = alloc->dvsize - reqsize;

        chunk = alloc->dv;
        if (rsize >= MIN_CHUNK_SIZE) {  /* split dv */
            next = alloc->dv = CHUNK_PLUS_OFFSET(chunk, reqsize);
            alloc->dvsize = rsize;
            SET_SIZE_AND_PINUSE_OF_FREE_CHUNK(next, rsize);
            SET_SIZE_AND_PINUSE_OF_INUSE_CHUNK(chunk, reqsize, chunkbits);
        }
        else {                /* exhaust dv */
            csize_t dvs = alloc->dvsize;
            alloc->dvsize = 0;
            alloc->dv = 0;
            SET_INUSE_AND_PINUSE(chunk, dvs, chunkbits);
        }

        return chunk;
    }

    /* No free space: call the higher-level allocator! */
    {
        size_t pagesize = 0; /* total_size와 같은 type 사용! */
        region_t *region = NULL;
        region_t *region_prev, *region_next;

        size = CHUNK2REGIONSIZE(reqsize);
        if (size >= (csize_t) IPARAM(_TOTAL_SYS_MEM_SIZE))
            return NULL;

        switch (alloc->alloctype) {
        case REGION_ALLOC_ROOT:
            if (alloc->total_size == 0 &&
                size <= IPARAM(_SYSTEM_MEMORY_INIT_SIZE))
                pagesize = IPARAM(_SYSTEM_MEMORY_INIT_SIZE);
            else
                pagesize = IPARAM(_SYSTEM_MEMORY_EXPAND_SIZE);

            /* 한번의 요청이 EXPAND_SIZE 이상일 경우엔 원하는 크기만큼 받아주자 */
            if (pagesize < size)
                pagesize = size;

            region = (region_t *) get_new_page(pagesize);
            break;
        case REGION_ALLOC_SYS:
        case REGION_ALLOC_PMEM:
            /* 현재까지 받은 total size의 절반 크기의 page 생성.
             * (단, 최소값 4K, 최대값 1M.)
             * (단, 사용자의 요청이 1M보다 크면 물론 사용자가 요청한 크기만큼.)
             */
            pagesize = (size_t)(TB_ALIGN64(alloc->total_size / 2));
            if (pagesize < 4096UL)
                pagesize = 4096UL;

            if (use_root_allocator) {
                if (pagesize >= 1048576UL) /* 1M */
                    pagesize = 1048576UL;
            } else {
                if (pagesize >= (size_t) IPARAM(_SYSTEM_MEMORY_EXPAND_SIZE))
                    pagesize = IPARAM(_SYSTEM_MEMORY_EXPAND_SIZE);
            }

            if (pagesize < size)
                pagesize = size;

            if (alloc->alloctype == REGION_ALLOC_PMEM) {
                pagesize = get_pbuddy_alloc_size(pagesize);
                region = (region_t *)pbuddy_malloc(pagesize);
            }
            else if (use_root_allocator) 
                region = (region_t *)tb_root_malloc(pagesize);
            else
                region = (region_t *)get_new_page(pagesize);

            break;

        default:
            assert(0);
        }

        if (region == NULL)
            return NULL;

        alloc->total_size += pagesize;

        region->size = (csize_t)pagesize;

        region_prev = &(alloc->regions);
        region_next = region_prev->next;

        region->prev = region_prev;
        region->next = region_next;
        region_prev->next = region;
        region_next->prev = region;

        return init_new_region(alloc, region,
                                      pagesize, reqsize, chunkbits);
    }
} /* malloc_internal */
/*************************************************************************
 * }}} malloc algorithm
 *************************************************************************/


/*************************************************************************
 * {{{ realloc algorithm
 *************************************************************************/
/* XXX shared pool allocator에 대해서는 동작 안함 */
chunk_t *
realloc_internal(alloc_t *alloc, chunk_t *chunk, uint64_t reqsize)
{
    csize_t oldsize = GET_CHUNKSIZE(chunk);
    chunk_t *newchunk;
    csize_t chunkbits = GET_CHUNK_BITS(chunk->head);

    /* Try to either shrink or extend into top. Else malloc-copy-free */

    if (oldsize >= reqsize) {   /* already big enough */
        csize_t rsize = oldsize - reqsize;

        if (rsize >= MIN_CHUNK_SIZE) {
            chunk_t *remainder = CHUNK_PLUS_OFFSET(chunk, reqsize);

#ifdef _ALLOC_USE_DBGINFO
#ifdef TB_DEBUG
            /* realloc에서는 미리 memeory를 닦아줄 수 없으므로,
             * 이곳에서 닦는다. */
            memset(remainder, 0xCA, rsize);
#endif
#endif
            SET_INUSE(chunk, reqsize, chunkbits);
            SET_INUSE(remainder, rsize, chunkbits);

            free_internal(alloc, remainder, false);
        }

        return chunk;
    }

    newchunk = malloc_internal(alloc, reqsize);

    if (newchunk != 0) {
        void *newmem = CHUNK2MEM(newchunk);
        /* oldsize < reqsize 일 때만 이리로 온다. */
        memcpy(newmem, CHUNK2MEM(chunk), oldsize - CHUNK_OVERHEAD);
#ifdef _ALLOC_USE_DBGINFO
#ifdef TB_DEBUG
        memset(CHUNK2MEM(chunk), 0xCA, oldsize - CHUNK_OVERHEAD);
#endif
#endif
        free_internal(alloc, chunk, false);
    }

    return newchunk;
} /* region_realloc_internal */
/*************************************************************************
 * }}} realloc algorithm
 *************************************************************************/


/*************************************************************************
 * {{{ free algorithm
 *************************************************************************/
void
free_internal(alloc_t *alloc, chunk_t *chunk,
                     tb_bool_t skip_fc)
{
    /*
      Consolidate freed chunks with preceeding or succeeding bordering
      free chunks, if they exist, and then place in a bin.  Intermixed
      with special cases for top, dv, mmapped chunks, and usage errors.
    */

    csize_t chunksize = GET_CHUNKSIZE(chunk);
    chunk_t *next = CHUNK_PLUS_OFFSET(chunk, chunksize);

    csize_t region_size;
    tb_bool_t do_footer_check;

    //printf("free %p, chunksize = %d\n", chunk, chunksize);

    if (!PINUSE(chunk)) { /* consolidate backward */
        csize_t prevsize = chunk->prev_foot;
        chunk_t *prev = CHUNK_MINUS_OFFSET(chunk, prevsize);

        //printf("  coalescing prev %p (size %d)\n", prev, prevsize);
        /* double free 를 dectect 하기 위해서 일부로 CINUSE 를 clear */
        CLEAR_CINUSE(chunk);
        chunksize += prevsize;
        chunk = prev;

        if (chunk != alloc->dv) {
            unlink_chunk(alloc, chunk, prevsize);
        }
        else if ((next->head & INUSE_BITS) == INUSE_BITS) {
            alloc->dvsize = chunksize;
            SET_FREE_WITH_PINUSE(chunk, chunksize, next);

            goto footer_check;
        }
    }

    if (!CINUSE(next)) {  /* consolidate forward */
        //printf("  coalescing next %p (size %d)\n", next, GET_CHUNKSIZE(next));

        if (next == alloc->dv) {
            chunksize += alloc->dvsize;
            alloc->dvsize = chunksize;

            alloc->dv = chunk;
            SET_SIZE_AND_PINUSE_OF_FREE_CHUNK(chunk, chunksize);

            next = CHUNK_PLUS_OFFSET(chunk, chunksize);
            goto footer_check;
        }
        else {
            csize_t nextsize = GET_FREECHUNKSIZE(next);

            chunksize += nextsize;
            unlink_chunk(alloc, next, nextsize);
            SET_SIZE_AND_PINUSE_OF_FREE_CHUNK(chunk, chunksize);

            next = CHUNK_PLUS_OFFSET(chunk, chunksize);

            if (chunk == alloc->dv) {
                alloc->dvsize = chunksize;
                goto footer_check;
            }
        }
    }
    else
        SET_FREE_WITH_PINUSE(chunk, chunksize, next);

footer_check:
    if (skip_fc) do_footer_check = false;
    else
        do_footer_check = (next->head & FOOTER_BIT) != 0;

    if (do_footer_check) {

        region_size = next->head & ~(FOOTER_BIT | INUSE_BITS);

        if (chunksize == REGION2CHUNKSIZE(region_size)) {
            region_t *region, *region_prev, *region_next;

            if (chunk == alloc->dv) {
                alloc->dv = NULL;
                alloc->dvsize = 0;
            }

            region = CHUNK2REGION(chunk);

            region_prev = region->prev;
            region_next = region->next;
            region_prev->next = region_next;
            region_next->prev = region_prev;

            switch (alloc->alloctype) {
            case REGION_ALLOC_ROOT:
                // printf("TID=%d: free:  %d\n", tb_get_thrid(), region_size);
                free_page(region, region_size);
                break;
            case REGION_ALLOC_SYS:
                if (use_root_allocator)
                    tb_root_free(region);
                else
                    free_page(region, region_size);
                break;
            case REGION_ALLOC_PMEM:
                pbuddy_free(region, region_size);
                break;
            default:
                assert(0);
            }

            assert(alloc->total_size >= region_size);
            alloc->total_size -= region_size;

            return;
        }
    }

    if (chunk != alloc->dv) {
        insert_chunk(alloc, chunk, chunksize);
    }
} /* free_internal */
/*************************************************************************
 * }}} free algorithm
 *************************************************************************/
