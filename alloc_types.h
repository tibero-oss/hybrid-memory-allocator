/**
 * @file    alloc_types.h
 * @brief   Doug Lea's allocator: internal types & definitions.
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


/* Internal header라 두 번 include하면 버그 */
#ifdef _ALLOC_TYPES_H
#   error "Cannot include alloc_types.h twice!!!"
#else
#define _ALLOC_TYPES_H

/* ------------------- csize_t and alignment properties -------------------- */

typedef uint64_t csize_t;   /* chunk size */

/* The byte and bit size of a csize_t */
#define CSIZE_T_SIZE         (sizeof(csize_t))
#define CSIZE_T_BITSIZE      (sizeof(csize_t) << 3)

#define ALLOC_IDX_BITS 8
#define ALLOC_IDX_SHIFT (64 - ALLOC_IDX_BITS)

#define ALLOC_IDX_MASK        0xff00000000000000LLU
/* Chunk size는 MAX_CHUNK_SIZE보다 작아야 함. */
#define MAX_CHUNK_SIZE (1LLU << 56)

#define ALLOC_CHUNK_BITS(alloc_idx)                                            \
     (((csize_t) (alloc_idx)) << ALLOC_IDX_SHIFT)

#define GET_CHUNK_BITS(head)                                                   \
     (((csize_t) (head)) & ALLOC_IDX_MASK)

#define GET_ALLOC_IDX(chunk)                                                   \
     ( (((csize_t) (chunk)->head) & ALLOC_IDX_MASK ) >> ALLOC_IDX_SHIFT )

/*************************************************************************
 * {{{ Stolen from dlmalloc
 *************************************************************************/
#define MALLOC_ALIGNMENT 8

/* The bit mask value corresponding to MALLOC_ALIGNMENT */
#define CHUNK_ALIGN_MASK    (MALLOC_ALIGNMENT - 1)

/* True if address a has acceptable alignment */
#define IS_ALIGNED(A)       (((ptrdiff_t)(A) & (CHUNK_ALIGN_MASK)) == 0)

/* -----------------------  Chunk representations ------------------------ */

/*
  (The following includes lightly edited explanations by Colin Plumb.)

  The malloc_chunk declaration below is misleading (but accurate and
  necessary).  It declares a "view" into memory allowing access to
  necessary fields at known offsets from a given base.

  Chunks of memory are maintained using a `boundary tag' method as
  originally described by Knuth.  (See the paper by Paul Wilson
  ftp://ftp.cs.utexas.edu/pub/garbage/allocsrv.ps for a survey of such
  techniques.)  Sizes of free chunks are stored both in the front of
  each chunk and at the end.  This makes consolidating fragmented
  chunks into bigger chunks fast.  The head fields also hold bits
  representing whether chunks are free or in use.

  Here are some pictures to make it clearer.  They are "exploded" to
  show that the state of a chunk can be thought of as extending from
  the high 31 bits of the head field of its header through the
  prev_foot and PINUSE_BIT bit of the following chunk header.

  A chunk that's in use looks like:

   chunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           | Size of previous chunk (if P = 1)                             |
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ |P|
         | Size of this chunk                                         1| +-+
   mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               |
         +-                                                             -+
         |                                                               |
         +-                                                             -+
         |                                                               :
         +-      size - sizeof(size_t) available payload bytes          -+
         :                                                               |
 chunk-> +-                                                             -+
         |                                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ |1|
       | Size of next chunk (may or may not be in use)               | +-+
 mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    And if it's free, it looks like this:

   chunk-> +-                                                             -+
           | User payload (must be in use, or we would have merged!)       |
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ |P|
         | Size of this chunk                                         0| +-+
   mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Next pointer                                                  |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Prev pointer                                                  |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               :
         +-      size - sizeof(struct chunk) unused bytes               -+
         :                                                               |
 chunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Size of this chunk                                            |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ |0|
       | Size of next chunk (must be in use, or we would have merged)| +-+
 mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                                                               :
       +- User payload                                                -+
       :                                                               |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                                                     |0|
                                                                     +-+
  Note that since we always merge adjacent free chunks, the chunks
  adjacent to a free chunk must be in use.

  Given a pointer to a chunk (which can be derived trivially from the
  payload pointer) we can, in O(1) time, find out whether the adjacent
  chunks are free, and if so, unlink them from the lists that they
  are on and merge them with the current chunk.

  Chunks always begin on even word boundaries, so the mem portion
  (which is returned to the user) is also on an even word boundary, and
  thus at least double-word aligned.

  The P (PINUSE_BIT) bit, stored in the unused low-order bit of the
  chunk size (which is always a multiple of two words), is an in-use
  bit for the *previous* chunk.  If that bit is *clear*, then the
  word before the current chunk size contains the previous chunk
  size, and can be used to find the front of the previous chunk.
  The very first chunk allocated always has this bit set, preventing
  access to non-existent (or non-owned) memory. If PINUSE is set for
  any given chunk, then you CANNOT determine the size of the
  previous chunk, and might even get a memory addressing fault when
  trying to do so.

  The C (CINUSE_BIT) bit, stored in the unused second-lowest bit of
  the chunk size redundantly records whether the current chunk is
  inuse. This redundancy enables usage checks within free and realloc,
  and reduces indirection when freeing and consolidating chunks.

  Each freshly allocated chunk must have both CINUSE and PINUSE set.
  That is, each allocated chunk borders either a previously allocated
  and still in-use chunk, or the base of its memory arena. This is
  ensured by making all allocations from the the `lowest' part of any
  found chunk.  Further, no free chunk physically borders another one,
  so each free chunk is known to be preceded and followed by either
  inuse chunks or the ends of memory.

  Note that the `foot' of the current chunk is actually represented
  as the prev_foot of the NEXT chunk. This makes it easier to
  deal with alignments etc but can be very confusing when trying
  to extend or adapt this code.

  The exceptions to all this are

     1. The special chunk `top' is the top-most available chunk (i.e.,
        the one bordering the end of available memory). It is treated
        specially.  Top is never included in any bin, is used only if
        no other chunk is available, and is released back to the
        system if it is very large (see M_TRIM_THRESHOLD).  In effect,
        the top chunk is treated as larger (and thus less well
        fitting) than any other available chunk.  The top chunk
        doesn't update its trailing size field since there is no next
        contiguous chunk that would have to index off it. However,
        space is still allocated for it (TOP_FOOT_SIZE) to enable
        separation or merging when space is extended.

     3. Chunks allocated via mmap, which have the lowest-order bit
        (IS_MMAPPED_BIT) set in their prev_foot fields, and do not set
        PINUSE_BIT in their head fields.  Because they are allocated
        one-by-one, each must carry its own prev_foot field, which is
        also used to hold the offset this chunk has within its mmapped
        region, which is needed to preserve alignment. Each mmapped
        chunk is trailed by the first two fields of a fake next-chunk
        for sake of usage checks.

*/

typedef struct chunk_s chunk_t;
struct chunk_s {
    csize_t prev_foot;            /* Size of previous chunk (if free).  */
    csize_t head;                 /* Size and inuse bits. */
    chunk_t *fd;                  /* double links -- used only if free. */
    chunk_t *bk;
};

typedef unsigned int bindex_t;         /* Described below */
typedef unsigned int binmap_t;         /* Described below */

/* ------------------- Chunks sizes and alignments ----------------------- */

#define MCHUNK_SIZE         (sizeof(chunk_t))

#define CHUNK_OVERHEAD   (CSIZE_T_SIZE)
/* conversion from malloc headers to user pointers, and back */
#define CHUNK2MEM(p)     ((void*)((char*)(p)       + (CSIZE_T_SIZE * 2U)))
#define MEM2CHUNK(mem)   ((chunk_t *)((char*)(mem) - (CSIZE_T_SIZE * 2U)))
#define SET_START_OFFSET(p, req_size) ((void) 0)

/* pad request bytes into a usable size */
#define PAD_REQUEST(req) \
   (csize_t) (((req) + CHUNK_OVERHEAD + CHUNK_ALIGN_MASK) & ~CHUNK_ALIGN_MASK)

/* The smallest size we can malloc is an aligned minimal chunk */
#ifdef _ALLOC_USE_DBGINFO
    /* _ALLOC_USE_DBGINFO 일 경우에는, MCHUNK_SIZE 보다 DBGINFO에 의한 영역이
     * 더 많이 필요하기 때문에 MIN_CHUNK_SIZE 의 기준이 MCHUNK_SIZE 대신
     * DBGINFO가 된다. (DBGINFO가 사용되는 것은 allocated 된 영역에 대해서이고,
     * MCHUNK_SIZE 가 사용되는 것은 free 된 영역에 대해서이다.
     */
#   define MIN_CHUNK_SIZE PAD_REQUEST(_ALLOC_ADD_DBGINFO_SIZE(1U))
#else
#   define MIN_CHUNK_SIZE                                                      \
    (csize_t) (((MCHUNK_SIZE + CHUNK_ALIGN_MASK) & ~CHUNK_ALIGN_MASK))
#endif

/* Bounds on request (not chunk) sizes. */
#define MIN_REQUEST         (MIN_CHUNK_SIZE - CHUNK_OVERHEAD - 1U)

/* pad request, checking for minimum (but not maximum) */
#define REQUEST2SIZE(req)                                                      \
    (((req) + 0U < MIN_REQUEST) ? MIN_CHUNK_SIZE : PAD_REQUEST(req))


/* ------------------ Operations on head and foot fields ----------------- */

/*
  The head field of a chunk is or'ed with PINUSE_BIT when previous
  adjacent chunk in use, and or'ed with CINUSE_BIT if this chunk is in
  use. If the chunk was obtained with mmap, the prev_foot field has
  IS_MMAPPED_BIT set, otherwise holding the offset of the base of the
  mmapped region to the base of the chunk.
*/

#define PINUSE_BIT          (1ULL)
#define CINUSE_BIT          (2ULL)
#define FOOTER_BIT          (4ULL)
#define INUSE_BITS          (PINUSE_BIT | CINUSE_BIT)

/* extraction of fields from head words */
#define CINUSE(p)           ((p)->head & CINUSE_BIT)
#define CLEAR_CINUSE(p)     ((p)->head &= ~CINUSE_BIT)
#define PINUSE(p)           ((p)->head & PINUSE_BIT)
#define FOOTER(p)           ((p)->head & FOOTER_BIT)
#define CLEAR_PINUSE(p)     ((p)->head &= ~PINUSE_BIT)

#   define GET_FREECHUNKSIZE(p)                                                \
        ((p)->head & ~(INUSE_BITS | FOOTER_BIT))
#   define GET_ALLOCCHUNKSIZE(p)                                               \
        ((p)->head & ~(ALLOC_IDX_MASK | INUSE_BITS | FOOTER_BIT))
#   define GET_CHUNKSIZE(p)                                                    \
        (((p)->head & CINUSE_BIT)                                              \
         ? GET_ALLOCCHUNKSIZE(p)                                               \
         : GET_FREECHUNKSIZE(p))

/* Treat space at ptr +/- offset as a chunk */
#define CHUNK_PLUS_OFFSET(p, s)  ((chunk_t *) (((char*) (p)) + (s)))
#define CHUNK_MINUS_OFFSET(p, s) ((chunk_t *) (((char*) (p)) - (s)))

/* Ptr to next or previous physical malloc_chunk. */
#define NEXT_CHUNK(p) ((chunk_t *) ((char *) (p) + GET_CHUNKSIZE(p)))
#define PREV_CHUNK(p) ((chunk_t *) (((char *) (p)) - ((p)->prev_foot)))

/* Get/set size at footer */
#define SET_FOOT(p, s)  (((chunk_t *) ((char *) (p) + (s)))->prev_foot = (s))

/* Set size, PINUSE bit, and foot */
#define SET_SIZE_AND_PINUSE_OF_FREE_CHUNK(p, s)                                \
    ((p)->head = (s | PINUSE_BIT), SET_FOOT(p, s))

/* Set size, PINUSE bit, foot, and clear next PINUSE */
#define SET_FREE_WITH_PINUSE(p, s, n)                                          \
    (CLEAR_PINUSE(n), SET_SIZE_AND_PINUSE_OF_FREE_CHUNK(p, s))

/* ---------------------- Overlaid data structures ----------------------- */

/*
  When chunks are not in use, they are treated as nodes of either
  lists or trees.

  "Small"  chunks are stored in circular doubly-linked lists, and look
  like this:

    chunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Size of previous chunk                            |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    `head:' |             Size of chunk, in bytes                         |P|
      mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Forward pointer to next chunk in list             |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Back pointer to previous chunk in list            |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Unused space (may be 0 bytes long)                .
            .                                                               .
            .                                                               |
nextchunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    `foot:' |             Size of chunk, in bytes                           |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  Larger chunks are kept in a form of bitwise digital trees (aka
  tries) keyed on chunksizes.  Because malloc_tree_chunks are only for
  free chunks greater than 256 bytes, their size doesn't impose any
  constraints on user chunk sizes.  Each node looks like:

    chunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Size of previous chunk                            |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    `head:' |             Size of chunk, in bytes                         |P|
      mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Forward pointer to next chunk of same size        |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Back pointer to previous chunk of same size       |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Pointer to left child (child[0])                  |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Pointer to right child (child[1])                 |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Pointer to parent                                 |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             bin index of this chunk                           |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |             Unused space                                      .
            .                                                               |
nextchunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    `foot:' |             Size of chunk, in bytes                           |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  Each tree holding treenodes is a tree of unique chunk sizes.  Chunks
  of the same size are arranged in a circularly-linked list, with only
  the oldest chunk (the next to be used, in our FIFO ordering)
  actually in the tree.  (Tree members are distinguished by a non-null
  parent pointer.)  If a chunk with the same size an an existing node
  is inserted, it is linked off the existing node using pointers that
  work in the same way as fd/bk pointers of small chunks.

  Each tree contains a power of 2 sized range of chunk sizes (the
  smallest is 0x100 <= x < 0x180), which is is divided in half at each
  tree level, with the chunks in the smaller half of the range (0x100
  <= x < 0x140 for the top nose) in the left subtree and the larger
  half (0x140 <= x < 0x180) in the right subtree.  This is, of course,
  done by inspecting individual bits.

  Using these rules, each node's left subtree contains all smaller
  sizes than its right subtree.  However, the node at the root of each
  subtree has no particular ordering relationship to either.  (The
  dividing line between the subtree sizes is based on trie relation.)
  If we remove the last chunk of a given size from the interior of the
  tree, we need to replace it with a leaf node.  The tree ordering
  rules permit a node to be replaced by any leaf below it.

  The smallest chunk in a tree (a common operation in a best-fit
  allocator) can be found by walking a path to the leftmost leaf in
  the tree.  Unlike a usual binary tree, where we follow left child
  pointers until we reach a null, here we follow the right child
  pointer any time the left one is null, until we reach a leaf with
  both child pointers null. The smallest chunk in the tree will be
  somewhere along that path.

  The worst case number of steps to add, find, or remove a node is
  bounded by the number of bits differentiating chunks within
  bins. Under current bin calculations, this ranges from 6 up to 21
  (for 32 bit sizes) or up to 53 (for 64 bit sizes). The typical case
  is of course much better.
*/

typedef struct treechunk_s treechunk_t;
struct treechunk_s {
    /* The first four fields must be compatible with malloc_chunk */
    csize_t prev_foot;
    csize_t head;
    treechunk_t *fd;
    treechunk_t *bk;

    treechunk_t *child[2];
    treechunk_t *parent;
    bindex_t index;
};

/* A little helper macro for trees */
#define LEFTMOST_CHILD(t) ((t)->child[0] != 0 ? (t)->child[0] : (t)->child[1])

/* ---------------------------- malloc_state ----------------------------- */

/*
   A malloc_state holds all of the bookkeeping for a space.
   The main fields are:

  Top
    The topmost chunk of the currently active segment. Its size is
    cached in topsize.  The actual size of topmost space is
    topsize+TOP_FOOT_SIZE, which includes space reserved for adding
    fenceposts and segment records if necessary when getting more
    space from the system.  The size at which to autotrim top is
    cached from mparams in trim_check, except that it is disabled if
    an autotrim fails.

  Designated victim (dv)
    This is the preferred chunk for servicing small requests that
    don't have exact fits.  It is normally the chunk split off most
    recently to service another small request.  Its size is cached in
    dvsize. The link fields of this chunk are not maintained since it
    is not kept in a bin.

  SmallBins
    An array of bin headers for free chunks.  These bins hold chunks
    with sizes less than MIN_LARGE_SIZE bytes. Each bin contains
    chunks of all the same size, spaced 8 bytes apart.  To simplify
    use in double-linked lists, each bin header acts as a malloc_chunk
    pointing to the real first node, if it exists (else pointing to
    itself).  This avoids special-casing for headers.  But to avoid
    waste, we allocate only the fd/bk pointers of bins, and then use
    repositioning tricks to treat these as the fields of a chunk.

  TreeBins
    Treebins are pointers to the roots of trees holding a range of
    sizes. There are 2 equally spaced treebins for each power of two
    from TREE_SHIFT to TREE_SHIFT+16. The last bin holds anything
    larger.

  Bin maps
    There is one bit map for small bins ("smallmap") and one for
    treebins ("treemap).  Each bin sets its bit when non-empty, and
    clears the bit when empty.  Bit operations are then used to avoid
    bin-by-bin searching -- nearly all "search" is done without ever
    looking at bins that won't be selected.  The bit maps
    conservatively use 32 bits per map word, even if on 64bit system.
    For a good description of some of the bit-based techniques used
    here, see Henry S. Warren Jr's book "Hacker's Delight" (and
    supplement at http://hackersdelight.org/). Many of these are
    intended to reduce the branchiness of paths through malloc etc, as
    well as to reduce the number of memory locations read or written.

  Segments
    A list of segments headed by an embedded malloc_segment record
    representing the initial space.

  Address check support
    The least_addr field is the least address ever obtained from
    MORECORE or MMAP. Attempted frees and reallocs of any address less
    than this are trapped (unless INSECURE is defined).

  Magic tag
    A cross-check field that should always hold same value as mparams.magic.

  Flags
    Bits recording whether to use MMAP, locks, or contiguous MORECORE

  Statistics
    Each space keeps track of current and maximum system memory
    obtained via MORECORE or MMAP.

  Locking
    If USE_LOCKS is defined, the "mutex" lock is acquired and released
    around every public call using this mspace.
*/

/* Bin types, widths and sizes */
#define NSMALLBINS        (32U)
#define NTREEBINS         (32U)
#define SMALLBIN_SHIFT    (3U)
#define TREEBIN_SHIFT     (8U)
#define MIN_LARGE_SIZE    (1U << TREEBIN_SHIFT)
#define MAX_SMALL_SIZE    (MIN_LARGE_SIZE - 1)
/*************************************************************************
 * }}} Stolen from dlmalloc
 *************************************************************************/


/*************************************************************************
 * {{{ Region data structure
 *************************************************************************/
typedef struct region_s region_t;
struct region_s {
    region_t *prev;
    region_t *next;
    int alloc_idx; /* allocator index: only for shared pool allocator */
    csize_t size;  /* size of this region, including region_t header */
};

/* 각 region의 구조는 다음과 같다.
 *
 * -----------------------------------------------
 * region header: TB_ALIGN64(sizeof(region_t)) bytes
 * -----------------------------------------------
 * ...(allocated chunks)...
 * -----------------------------------------------
 * footer chunk:
 *      8 bytes
 *          prev_foot: (used by the previous chunk)
 *          head:      (region의 크기) | FOOTER_BIT
 * -----------------------------------------------
 *
 * 새로 할당된 region은 init_new_region에서 초기화된다.
 */

#define REGION_HEADER_SIZE (TB_ALIGN64(sizeof(region_t)))
#define REGION_FOOTER_SIZE (CSIZE_T_SIZE * 2U)

/* Region size <=> max. chunk size */
#define REGION2CHUNKSIZE(size) \
    ((size) - REGION_HEADER_SIZE - REGION_FOOTER_SIZE)

#define CHUNK2REGIONSIZE(size) \
    ((size) + REGION_HEADER_SIZE + REGION_FOOTER_SIZE)

/* Pointer to a region <=> pointer to its first chunk */
#define REGION2CHUNK(region) \
    ((chunk_t *) ((char *) (region) + REGION_HEADER_SIZE))
#define CHUNK2REGION(chunk) \
    ((region_t *) ((char *) (chunk) - REGION_HEADER_SIZE))

#define REGION2FOOTER(region, region_size) \
    ((chunk_t *) ((char *) (region) + (region_size) - REGION_FOOTER_SIZE))

typedef struct alloc_s {
    allocator_t super;

    region_alloctype_t alloctype;

    /* 1. allocator index in the shared pool allocator set
     * 2. allocator index in the ROOT allocator set
     * 3. allocator index in the region allocator pool
     */
    int alloc_idx;

    /* 전체 크기의 합이므로 매우 커질 수 있어서 size_t를 사용한다.
     * (64bit machine에서 64bit임.)
     */
    uint64_t total_size;   /* total size of all allocated regions */
    uint64_t total_used;   /* total used size (sum of all used chunks) */
    uint64_t total_used_max;  /* max total used size (sum of all used chunks) */

    /* Regions: dummy header node in a circular doubly linked list. */
    region_t regions;

    binmap_t smallmap;
    binmap_t treemap;
    csize_t dvsize;
    chunk_t *dv;
    /* XXX:smallbins 의 경우, chunk_t 구조에서 실제 사용하는 것은 fd, bk
     *     두개 뿐이다. 그래서 원래는 chunk_t 자체의 배열을 써야하는 것을
     *     메모리 사용량을 줄이기 위한 편법으로 아래와 같이 썼다.
     *     일반적인 경우, 2 * n + 1 이 n번째 smallbins의 fd가 되고,
     *     2 * n + 2 가 n번째 smallibs의 bk가 된다. (모두 같은 비율로 shift
     *     된다.)
     */
    chunk_t *smallbins[(NSMALLBINS+2)*2];
    treechunk_t *treebins[NTREEBINS];

} alloc_t;

/*************************************************************************
 * }}} Region data structure
 *************************************************************************/


/*************************************************************************
 * {{{ Stolen from dlmalloc
 *************************************************************************/
/* ---------------------------- Indexing Bins ---------------------------- */

#define IS_SMALL(s)         (((s) >> SMALLBIN_SHIFT) < NSMALLBINS)
#define SMALL_INDEX(s)      ((s)  >> SMALLBIN_SHIFT)
#define SMALL_INDEX2SIZE(i) ((unsigned int)((i)  << SMALLBIN_SHIFT))

/* addressing by index. See above about smallbin repositioning */
#define SMALLBIN_AT(alloc, i)                                                  \
    ((chunk_t *)((char*)&((alloc)->smallbins[(i) * 2])))
#define TREEBIN_AT(alloc,i)     (&((alloc)->treebins[i]))

/* assign tree index for size S to variable I */
#if defined(__GNUC__) && defined(i386)
#define COMPUTE_TREE_INDEX(S, I)                                               \
    do {                                                                       \
        csize_t X = S >> TREEBIN_SHIFT;                                        \
        if (X == 0)                                                            \
            I = 0;                                                             \
        else if (X > 0xFFFF)                                                   \
            I = NTREEBINS-1;                                                   \
        else {                                                                 \
            unsigned int K;                                                    \
                                                                               \
            __asm__("bsrl %1,%0\n\t" : "=r" (K) : "rm"  (X));                  \
            I =  (bindex_t)((K << 1) + ((S >> (K + (TREEBIN_SHIFT-1)) & 1)));  \
        }                                                                      \
    } while(0)
#else
#define COMPUTE_TREE_INDEX(S, I)                                               \
    do {                                                                       \
        csize_t X = S >> TREEBIN_SHIFT;                                        \
        if (X == 0)                                                            \
            I = 0;                                                             \
        else if (X > 0xFFFF)                                                   \
            I = NTREEBINS - 1;                                                 \
        else {                                                                 \
            unsigned int Y = (unsigned int) X;                                 \
            unsigned int N = ((Y - 0x100) >> 16) & 8;                          \
            unsigned int K = (((Y <<= N) - 0x1000) >> 16) & 4;                 \
            N += K;                                                            \
            N += K = (((Y <<= K) - 0x4000) >> 16) & 2;                         \
            K = 14 - N + ((Y <<= K) >> 15);                                    \
            I = (K << 1) + ((S >> (K + (TREEBIN_SHIFT - 1)) & 1));             \
        }                                                                      \
    } while(0)
#endif

/* Bit representing maximum resolved size in a treebin at i */
#define BIT_FOR_TREE_INDEX(i) \
    (i == NTREEBINS-1)? (CSIZE_T_BITSIZE-1) : (((i) >> 1) + TREEBIN_SHIFT - 2)

/* Shift placing maximum resolved bit in a treebin at i as sign bit */
#define LEFTSHIFT_FOR_TREE_INDEX(i)                                            \
    ((i == NTREEBINS-1) ? 0 :                                                  \
     ((CSIZE_T_BITSIZE-1) - (((i) >> 1) + TREEBIN_SHIFT - 2)))

/* The size of the smallest chunk held in bin with index i */
#define MINSIZE_FOR_TREE_INDEX(i)                                              \
    (((size_t)(1U) << (((i) >> 1) + TREEBIN_SHIFT)) |                          \
    (((size_t)((i) & 1U)) << (((i) >> 1U) + TREEBIN_SHIFT - 1)))

/* ------------------------ Operations on bin maps ----------------------- */

/* bit corresponding to given index */
#define IDX2BIT(i)              ((binmap_t)(1) << (i))

/* Mark/Clear bits with given index */
#define MARK_SMALLMAP(alloc,i)      ((alloc)->smallmap |=  IDX2BIT(i))
#define CLEAR_SMALLMAP(alloc,i)     ((alloc)->smallmap &= ~IDX2BIT(i))
#define SMALLMAP_IS_MARKED(alloc,i) ((alloc)->smallmap &   IDX2BIT(i))

#define MARK_TREEMAP(alloc,i)       ((alloc)->treemap  |=  IDX2BIT(i))
#define CLEAR_TREEMAP(alloc,i)      ((alloc)->treemap  &= ~IDX2BIT(i))
#define TREEMAP_IS_MARKED(alloc,i)  ((alloc)->treemap  &   IDX2BIT(i))

/* index corresponding to given bit */
#include "tb_bits.h"

/* isolate the least set bit of a bitmap */
#define LEAST_BIT(x)         ((x) & -(x))

/* mask with all bits to left of least bit of x on */
#define LEFT_BITS(x)         (((x)<<1) | -((x)<<1))

/* ----------------------- Runtime Check Support ------------------------- */

/*
  For security, the main invariant is that malloc/free/etc never
  writes to a static address other than malloc_state, unless static
  malloc_state itself has been corrupted, which cannot occur via
  malloc (because of these checks). In essence this means that we
  believe all pointers, sizes, maps etc held in malloc_state, but
  check all of those linked or offsetted from other embedded data
  structures.  These checks are interspersed with main code in a way
  that tends to minimize their run-time cost.

  When FOOTERS is defined, in addition to range checking, we also
  verify footer fields of inuse chunks, which can be used guarantee
  that the mstate controlling malloc/free is intact.  This is a
  streamlined version of the approach described by William Robertson
  et al in "Run-time Detection of Heap-based Overflows" LISA'03
  http://www.usenix.org/events/lisa03/tech/robertson.html The footer
  of an inuse chunk holds the xor of its mstate and a random seed,
  that is checked upon calls to free() and realloc().  This is
  (probablistically) unguessable from outside the program, but can be
  computed by any code successfully malloc'ing any chunk, so does not
  itself provide protection against code that has already broken
  security through some other means.  Unlike Robertson et al, we
  always dynamically check addresses of all offset chunks (previous,
  next, etc). This turns out to be cheaper than relying on hashes.
*/

    /* Set CINUSE bit and PINUSE bit of next chunk */
#   define SET_INUSE(p, s, chunkbits)                                          \
      ((p)->head = (((p)->head & PINUSE_BIT) | s | CINUSE_BIT | (chunkbits)),  \
       ((chunk_t *) (((char*) (p)) + (s)))->head |= PINUSE_BIT)

    /* Set CINUSE and PINUSE of this chunk and PINUSE of next chunk */
#   define SET_INUSE_AND_PINUSE(p, s, chunkbits)                               \
        do {                                                                   \
            CHUNK_PLUS_OFFSET(p, s)->head |= PINUSE_BIT;                       \
            (p)->head = (s | PINUSE_BIT | CINUSE_BIT | (chunkbits));           \
        } while (0)

    /* Set size, CINUSE and PINUSE bit of this chunk */
#   define SET_SIZE_AND_PINUSE_OF_INUSE_CHUNK(p, s, chunkbits)                 \
      ((p)->head = (s | PINUSE_BIT | CINUSE_BIT | (chunkbits)))

/* ----------------------- Operations on smallbins ----------------------- */

/*
  Various forms of linking and unlinking are defined as macros.  Even
  the ones for trees, which are very long but have very short typical
  paths.  This is ugly but reduces reliance on inlining support of
  compilers.
*/

/* Link a free chunk into a smallbin  */
#define insert_small_chunk(alloc, P, S)                                        \
    do {                                                                       \
        bindex_t I  = SMALL_INDEX(S);                                          \
        chunk_t *B = SMALLBIN_AT(alloc, I);                                    \
        chunk_t *F = B;                                                        \
        if (!SMALLMAP_IS_MARKED(alloc, I))                                     \
            MARK_SMALLMAP(alloc, I);                                           \
        else                                                                   \
            F = B->fd;                                                         \
        B->fd = P;                                                             \
        F->bk = P;                                                             \
        P->fd = F;                                                             \
        P->bk = B;                                                             \
    } while(0)

/* Unlink a chunk from a smallbin  */
#define unlink_small_chunk(alloc, P, S)                                        \
    do {                                                                       \
        chunk_t *F = P->fd;                                                    \
        chunk_t *B = P->bk;                                                    \
        bindex_t I = SMALL_INDEX(S);                                           \
        if (F == B)                                                            \
            CLEAR_SMALLMAP(alloc, I);                                          \
        F->bk = B;                                                             \
        B->fd = F;                                                             \
    } while(0)

/* Unlink the first chunk from a smallbin */
#define unlink_first_small_chunk(alloc, B, P, I)                               \
    do {                                                                       \
        chunk_t *F = P->fd;                                                    \
        if (B == F)                                                            \
            CLEAR_SMALLMAP(alloc, I);                                          \
        B->fd = F;                                                             \
        F->bk = B;                                                             \
    } while(0)

/* Replace dv node, binning the old one */
/* Used only when dvsize known to be small */
#define replace_dv(alloc, P, S)                                                \
    do {                                                                       \
        csize_t DVS = (alloc)->dvsize;                                         \
        if (DVS != 0) {                                                        \
            chunk_t *DV = (alloc)->dv;                                         \
            insert_small_chunk(alloc, DV, DVS);                                \
        }                                                                      \
        (alloc)->dvsize = S;                                                   \
        (alloc)->dv = P;                                                       \
    } while(0)

/* ------------------------- Operations on trees ------------------------- */

/* Insert chunk into tree */
static inline void
insert_large_chunk(alloc_t *alloc,
                   treechunk_t *chunk, csize_t chunk_size)
{
    treechunk_t **head;
    bindex_t idx;

    COMPUTE_TREE_INDEX(chunk_size, idx);
    head = TREEBIN_AT(alloc, idx);
    chunk->index = idx;
    chunk->child[0] = chunk->child[1] = NULL;
    if (!TREEMAP_IS_MARKED(alloc, idx)) {
        MARK_TREEMAP(alloc, idx);
        *head = chunk;
        chunk->parent = (treechunk_t *) head;
        chunk->fd = chunk->bk = chunk;
    }
    else {
        treechunk_t *target = *head;
        csize_t std = chunk_size << LEFTSHIFT_FOR_TREE_INDEX(idx);
        for (;;) {
            if (GET_CHUNKSIZE(target) != chunk_size) {
                treechunk_t **cur;

                cur = &(target->child[(std >> (CSIZE_T_BITSIZE - 1)) & 1]);
                std <<= 1;
                if (*cur != NULL) {
                    target = *cur;
                }
                else {
                    *cur = chunk;
                    chunk->parent = target;
                    chunk->fd = chunk->bk = chunk;
                    break;
                }
            }
            else {
                treechunk_t *next = target->fd;

                target->fd = next->bk = chunk;
                chunk->fd = next;
                chunk->bk = target;
                chunk->parent = NULL;
                break;
            }
        }
    }
}

/*
  Unlink steps:

  1. If x is a chained node, unlink it from its same-sized fd/bk links
     and choose its bk node as its replacement.
  2. If x was the last node of its size, but not a leaf node, it must
     be replaced with a leaf node (not merely one with an open left or
     right), to make sure that lefts and rights of descendents
     correspond properly to bit masks.  We use the rightmost descendent
     of x.  We could use any other leaf, but this is easy to locate and
     tends to counteract removal of leftmosts elsewhere, and so keeps
     paths shorter than minimally guaranteed.  This doesn't loop much
     because on average a node in a tree is near the bottom.
  3. If x is the base of a chain (i.e., has parent links) relink
     x's parent and children to x's replacement (or null if none).
*/

static inline void
unlink_large_chunk(alloc_t *alloc, treechunk_t *chunk)
{
    treechunk_t *parent = chunk->parent;
    treechunk_t *prev;
    if (chunk->bk != chunk) {
        treechunk_t *next = chunk->fd;
        prev = chunk->bk;
        next->bk = prev;
        prev->fd = next;
    }
    else {
        treechunk_t **prev_ptr;
        if (((prev = *(prev_ptr = &(chunk->child[1]))) != 0) ||
            ((prev = *(prev_ptr = &(chunk->child[0]))) != 0)) {
            treechunk_t **cur_ptr;
            while ((*(cur_ptr = &(prev->child[1])) != 0) ||
                   (*(cur_ptr = &(prev->child[0])) != 0)) {
                prev = *(prev_ptr = cur_ptr);
            }
            *prev_ptr = 0;
        }
    }
    if (parent != 0) {
        treechunk_t **head = TREEBIN_AT(alloc, chunk->index);
        if (chunk == *head) {
            if ((*head = prev) == 0)
                CLEAR_TREEMAP(alloc, chunk->index);
        }
        else {
            if (parent->child[0] == chunk)
                parent->child[0] = prev;
            else
                parent->child[1] = prev;
        }
        if (prev != 0) {
            treechunk_t *child;
            prev->parent = parent;
            if ((child = chunk->child[0]) != 0) {
                prev->child[0] = child;
                child->parent = prev;
            }
            if ((child = chunk->child[1]) != 0) {
                prev->child[1] = child;
                child->parent = prev;
            }
        }
    }
}

/* Relays to large vs small bin operations */

#define insert_chunk(alloc, chunk, size)                                       \
    do {                                                                       \
        if (IS_SMALL(size))                                                    \
            insert_small_chunk(alloc, chunk, size);                            \
        else {                                                                 \
            treechunk_t *TP = (treechunk_t *)(chunk);                          \
            insert_large_chunk(alloc, TP, size);                               \
        }                                                                      \
    } while(0)

#define unlink_chunk(alloc, chunk, size)                                       \
    do {                                                                       \
        if (IS_SMALL(size))                                                    \
            unlink_small_chunk(alloc, chunk, size);                            \
        else {                                                                 \
            treechunk_t *TP = (treechunk_t *)(chunk);                          \
            unlink_large_chunk(alloc, TP);                                     \
        }                                                                      \
    } while(0)
/*************************************************************************
 * }}} Stolen from dlmalloc
 *************************************************************************/

/*************************************************************************
 * {{{ Internal API
 *************************************************************************/


typedef struct alloc_parent_s {
    allocator_t super;

    int alloc_type;
    int child_cnt;
    int shared_child_cnt; /* 프로세스 내에서 공용으로 사용되는 child cnt */

    tb_thread_mutex_t *child_mutexs; /* 프로세스 내의 공용 child allocator를 위함 */

    alloc_t children[1]; /* array of [0..(child_cnt-1)] */
} alloc_parent_t;

#define ALLOC_PARENT_SIZE(child_cnt)                                              \
    TB_ALIGN64( offsetof(alloc_parent_t, children) +                              \
                (child_cnt) * TB_ALIGN64(sizeof(alloc_t) ))

chunk_t *malloc_internal(alloc_t *alloc, uint64_t reqsize);

chunk_t *realloc_internal(alloc_t *alloc, chunk_t *chunk, uint64_t reqsize);

void free_internal(alloc_t *alloc, chunk_t *chunk,
                   tb_bool_t skip_fc);
/*************************************************************************
 * }}} Internal API
 *************************************************************************/

static inline void
chunk_struct_dump(dstream_t *dstream, chunk_t *chunk)
{
    dprint(dstream, "----------------------------------------------------------------------\n");
    dprint(dstream, "Chunk Struct Dump  0x%X\n", chunk);

    if (PINUSE(chunk)) {
        dprint(dstream, "size: %lld cinuse: %d pinuse: %d footer: %d\n",
                        GET_CHUNKSIZE(chunk), CINUSE(chunk), 1, FOOTER(chunk));
    }
    else {
        dprint(dstream, "size: %lld cinuse: %d pinuse: %d footer: %d "
                        "prev-chunk size: %lld\n", GET_CHUNKSIZE(chunk),
                        CINUSE(chunk), 0, FOOTER(chunk), chunk->prev_foot);
    }

    dprint(dstream, "----------------------------------------------------------------------\n");
}
#endif /* _ALLOC_TYPES_H */
