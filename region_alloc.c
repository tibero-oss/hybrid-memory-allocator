/**
 * @file    region_alloc.c
 * @brief   *NEW* region memory allocator.
 *
 * Based on Doug Lea's malloc v2.8.2.
 *
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


#include "tb_common.h"
#include "thr_assert.h"
#include "tb_log.h"
#include "dstream.h"
#include "tb_mutex.h"
#include "iparam.h"

#include "allocator.h"

#include "pmem_buddy.h"

#include "alloc_dbginfo.h"
#include "alloc_dbginfo_dump.h"
#include "alloc_efence.h"
#include "alloc_types.h"
#include "alloc_alg.h"

/*************************************************************************
 * The allocator interface
 *************************************************************************/

static void *region_malloc(allocator_t *allocator, int64_t bytes, const char *file, int line);
static void *region_valloc(allocator_t *allocator, int64_t bytes, const char *file, int line);
static void *region_calloc(allocator_t *allocator, int64_t bytes, const char *file, int line);
static void *region_realloc(allocator_t *allocator, void *ptr, int64_t bytes, const char *file, int line);
static void region_free(allocator_t *allocator, void *ptr , const char *file, int line);
static void region_delete(allocator_t *allocator, const char *file_delete, int line_delete);

static void region_tracedump(dstream_t *dstrem, allocator_t *allocator, const char *indent);
static void region_throw(allocator_t *allocator);

static const allocator_desc_t region_allocator_desc = {
    region_malloc,
    region_valloc,
    region_calloc,
    region_realloc,
    region_free,
    region_delete,
    region_tracedump,
    region_throw
};

alloc_parent_t *ROOT_ALLOC_PARENT = NULL;
alloc_t *ROOT_ALLOC = NULL;

allocator_t *SYSTEM_ALLOC = NULL;
allocator_t *PMEM_SYSTEM_ALLOC = NULL;

tb_bool_t use_root_allocator = true;
tb_bool_t force_malloc_use = false;

/*************************************************************************
 * Static function declarations
 *************************************************************************/

static void region_chunk_dump(dstream_t *ds, chunk_t *chunk);

static void region_previous_chunk_dump(dstream_t *ds, allocator_t *allocator,
                                       chunk_t *target_chunk);

static allocator_t *
sys_region_allocator_init(alloc_t *alloc, allocator_t *parent,
                               tb_bool_t use_mutex, tb_bool_t use_pmem,
                               const char *file, int line);

/*************************************************************************
 * {{{ Allocator constructor/destructor
 *************************************************************************/

void
allocator_setname(allocator_t *allocator, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsnprintf(allocator->name, ALLOCATOR_NAME_MAXLEN, fmt, args);
    va_end(args);
} /* allocator_setname */

void static
root_allocator_new(void)
{
    alloc_t *child;
    char *ptr;
    int child_cnt;
    int n, idx, p_size, alloc_size;
    region_t *region;
    chunk_t *bin;

    if (IPARAM(_ROOT_ALLOCATOR_CNT) == 0) {
        use_root_allocator = false;
        return;
    }

    child_cnt  = IPARAM(_ROOT_ALLOCATOR_CNT);

    /* root allocator 가 mmap 대신 malloc을 쓸 것인지 결정 */
    force_malloc_use = IPARAM(_FORCE_NATIVE_ALLOC_USE);

    p_size = ALLOC_PARENT_SIZE(child_cnt);
    /* mutex은 공용 root allocator들만 잡는다. */
    alloc_size = p_size
               + TB_ALIGN64(sizeof(tb_thread_mutex_t)) * child_cnt;
    ROOT_ALLOC_PARENT = (alloc_parent_t *) malloc(alloc_size);

    ROOT_ALLOC_PARENT->super.alloc_owner_id = (int)tb_get_thrid();
    ROOT_ALLOC_PARENT->super.alloc_type = ALLOC_TYPE_REGION_ROOT;
    ROOT_ALLOC_PARENT->super.desc = NULL;
    strcpy(ROOT_ALLOC_PARENT->super.name, "(ROOT parent allocator)");
    ROOT_ALLOC_PARENT->super.file = __FILE__;
    ROOT_ALLOC_PARENT->super.line = (uint32_t)__LINE__;
    ROOT_ALLOC_PARENT->super.vcode = ALLOCATOR_VCODE;

    /* Initialize the allocator. */
    MUTEX_INIT(&(ROOT_ALLOC_PARENT->super.mutex));

    ROOT_ALLOC_PARENT->super.parent = NULL;
    INIT_LIST_HEAD(&(ROOT_ALLOC_PARENT->super.child));

    ROOT_ALLOC_PARENT->child_cnt = child_cnt;
    ROOT_ALLOC = ROOT_ALLOC_PARENT->children;

    ROOT_ALLOC_PARENT->child_mutexs =
        (tb_thread_mutex_t *)
        ((char *) ROOT_ALLOC_PARENT + ALLOC_PARENT_SIZE(child_cnt));

    for (n = 0; n < child_cnt; n++) {
        child = &ROOT_ALLOC[n];

        child->super.alloc_owner_id = (int)tb_get_thrid();
        child->super.logging = false;

        child->super.alloc_type = ALLOC_TYPE_REGION_ROOT;
        child->super.desc = &region_allocator_desc;
        sprintf(child->super.name, "(SYSTEM ROOT allocator #%d)", n);
        child->super.file = __FILE__;
        child->super.line = (uint32_t)__LINE__;
        child->super.vcode = ALLOCATOR_VCODE;

        /* parent의 mutex을 사용하므로, 여기는 false로 한다. */
        child->super.use_mutex = false;
        /* 아래는 사용하진 않지만 일단 init은 해 둔다. */
        MUTEX_INIT(&(child->super.mutex));

        child->super.vcode = ALLOCATOR_VCODE;

        MUTEX_INIT(&(ROOT_ALLOC_PARENT->child_mutexs[n]));

        child->super.parent = &(ROOT_ALLOC_PARENT->super);
        INIT_LIST_HEAD(&child->super.child);
        list_add_tail(&child->super.link, &ROOT_ALLOC_PARENT->super.child);

        child->alloctype= REGION_ALLOC_ROOT;
        child->alloc_idx = n;
        child->total_size = 0;
        child->total_used = 0;

        region = &(child->regions);
        region->prev = region->next = region;

        child->smallmap = 0;
        child->treemap = 0;
        child->dvsize = 0;
        child->dv = NULL;

        for (idx = 0; idx < 32; idx++) {
            bin = SMALLBIN_AT(child, idx);
            bin->fd = bin->bk = bin;

            child->treebins[idx] = NULL;
        }

        if (IPARAM(_ROOT_ALLOCATOR_RESERVED_SIZE) > 0) {
            ptr = tb_malloc(&(child->super), IPARAM(_ROOT_ALLOCATOR_RESERVED_SIZE));
            ptr = tb_realloc(&(child->super), ptr, 1);
        }
    }
} /* root_allocator_new */

allocator_t *
system_allocator_new(tb_bool_t use_pmem, const char *file, int line)
{
    return region_allocator_new_internal(NULL, true, use_pmem, file, line);
} /* system_allocator_new */

static allocator_t *
sys_region_allocator_init(alloc_t *alloc, allocator_t *parent,
                          tb_bool_t use_mutex, tb_bool_t use_pmem,
                          const char *file, int line)
{
    region_t *region;
    chunk_t *bin;
    int idx;

    alloc->super.alloc_owner_id = (int)tb_get_thrid();
    alloc->super.logging = false;

    if (use_pmem)
        alloc->super.alloc_type = ALLOC_TYPE_REGION_PMEM;
    else
        alloc->super.alloc_type = ALLOC_TYPE_REGION_SYS;

    alloc->super.desc = &region_allocator_desc;

    if (parent) {
        if (use_pmem)
            strcpy(alloc->super.name, "(PMEM region allocator)");
        else
            strcpy(alloc->super.name, "(region allocator)");
    }
    else {
        if (use_pmem)
            strcpy(alloc->super.name, "(PMEM SYSTEM ALLOC)");
        else
            strcpy(alloc->super.name, "(SYSTEM ALLOC)");
    }

    alloc->super.file = file;
    alloc->super.line = (uint32_t)line;

    /* file_delete 및 line_delete 는 allocator delete 시 double free detection
       을 위해 사용. 기본값으로는 file, line 과 동일한 값을 넣어준다.
       따라서 file == file_delete, line == line_delete 면 allocator delete 가
       일어나지 않은 것 */
    alloc->super.file_delete = file;
    alloc->super.line_delete = (uint32_t)line;
    alloc->super.vcode = ALLOCATOR_VCODE;

    /* Initialize the allocator. */
    alloc->super.use_mutex = use_mutex;
    if (use_mutex) {
        MUTEX_INIT(&(alloc->super.mutex));
    }

    if (parent != NULL && parent->use_mutex)
        MUTEX_LOCK(&parent->mutex);

    alloc->super.parent = parent;
    INIT_LIST_HEAD(&(alloc->super.child));
    if (parent != NULL)
        list_add_tail(&alloc->super.link, &parent->child);

    if (use_pmem)
        alloc->alloctype = REGION_ALLOC_PMEM;
    else
        alloc->alloctype = REGION_ALLOC_SYS;
    alloc->alloc_idx = 0;
    alloc->total_size = 0;
    alloc->total_used = 0;

    region = &(alloc->regions);
    region->prev = region->next = region;

    alloc->smallmap = 0;
    alloc->treemap = 0;
    alloc->dvsize = 0;
    alloc->dv = NULL;

    for (idx = 0; idx < 32; idx++) {
        bin = SMALLBIN_AT(alloc, idx);
        bin->fd = bin->bk = bin;

        alloc->treebins[idx] = NULL;
    }

    if (parent != NULL && parent->use_mutex)
        MUTEX_UNLOCK(&parent->mutex);

    return &alloc->super;
}

/**
 * @brief   PGA를 사용하기 위한 region allocator를 생성한다.
 *
 * @param[in]   parent
 * @param[in]   scope
 * @param[in]   use_mutex    mutex을 사용할지 말지 결정
 *
 * system malloc을 사용하는 region allocator를 생성하는 부분
 * 즉 PGA을 위한 allocator를 생성하는 부분이다.
 */
allocator_t *
region_allocator_new_internal(allocator_t *parent,
                              tb_bool_t use_mutex,
                              tb_bool_t use_pmem,
                              const char *file, int line)
{
    alloc_t *alloc;

    /* Allocator 초기화. */
    alloc = malloc(sizeof(alloc_t));

    if (alloc == NULL)
        return NULL;

    sys_region_allocator_init(alloc, parent, use_mutex, use_pmem, file,
                                   line);
    return &alloc->super;
} /* real_sys_region_allocator_new */


#ifdef TB_DEBUG
static void
region_redzone_check(allocator_t *allocator, region_t *region)
{
    void *ptr;
    chunk_t *first, *footer, *chunk, *next;

    first = REGION2CHUNK(region);
    footer = REGION2FOOTER(region, region->size);

    /* Footer chunk: region->size를 나타내며, CINUSE_BIT, FOOTER_BIT가
     * 켜져 있어야 한다. (PINUSE_BIT는 0, 1 모두 가능.)
     */
    TB_THR_ASSERT((footer->head & ~PINUSE_BIT) ==
              (region->size | CINUSE_BIT | FOOTER_BIT));

    /* Check the PINUSE bit of the first chunk. */
    TB_THR_ASSERT(first->head & PINUSE_BIT);

    /* Check the chunks between first and footer. */
    for (chunk = first; chunk < footer; chunk = next) {
        next = NEXT_CHUNK(chunk);
        TB_THR_ASSERT((chunk->head & FOOTER_BIT) == 0);
        if (CINUSE(chunk)) {
            TB_THR_ASSERT(PINUSE(next));

            ptr = CHUNK2MEM(chunk);
            alloc_check_redzone(allocator, ptr, true);
        }
        else {
            /* chunk is free: next->prev_size is meaningful. */
            TB_THR_ASSERT(next->prev_foot == GET_CHUNKSIZE(chunk));

            /* Free chunks cannot be adjacent. */
            TB_THR_ASSERT(CINUSE(next));
            TB_THR_ASSERT(PINUSE(next) == 0);
        }

        chunk = next;
    }
} /* region_redzone_check */
#else /* no TB_DEBUG */
#define region_redzone_check(allocator, region) (void) 0
#endif


/**
 * @brief   region allocator를 삭제한다.
 *
 * @param[in]   allocator
 *
 * region allocator를 삭제하는 모듈.
 * @warning 당연한 이야기지만, allocator는 이후엔 사용할 수 없다!
 *          allocator를 삭제하면 그 allocator가 할당했던 모든 memory까지
 *          반납한다.
 *          또한 자기 자신뿐 아니라, 자신의 child allocator까지 모두 찾아서
 *          삭제한다.
 */
static void
region_delete(allocator_t *allocator, const char *file_delete, int line_delete)
{
    allocator_t *parent, *child;
    alloc_t *alloc = (alloc_t *) allocator;
    region_t *head, *region, *next;

    TB_THR_ASSERT(allocator->vcode == ALLOCATOR_VCODE);
    allocator->vcode = 0;

    allocator->file_delete = file_delete;
    allocator->line_delete = (uint32_t)line_delete;

    /* Parent alloator의 child allocator list에서 자기 자신 제거. */
    parent = alloc->super.parent;

    /* Free child allocators. */
    while (!list_empty(&(alloc->super.child))) {
        child = list_entry(alloc->super.child.next, allocator_t, link);
        allocator_delete(child);
    }

    if (parent != NULL) {
        if (parent->use_mutex)
            MUTEX_LOCK(&parent->mutex);

        list_del(&alloc->super.link);

        if (parent->use_mutex)
            MUTEX_UNLOCK(&parent->mutex);
    }

    /* Free each region. */
    switch (alloc->alloctype) {
    case REGION_ALLOC_SYS:
    case REGION_ALLOC_PMEM:
        head = &(alloc->regions);
        for (region = head->next; region != head; region = next) {
            next = region->next;
#ifdef TB_DEBUG
            region_redzone_check(allocator, region);
#endif
            if (alloc->alloctype == REGION_ALLOC_PMEM)
                pbuddy_free(region, region->size);
            else if (use_root_allocator)
                tb_root_free(region);
            else
                free_page(region, region->size);
        }

        if (allocator->use_mutex)
            MUTEX_DESTROY(&(allocator->mutex));

        free(alloc);
        break;

    case REGION_ALLOC_ROOT:
        TB_THR_ASSERT(!"Cannot delete ROOT region allocator!");

    default:
        TB_THR_ASSERT(!"Unknown allocator!");
    }
} /* region_delete */


void
get_allocator_status(allocator_t *allocator,
                     uint64_t *total_size, uint64_t *used_size)
{
    alloc_t *alloc = (alloc_t *) allocator;

    *total_size = alloc->total_size;
    *used_size = alloc->total_used;
}

static void
dump_allocator_struct_info(allocator_t *alloc)
{
    const char *filename, *s;
    fprintf(stderr, "[allocator info]\n");
    fprintf(stderr, "alloc_type: %d\n", alloc->alloc_type);
    fprintf(stderr, "alloc_desc: %p\n", alloc->desc);
    fprintf(stderr, "name: %s\n", alloc->name);
    fprintf(stderr, "logging: %d\n", alloc->logging);
    filename = alloc->file;
    for (s = filename; *s; s++) if (IS_PATH_TOK(*s)) filename = s + 1;
    fprintf(stderr, "file: %s\n", filename);
    fprintf(stderr, "line: %d\n", alloc->line);
    fprintf(stderr, "parent: %p\n", alloc->parent);
    fprintf(stderr, "alloc_owner_id: %d\n", alloc->alloc_owner_id);
}

/* allocator에서 malloc한 region들만 골라 지워준다.
 * (region_delete 와 거의 동일하나, allocator 자체는 남아 있다는 점이 다르다)
 */
void
allocator_cleanup(allocator_t *allocator)
{
    alloc_t *alloc = (alloc_t *) allocator;
    region_t *head, *region, *next;
    chunk_t *bin;
    int idx;
    allocator_t *alloc_child = NULL;

    if (allocator->vcode != ALLOCATOR_VCODE) {
        fprintf(stderr, "[%d] "
                "Trying to cleanup an invalid allocator!\n"
                "This allocator was created at %s:%d\n"
                "vcode: 0x%X valid code: 0x%X\n",
                (int)tb_get_thrid(), allocator->file, allocator->line,
                allocator->vcode, ALLOCATOR_VCODE);

        /* file_delete 및 line_delete 는 allocator_delete 가 발생한 위치를
         * 저장하는 변수로, allocator 생성시에는 각각 allocator->file 과
         * allocator->line 으로 초기화되도록 하였음 */
        if (strcmp(allocator->file_delete, allocator->file) != 0 ||
            allocator->line_delete != allocator->line) {
            fprintf(stderr, "[%d] "
                    "This is a deleted allocator!\n"
                    "This allocator was deleted at %s:%d\n",
                    (int)tb_get_thrid(), allocator->file_delete,
                    allocator->line_delete);
        }

        TB_THR_ASSERT(!"allocator->vcode != ALLOCATOR_VCODE)");
    }

    if (alloc->super.use_mutex)
        MUTEX_LOCK(&alloc->super.mutex);

    /* CHILD allocator 는 없어야 되겠죠? -.- */
    if(!list_empty(&(alloc->super.child))) {
        alloc_child = list_entry(alloc->super.child.next, allocator_t, link);
        fprintf(stderr, "alloc_child 0x%p\n", alloc_child);
        dump_allocator_struct_info(alloc_child);
    }
    TB_THR_ASSERT(list_empty(&(alloc->super.child)));

    /* Free each region. */
    switch (alloc->alloctype) {
    case REGION_ALLOC_SYS:
    case REGION_ALLOC_PMEM:
        /* 이미 cleanup 된 allocator라면 아래 작업들도 생략해 주자. */
        if (alloc->total_size == 0)
            break;

        head = &(alloc->regions);
        for (region = head->next; region != head; region = next) {
            next = region->next;
            region_redzone_check(allocator, region);
            if (alloc->alloctype == REGION_ALLOC_PMEM)
                pbuddy_free(region, region->size);
            else if (use_root_allocator)
                tb_root_free(region);
            else
                free_page(region, region->size);
        }

        alloc->total_size = 0;
        alloc->total_used = 0;

        region = &(alloc->regions);
        region->prev = region->next = region;

        alloc->smallmap = 0;
        alloc->treemap = 0;
        alloc->dvsize = 0;
        alloc->dv = NULL;

        for (idx = 0; idx < 32; idx++) {
            bin = SMALLBIN_AT(alloc, idx);
            bin->fd = bin->bk = bin;

            alloc->treebins[idx] = NULL;
        }

        break;

    case REGION_ALLOC_ROOT:
        TB_THR_ASSERT(!"Cannot cleanup ROOT region allocator!");

    default:
        TB_THR_ASSERT(!"Unknown allocator!");
    }

    if (alloc->super.use_mutex)
        MUTEX_UNLOCK(&alloc->super.mutex);

} /* allocator_cleanup */

void static
root_allocator_delete()
{
    region_t *head, *region, *next;
    alloc_t *child;
    int n;

    if (!use_root_allocator || !ROOT_ALLOC_PARENT)
        return;

    if (SYSTEM_ALLOC != NULL) {
        allocator_delete(SYSTEM_ALLOC);
        SYSTEM_ALLOC = NULL;
    }

    TB_THR_ASSERT(ROOT_ALLOC_PARENT->super.vcode == ALLOCATOR_VCODE);
    ROOT_ALLOC_PARENT->super.vcode = 0;

    for (n = 0; n < ROOT_ALLOC_PARENT->child_cnt; n++) {
        child = &ROOT_ALLOC[n];

        TB_THR_ASSERT(child->super.vcode == ALLOCATOR_VCODE);
        child->super.vcode = 0;

        head = &(child->regions);
        for (region = head->next; region != head; region = next)
        {
            next = region->next;
#ifdef TB_DEBUG
            region_redzone_check((allocator_t *)child, region);
#endif
            free_page(region, region->size);
        }

        MUTEX_DESTROY(&(child->super.mutex));
        MUTEX_DESTROY(&(ROOT_ALLOC_PARENT->child_mutexs[n]));

        list_del(&child->super.link);
    }

    MUTEX_DESTROY(&(ROOT_ALLOC_PARENT->super.mutex));

    free(ROOT_ALLOC_PARENT);
    ROOT_ALLOC_PARENT = NULL;
} /* root_allocator_new */

void tballoc_init_internal(const char *file, int line)
{
    root_allocator_new();
    if (pbuddy_alloc_init(IPARAM(PMEM_DIR), NULL, IPARAM(PMEM_MAX_SIZE), IPARAM(PMEM_ALLOC_SIZE)) == NULL) {
        pbuddy_alloc_destroy();
        root_allocator_delete();
        return NULL;
    }

    SYSTEM_ALLOC = system_allocator_new(false, file, line);
    PMEM_SYSTEM_ALLOC = system_allocator_new(true, file, line);
}

void tballoc_clear(void)
{
    allocator_delete(SYSTEM_ALLOC);
    SYSTEM_ALLOC = NULL;
    root_allocator_delete();

    allocator_delete(PMEM_SYSTEM_ALLOC);
    PMEM_SYSTEM_ALLOC = NULL;
    pbuddy_alloc_destroy();
}
/*************************************************************************
 * }}} Allocator constructor/destructor
 *************************************************************************/


/*************************************************************************
 * {{{ root allocator API
 *************************************************************************/
/**
 * @brief   system root allocator중 하나에서 memory를 받아오는 API
 *
 * @param[in]   bytes
 */
void *
tb_root_malloc(int64_t bytes)
{
    int idx, cnt;
    char *ptr;

    TB_THR_ASSERT1(ROOT_ALLOC_PARENT, IPARAM(_ROOT_ALLOCATOR_CNT));

    cnt = ROOT_ALLOC_PARENT->child_cnt;
    idx = (int)tb_get_thrid() % ROOT_ALLOC_PARENT->child_cnt;
    idx = mutex_array_lockany(ROOT_ALLOC_PARENT->child_mutexs, cnt, idx);

    /* idx를 저장하기 위해 맨 앞에 16byte 여유를 둔다. */
    ptr = tb_malloc(&(ROOT_ALLOC[idx].super), bytes + 16);
    MUTEX_UNLOCK(&ROOT_ALLOC_PARENT->child_mutexs[idx]);

    if (ptr == NULL)
        return NULL;

    *(int *)ptr = idx;

    return (ptr + 16);
} /* tb_root_malloc */


/**
 * @brief   할당받은 memory를 반납하는 함수
 *
 * @param[in]   allocator
 * @param[in]   ptr
 *
 * 일반적인 free와 동일하다고 생각할 수 있다.
 */
void
tb_root_free(void *in_ptr)
{
    int idx;
    char *ptr;

    ptr = in_ptr;
    ptr -= 16;
    idx = *(int *) ptr;

    TB_THR_ASSERT1(ROOT_ALLOC_PARENT, IPARAM(_ROOT_ALLOCATOR_CNT));
    TB_THR_ASSERT1((idx >= 0) && (idx < ROOT_ALLOC_PARENT->child_cnt), idx);

    MUTEX_LOCK(&ROOT_ALLOC_PARENT->child_mutexs[idx]);
    tb_free(&(ROOT_ALLOC[idx].super), ptr);
    MUTEX_UNLOCK(&ROOT_ALLOC_PARENT->child_mutexs[idx]);
} /* tb_root_free */
/*************************************************************************
 * }}} root allocator API
 *************************************************************************/


/*************************************************************************
 * {{{ Public allocator API
 *************************************************************************/

/**
 * @brief   malloc과 valloc이 거의 동일하므로 공통 루틴을 뽑아냈다.
 *
 * @param[in]   allocator
 * @param[in]   bytes
 * @param[in]   valloc      : true면 valloc, false면 일반 malloc
 */
static inline void *
region_malloc_internal(allocator_t *allocator, int64_t bytes,
                       const tb_bool_t valloc, const char *file, int line)
{
    uint64_t req_size;
    alloc_t *alloc = (alloc_t *) allocator;
    chunk_t *chunk;
    char *mem;
    csize_t chunksize;

    TB_THR_ASSERT(bytes < INT64_MAX);
    TB_THR_ASSERT(bytes >= 0);

    if (alloc->super.use_mutex)
        MUTEX_LOCK(&alloc->super.mutex);

    req_size = _ALLOC_ADD_DBGINFO_SIZE(bytes);
    req_size = REQUEST2SIZE((uint64_t)req_size);

    /* ROOT allocator 에서 malloc 을 받아올 땐, allocator idx가 추가된다. */
    if (alloc->alloctype == REGION_ALLOC_ROOT)
        req_size += TB_ALIGN64(sizeof(int));

    TB_THR_ASSERT4(req_size < MAX_CHUNK_SIZE,
                   bytes, req_size, MAX_CHUNK_SIZE, line);

    chunk = malloc_internal (alloc, req_size);

    if (chunk == NULL) {
        if (alloc->super.use_mutex)
            MUTEX_UNLOCK(&alloc->super.mutex);

        if (alloc->alloctype != REGION_ALLOC_ROOT) {
#ifdef _ALLOC_USE_DBGINFO
            TB_LOG("Out of Memory(type:%d malloc): %lld bytes (line:%d) (file:%s)",
                alloc->alloctype, bytes, line, file);
#else
            TB_LOG("Out of Memory(type:%d malloc): %lld bytes",
                alloc->alloctype, bytes);
#endif
        }
        return NULL;
    }

    SET_START_OFFSET(chunk, bytes);
    mem = CHUNK2MEM(chunk);

    chunksize = GET_CHUNKSIZE(chunk);
    alloc->total_used += chunksize;

#ifdef _ALLOC_USE_DBGINFO
    alloc_init_redzone(&(alloc->super), mem, bytes, valloc, file, line);
    mem = _ALLOC_DBGINFO2MEM(mem);
    if (valloc)
        mem += _ALLOC_GET_ALIGN_OFFSET(bytes);
#endif

    if (alloc->super.use_mutex)
        MUTEX_UNLOCK(&alloc->super.mutex);

    TB_LOG("malloc (alloc=%p, ptr=%p)", allocator, mem);
    return mem;
} /* region_malloc_internal */


/**
 * @brief   region allocator에서 memory를 할당받는 부분
 *
 * @param[in]   allocator
 * @param[in]   bytes
 *
 * 일반적인 malloc과 하는 일이 동일하다고 생각하고 사용할 수 있다.
 */
static void *
region_malloc(allocator_t *allocator, int64_t bytes, const char *file, int line)
{
    return region_malloc_internal(allocator, bytes, false, file, line);
} /* region_malloc */

static inline void *
tb_valloc_internal(allocator_t *alloc, uint bytes, const char* file, int line)
{
    int pagesize = getpagesize();
    uint req_bytes;
    int overhead_bytes;
    char *ptr, *aligned_ptr;
    void *aligned_dbginfo;
    chunk_t *chunk;

    if (bytes == 0)
        return NULL;

    /* 실제 ptr의 위치를 기록하기 위한 CSIZE_T_SIZE와
     * region_free를 속이기 위한 head (CSIZE_T_SIZE) 및 dbginfo 추가 */
    req_bytes = CSIZE_T_SIZE * 2 + _ALLOC_ADD_DBGINFO_SIZE(bytes);

    /* _ALLOC_ADD_DBGINFO_SIZE가 상수가 아닌 macro 함수 형태여서
     * overhead_bytes를 따로 계산 */
    overhead_bytes = req_bytes - bytes;

    /* page align을 맞추기 위해 pagesize 추가 */
    req_bytes += pagesize;

    ptr = (alloc->desc->func_malloc)(alloc, req_bytes, file, line);
    if (ptr == NULL)
        return NULL;

    aligned_ptr = TB_ALIGNP(ptr, pagesize);
    if (aligned_ptr - ptr < overhead_bytes)
        aligned_ptr += pagesize;

    TB_THR_ASSERT(aligned_ptr + bytes <= ptr + req_bytes);

    /* 이 부분이 valloc을 사용하지 않은 경우의 free 부분과 일치해야 함 */
    aligned_dbginfo = _ALLOC_MEM2DBGINFO(aligned_ptr);

    alloc_init_redzone(alloc, aligned_dbginfo, bytes, false, file, line);

    chunk = MEM2CHUNK(aligned_dbginfo);
    chunk->head = 0; /* valloc임을 표시 (size == 0) */
    chunk->prev_foot = (int)(ptrdiff_t)(aligned_ptr - ptr);

    return aligned_ptr;
}

/* valloc free를 지원하기 위해서는 base pointer를 구할 때
 * 항상 _ALLOC_MEM2DBGINFO_VALLOC을 써야 한다. */
#define _ALLOC_MEM2DBGINFO_VALLOC(ptr)                                         \
    _ALLOC_MEM2DBGINFO(                                                        \
        GET_CHUNKSIZE(MEM2CHUNK(_ALLOC_MEM2DBGINFO(ptr))) == 0      ?          \
        (char *)ptr - MEM2CHUNK(_ALLOC_MEM2DBGINFO(ptr))->prev_foot :          \
        ptr)

/**
 * @brief   region allocator에서 pagesize에 align된 memory를 할당받음
 *
 * malloc과 거의 동일하나, align을 맞추지 않고 돌려준다는 점만
 * 다르다. (뒤로 memory 범람이 일어나는 것을 찾기 위한 것으로,
 * 돌려받는 ptr의 주소 + 요청한 size가 딱 page의 경계가 되도록
 * 만들어서 돌려준다.)
 * align이 맞지 않기 때문에 구조체 용도로는 부적합하고,
 * string 저장용도로 사용할 때에만 사용 가능할 것이다.
 * (string 저장 용으로 buffer를 할당받고, 실수로 할당받은 것보다
 * 더 많은 공간을 쓰려할 때를 잡는 용도로 적합하다.)
 *
 * @param[in]   allocator
 * @param[in]   bytes
 */
static void *
region_valloc(allocator_t *allocator, int64_t bytes, const char *file, int line)
{
    return tb_valloc_internal(allocator, bytes, file, line);
} /* region_valloc */

/**
 * @brief   region allocator에서 memory를 할당받고 초기화하는 함수
 *
 * @param[in]   allocator
 * @param[in]   bytes
 *
 * 일반적인 calloc과 동일하다고 생각할 수 있다.
 * (내부적으론 malloc 이후 초기화하는 부분만 추가되어 있다.)
 */
static void *
region_calloc(allocator_t *allocator, int64_t bytes, const char *file, int line)
{
    void *ptr;

    ptr = region_malloc(allocator, bytes, file, line);
    if (ptr != NULL)
        memset(ptr, 0x00, bytes);

    return ptr;
} /* region_calloc */


/**
 * @brief   할당받은 memory의 크기를 바꾸는 함수
 *
 * @param[in]   allocator
 * @param[in]   ptr
 * @param[in]   bytes
 *
 * 일반적인 realloc과 동일하다고 생각할 수 있다.
 */
static void *
region_realloc(allocator_t *allocator, void *ptr, int64_t bytes, const char *file, int line)
{
    alloc_t *alloc = (alloc_t *)allocator;
    chunk_t *chunk;
    csize_t oldsize;
    csize_t newsize;
    char *mem;
    void *base = ptr;
    dstream_t *ds = &debug_dstream;

    TB_THR_ASSERT(bytes < INT64_MAX);
    TB_THR_ASSERT(bytes > 0);

    if (ptr == NULL)
        return region_malloc(allocator, bytes, file, line);

    if (alloc->super.use_mutex)
        MUTEX_LOCK(&alloc->super.mutex);

#ifdef _ALLOC_USE_DBGINFO
    base = _ALLOC_MEM2DBGINFO(ptr);

    /* XXX: 여기서는 아직 원래 내용을 copy하기 전이기 때문에,
     *      redzone check에서 memory를 닦을 수 없다. */
    alloc_check_redzone(&(alloc->super), base, false);
#endif

    oldsize = GET_CHUNKSIZE(MEM2CHUNK(base));
    if (alloc->total_used < oldsize) {
        chunk = MEM2CHUNK(base);
        region_chunk_dump(ds, chunk);
        region_previous_chunk_dump(ds, allocator, chunk);
        if (alloc->super.use_mutex)
            MUTEX_UNLOCK(&alloc->super.mutex);
        TB_THR_ASSERT2(!"alloc->total_used >= oldsize",
                       alloc->total_used, oldsize);
    }

    chunk = realloc_internal(alloc, MEM2CHUNK(base),
                             REQUEST2SIZE(_ALLOC_ADD_DBGINFO_SIZE(bytes)));

    if (chunk == NULL) {
        if (alloc->super.use_mutex)
            MUTEX_UNLOCK(&alloc->super.mutex);

#ifdef _ALLOC_USE_DBGINFO
        TB_LOG("Out of Memory(type:%d malloc): %lld bytes (line:%d) (file:%s)",
            alloc->alloctype, bytes, line, file);
#else
        TB_LOG("Out of Memory(type:%d malloc): %lld bytes",
            alloc->alloctype, bytes);
#endif
        return NULL;
    }
    mem = CHUNK2MEM(chunk);

    /* total_used는 64bit이고 chunk size는 32bit일 수 있으므로, 한줄로
     * 줄이면 오동작할 수 있음.
     */
    newsize = GET_CHUNKSIZE(chunk);
    alloc->total_used -= oldsize;
    alloc->total_used += newsize;

#ifdef _ALLOC_USE_DBGINFO
    alloc_init_redzone(&(alloc->super), mem, bytes, false, file, line);
    mem = _ALLOC_DBGINFO2MEM(mem);
#endif

    if (alloc->super.use_mutex)
        MUTEX_UNLOCK(&alloc->super.mutex);

    return mem;
} /* region_realloc */

/**
 * @brief   할당받은 memory를 반납하는 함수
 *
 * @param[in]   allocator
 * @param[in]   ptr
 *
 * 일반적인 free와 동일하다고 생각할 수 있다.
 */
static void
region_free(allocator_t *allocator, void *ptr, const char *file, int line)
{
    void *base;
    chunk_t *chunk;
    csize_t chunksize;
    alloc_t *alloc = (alloc_t *) allocator;
    dstream_t *ds = &debug_dstream;
    tb_bool_t reuse;

    if (allocator->vcode != ALLOCATOR_VCODE) {
        TB_LOG("try to free a chunk at invalid allocator. "
            "allocator name: %s ptr: %p file: %s line: %d "
            "vcode: 0x%X valid code: 0x%X\n",
            allocator->name, allocator, allocator->file, allocator->line,
            allocator->vcode, ALLOCATOR_VCODE);
        base = _ALLOC_MEM2DBGINFO_VALLOC(ptr);
        chunk = MEM2CHUNK(base);
        region_chunk_dump(ds, chunk);
        TB_THR_ASSERT(!"allocator->vcode != ALLOCATOR_VCODE)");
    }

    if (alloc->super.use_mutex)
        MUTEX_LOCK(&alloc->super.mutex);

    base = _ALLOC_MEM2DBGINFO_VALLOC(ptr);
    chunk = MEM2CHUNK(base);
    if (CINUSE(chunk) == 0) {
        /* chunk가 어떤 상태인지 정보를 남겨 두자 */
        /* chunk가 깨져 있을 수도 있으므로 hexa dump 도 남겨두자. */
        region_chunk_dump(ds, chunk);
        region_previous_chunk_dump(ds, allocator, chunk);
        if (alloc->super.use_mutex)
            MUTEX_UNLOCK(&alloc->super.mutex);
        fprintf(stderr, "Internal Error while calling 'region_free()'. "
                "file: %s line: %d\n",
                file, line);
        TB_THR_ASSERT1(!"CINUSE(MEM2CHUNK(base))", (MEM2CHUNK(base))->head);
    }

#ifdef _ALLOC_USE_DBGINFO
    if (redzone_is_valid(allocator, base, true) == false) {
        fprintf(stderr, "Invalid redzone detected. ptr: %p\n", chunk);
        region_chunk_dump(ds, chunk);
        region_previous_chunk_dump(ds, allocator, chunk);
        if (alloc->super.use_mutex)
            MUTEX_UNLOCK(&alloc->super.mutex);
        fprintf(stderr, "Internal Error while calling 'region_free()'. "
                "file: %s line: %d\n",
                file, line);
        TB_THR_ASSERT(!"invalid redzone detected");
    }
#endif

    chunksize = GET_CHUNKSIZE(MEM2CHUNK(base));
    if (alloc->total_used < chunksize) {
        region_chunk_dump(ds, chunk);
        region_previous_chunk_dump(ds, allocator, chunk);
        if (alloc->super.use_mutex)
            MUTEX_UNLOCK(&alloc->super.mutex);
        fprintf(stderr, "Internal Error while calling 'region_free()'. "
                "file: %s line: %d\n",
                file, line);
        TB_THR_ASSERT2(!"alloc->total_used >= chunksize",
                       alloc->total_used, chunksize);
    }
    alloc->total_used -= chunksize;

    /* 재사용을 위해서 실제로 해제하지 않는 경우. */
    reuse = (alloc->alloctype == REGION_ALLOC_ROOT &&
             alloc->total_size <= IPARAM(_ROOT_ALLOCATOR_RUSZE_SIZE));

    free_internal(alloc, MEM2CHUNK(base), reuse);

    if (alloc->super.use_mutex)
        MUTEX_UNLOCK(&alloc->super.mutex);

} /* region_free */

/**
 * @brief   allocator의 상태를 출력하는 함수
 *
 * @param[in]   file      출력값을 저장할 곳. (ex. stdout)
 * @param[in]   allocator
 * @param[in]   indent    allocator의 level을 출력하기 위한 indent 정보
 *
 * allocator의 상태를 출력한다. (allocator가 할당할 수 있는 memory와
 * 현재 할당한 memory등.)
 */
static void
region_tracedump(dstream_t *dstream, allocator_t *allocator, const char *indent)
{
    alloc_t *alloc = (alloc_t *) allocator;

    dprint(dstream,
           "%s  "LLU" bytes are allocated in total (used + reserved).\n"
           "%s  "LLU" bytes are actually being used.\n",
            indent, (uint64_t) alloc->total_size,
            indent, (uint64_t) alloc->total_used);

    dprint(dstream, "%s  beginning sanity check...\n", indent);
} /* region_tracedump */


/*************************************************************************
 * }}} Public allocator API
 *************************************************************************/


/*************************************************************************
 * {{{ Recovery functions
 *************************************************************************/

static void region_throw(allocator_t *allocator) { return ; }

/*************************************************************************
 * }}} Recovery function
 *************************************************************************/

/*************************************************************************
 * {{{ Tracedump & allocator check functions
 *************************************************************************/

static void
region_chunk_dump(dstream_t *ds, chunk_t *chunk) {

	uint64_t max_dump_size = 104857600;
	uint64_t dump_size;
    dprint(ds, "----------------------------------------------------------------------\n");
    dprint(ds, "Region Chunk Dump");
    chunk_struct_dump(ds, chunk);
#ifdef _ALLOC_USE_DBGINFO
    if (CINUSE(chunk))
        dbginfo_struct_dump(ds, CHUNK2MEM(chunk));
#endif
    dprint(ds, "----------------------------------------------------------------------\n");

	if (max_dump_size < GET_CHUNKSIZE(chunk)) { /* chunk size가 100MB를 넘으면 */
        dprint(ds, "Exceed Maximum Chunk Dump Size - %d: Chunk Dump Size - %d",
               max_dump_size, GET_CHUNKSIZE(chunk));
		/* 현재  chunk가 있는 page에서 4kB align을 하여 현재 page 나머지와 다음 page(4kB)까지 찍도록 */
		dump_size = 8192-((uint64_t)chunk % 4096);
        dprint(ds, "Chunk Mem Hex Dump  0x%X size %d\n",
               chunk, dump_size);
        /* TODO: dbinary_dump(ds, chunk, max_dump_size); */
        dprint(ds, "----------------------------------------------------------------------\n");
    }
    else {
        dprint(ds, "Chunk Mem Hex Dump  0x%X size %d\n",
               chunk, GET_CHUNKSIZE(chunk) + CSIZE_T_SIZE);
        /* TODO: dbinary_dump(ds, chunk, GET_CHUNKSIZE(chunk) + CSIZE_T_SIZE); */
        dprint(ds, "----------------------------------------------------------------------\n");
    }
}

void
region_previous_chunk_dump(dstream_t *ds, allocator_t *allocator,
                           chunk_t *target_chunk)
{
    chunk_t *chunk, *footer, *prev_chunk;
    alloc_t *alloc = (alloc_t *)allocator;
    region_t *head, *region;
    tb_bool_t found = false;
    dprint(ds, "Previous Chunk Dump\n");

    head = &(alloc->regions);
    for (region = head->next; region != head; region = region->next) {
        chunk = REGION2CHUNK(region);
        footer = REGION2FOOTER(region, region->size);
        if (chunk <= target_chunk && target_chunk < footer) {
            prev_chunk = NULL;
            while (1) {
                if (chunk == target_chunk) {
                    if (prev_chunk == NULL) {
                        dprint(ds, "No previous chunk.\n");
                    }
                    else {
                        region_chunk_dump(ds, prev_chunk);
                    }
                    found = true;
                }
                else if (chunk > target_chunk) {
                    dprint(ds, "Skipped target chunk.\n");
                    TB_THR_ASSERT(prev_chunk != NULL);
                    region_chunk_dump(ds, prev_chunk);
                    found = true;
                }

                prev_chunk = chunk;

                if (GET_CHUNKSIZE(chunk)==0){
                    dprint(ds, "ERROR: GET_CHUNKSIZE failed.\n");
                    break;
                }

                chunk = CHUNK_PLUS_OFFSET(chunk, GET_CHUNKSIZE(chunk));
                if (chunk == footer)
                    break;
                if (chunk > footer)
                    dprint(ds, "CRITICAL ERROR: Memory Corruption.\n");
                    break;
            }
        }

        if (found) break;
    }
}

/*************************************************************************
 * }}} Tracedump & allocator check functions
 *************************************************************************/

/**
 * @brief   하위 allocator 를 포함하여 파라미터로 받은 allocator의 메모리
 *          실사용량을 return
 *
 *          주의!!! mutex을 사용하지 않는 세션 범위의 allocator 를 대상으로
 *          실사용량을 현재 세션에서 간단하게 수집하는 함수이므로 mutex을
 *          사용하면서 쓰레드들끼리 공유하는 allocator에 적용하면 안된다.
 *
 * @param[in]   allocator
 *
 * @return  TODO:
 *
 * @warning TODO:
 */
uint64_t
get_alloc_used_size_including_childs(allocator_t *allocator)
{
    allocator_t *child;
    alloc_t *alloc = (alloc_t *) allocator;
    uint64_t total_alloc_used_size = 0;
    list_link_t *elem, *next;

    TB_THR_ASSERT(allocator->vcode == ALLOCATOR_VCODE);

    /* child allocators. */
    list_for_each_safe(elem, next, &(allocator->child)) {
        child = list_entry(elem, allocator_t, link);
        total_alloc_used_size += get_alloc_used_size_including_childs(child);
    }

    /* currunt allocator. */
    total_alloc_used_size += alloc->total_used;

    return total_alloc_used_size;

} /* get_alloc_used_size_including_childs */

uint64_t 
get_total_size(allocator_t *alloc)
{
    return ((alloc_t *)alloc)->total_size;
}

uint64_t get_total_used(allocator_t *alloc)
{
    return ((alloc_t *)alloc)->total_used;
}

uint64_t get_chunk_size(uint64_t req_size)
{
    return REQUEST2SIZE(_ALLOC_ADD_DBGINFO_SIZE(req_size));
}

/* end  f region_alloc.c */
