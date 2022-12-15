/**
 * @file    allocator.h
 * @brief   header file for the generic allocator API.
 *
 * @author
 * @version \$Id$
 */

#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include "tb_common.h"
#include "list.h"
#include "thr_assert.h"
#include "iparam.h"

#ifndef _TB_THREAD_MUTEX_T
#define _TB_THREAD_MUTEX_T
typedef pthread_mutex_t tb_thread_mutex_t;
#endif /* _TB_THREAD_MUTEX_T */

/*************************************************
 * Allocator base class definition
 *************************************************/

typedef struct allocator_desc_s allocator_desc_t;

/**
 * @typedef allocator_t
 * @brief   "abstract base class" of all allocators.
 *
 * Each concrete "allocator class" is defined as follows:
 *
 * @code
 *      struct XXX_allocator_s {
 *          allocator_t super; <== base class
 *
 *          ......             <== allocator-specific information
 *      }
 * @endcode
 */

#define ALLOCATOR_NAME_MAXLEN 32

#define ALLOCATOR_VCODE 0x7A7A7A7AU

#ifndef _ALLOCATOR_T
#define _ALLOCATOR_T
typedef struct allocator_s allocator_t;
#endif /* _ALLOCATOR_T */

enum allocator_type_e {
    ALLOC_TYPE_SYSTEM               = 0,
    ALLOC_TYPE_REGION_ROOT,
    ALLOC_TYPE_REGION_SYS,
    ALLOC_TYPE_REGION_PMEM,
    ALLOC_TYPE_MAX
};
typedef enum allocator_type_e allocator_type_t;

enum region_alloctype_e {
    REGION_ALLOC_ROOT,
    REGION_ALLOC_SYS,
    REGION_ALLOC_PMEM
};
typedef enum region_alloctype_e region_alloctype_t;

struct allocator_s {
    allocator_type_t alloc_type;
    const allocator_desc_t *desc;
    char name[ALLOCATOR_NAME_MAXLEN];
    tb_bool_t logging;
    const char *file;   /* allocator를 부른 code 의 위치를 저장 (filename) */
    uint32_t line;      /* allocator를 부른 code 의 위치를 저장 (line) */
    const char *file_delete; /* allocator를 삭제한 code 의 위치를 저장 (filename) */
    uint32_t line_delete; /* allocator를 삭제한 code 의 위치를 저장 (line) */

    tb_bool_t use_mutex;
    tb_thread_mutex_t mutex;

    allocator_t *parent; /* parent allocator */
    list_link_t link;
    list_t child;

    uint32_t alloc_owner_id;

    uint32_t vcode;  /* 실제 사용 중인 allocator인지 확인하는 검증 코드 */
};

/**
 * @brief   allocator descriptor.
 *
 * - func_malloc, func_calloc, func_realloc, func_free: Self-explanatory.
 * - func_delete: Allocator destructor.
 *
 * Normally, one should use the "allocator API" macros defined below.
 *
 * Note that there exists no "func_new", because each allocator needs
 * different arguments for its constructor.  You need to call these
 * allocator-specific constructors in order to create an allocator
 * instance.
 *
 * @warning As in realloc(), func_realloc() returns NULL when reallocation
 *          is impossible.  In this case, the memory (pointed by "ptr") is
 *          NOT freed or moved.
 */
#ifndef _DSTREAM_T
#define _DSTREAM_T
typedef struct dstream_s dstream_t;
#endif
struct allocator_desc_s {
    void *(*func_malloc)(allocator_t *allocator, int64_t bytes, const char *file, int line);
    void *(*func_valloc)(allocator_t *allocator, int64_t bytes, const char *file, int line);
    void *(*func_calloc)(allocator_t *allocator, int64_t bytes, const char *file, int line);
    void *(*func_realloc)(allocator_t *allocator, void *ptr,
                          int64_t bytes, const char *file, int line);
    void (*func_free)(allocator_t *allocator, void *ptr, const char *file, int line);
    void (*func_delete)(allocator_t *allocator, const char *file_delete,
                       int line_delete);
    void (*func_tracedump)(dstream_t *dstream, allocator_t *allocator,
                           const char *indent);
    void (*func_throw)(allocator_t *allocator);
};

/*************************************************
 * Allocator API
 *************************************************/
extern allocator_t *SYSTEM_ALLOC;
extern allocator_t *PMEM_SYSTEM_ALLOC;

void tballoc_init(void);
void tballoc_clear(void);

#define region_allocator_new(parent, use_mutex)                  \
    region_allocator_new_internal(parent, use_mutex, false, __FILE__, __LINE__)

#define region_pallocator_new(parent, use_mutex)                  \
    region_allocator_new_internal(parent, use_mutex, true, __FILE__, __LINE__)

allocator_t *region_allocator_new_internal(allocator_t *parent,
                                           tb_bool_t use_mutex,
                                           tb_bool_t use_pmem,
                                           const char *file, int line);
/* Destructor. */
#define allocator_delete(allocator) \
    ( ((allocator)->desc->func_delete)(allocator, __FILE__, __LINE__) )

void allocator_cleanup(allocator_t *allocator);

void allocator_setname(allocator_t *allocator, const char *fmt, ...);
#define allocator_getname(allocator) ((allocator)->name)

#define allocator_log_on(alloc)  ((alloc)->logging = true)
#define allocator_log_off(alloc) ((alloc)->logging = false)

extern tb_bool_t use_root_allocator;
extern tb_bool_t force_malloc_use;
void *tb_root_malloc(int64_t bytes);
void tb_root_free(void *in_ptr);




/**
 * @name    Wrappers for allocator API.
 *
 * These functions return NULL when an error occurs.
 *
 * @warning tb_free() always succeeds --- there is no return code.
 *
 * @warning As in realloc(), tbx_realloc() returns NULL when reallocation
 *          is impossible.  In this case, the memory (pointed by "ptr") is
 *          NOT freed or moved.
 */

#define tb_malloc(allocator, bytes)                                            \
    _tb_malloc(allocator, bytes, __FILE__, __LINE__)
#define tb_valloc(allocator, bytes)                                            \
    _tb_valloc(allocator, bytes, __FILE__, __LINE__)
#define tb_calloc(allocator, bytes)                                            \
    _tb_calloc(allocator, bytes, __FILE__, __LINE__)
#define tb_realloc(allocator, ptr, bytes)                                      \
    _tb_realloc(allocator, ptr, bytes, __FILE__, __LINE__)
#define tb_strdup(allocator, src)                                              \
    _tb_strdup(allocator, src, __FILE__, __LINE__)
#define tb_strndup(allocator, src, n)                                          \
    _tb_strndup(allocator, src, n, __FILE__, __LINE__)
#define tb_free(allocator, ptr)                                                \
    _tb_free(allocator, ptr, __FILE__, __LINE__)

static inline void *
_tb_malloc(allocator_t *allocator, int64_t bytes, const char *file, int line)
{
    void *ptr = NULL;

    TB_THR_ASSERT(allocator != NULL);

    ptr = ((allocator)->desc->func_malloc)(allocator, bytes, file, line);

    return ptr;
} /* _tbx_malloc */

static inline void *
_tb_valloc(allocator_t *allocator, int64_t bytes, const char *file, int line)
{
    void *ptr;

    TB_THR_ASSERT(allocator != NULL);

    ptr = (allocator->desc->func_valloc)(allocator, bytes, file, line);

    return ptr;
} /* _tbx_valloc */

static inline void *
_tb_calloc(allocator_t *allocator, int64_t bytes, const char *file, int line)
{
    void *ptr;

    TB_THR_ASSERT(allocator != NULL);

    ptr = (allocator->desc->func_calloc)(allocator, bytes, file, line);

    return ptr;
} /* _tbx_calloc */

static inline void *
_tb_realloc(allocator_t *allocator, void *ptr, int64_t bytes, const char *file,
            int line)
{
    TB_THR_ASSERT(allocator != NULL);

    ptr = (allocator->desc->func_realloc)(allocator, ptr, bytes, file, line);

    return ptr;
} /* _tbx_realloc */

static inline void
_tb_free(allocator_t *allocator, void *ptr, const char *file, int line)
{
    TB_THR_ASSERT(allocator != NULL);

    (allocator->desc->func_free)(allocator, ptr, file, line);
} /* _tbx_free */

static inline char *
_tb_strdup(allocator_t *allocator, const char *src, const char *file, int line)
{
    int   len;
    char *dest;

    len = (int)strlen(src);
    dest = (char *)_tb_malloc(allocator, len + 1, file, line);

    if (dest)
        memcpy(dest, src, len + 1);

    return dest;
}

static inline char *
_tb_strndup(allocator_t *allocator, const char *src, int n, const char *file,
            int line)
{
    char *dest;

    dest = (char *)_tb_malloc(allocator, n + 1, file, line);
    if (dest) {
        memcpy(dest, src, n);
        dest[n] = '\0';
    }

    return dest;
}

/*************************************************
 * Debugging/tracedump functions
 *************************************************/

uint64_t get_total_size(allocator_t *alloc);
uint64_t get_total_used(allocator_t *alloc);
uint64_t get_alloc_used_size_including_childs(allocator_t *allocator);
uint64_t get_chunk_size(uint64_t req_size);

/* dump 로직은 포팅 대상에서 제외 */

#endif /* _ALLOCATOR_H */
