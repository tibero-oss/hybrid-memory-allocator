/**
 * @file    alloc_dbginfo.h
 * @brief   Debug information for allocator.
 *
 * @author
 * @version \$Id$
 *
 * allocator의 debuggin support를 위한 각종 내용이 들어있다.
 */

#ifndef _ALLOC_DBGINFO_H
#define _ALLOC_DBGINFO_H

#include "list.h"
#include "dstream.h"
#include "allocator.h"

#ifndef _ALLOC_DBGINFO_T
#define _ALLOC_DBGINFO_T
typedef struct alloc_dbginfo_s alloc_dbginfo_t;
#endif /* _ALLOC_DBGINFO_T */

/* SEWOONG
void alloc_tracedump(dstream_t *dstream, allocator_t *allocator);
void alloc_tracedump_internal(dstream_t *dstream, allocator_t *allocator,
                              int level, tb_bool_t summary);
                              */

/*************************************************************************
 * {{{ _ALLOC_USE_DBGINFO
 *************************************************************************/
#ifdef _ALLOC_USE_DBGINFO
/* _ALLOC_USE_DBGINFO를 사용하면, 할당받는 모든 memory들에 debuggin을 위한
 * chunk를 붙인다.
 * chunk의 내용으론, 아래 나오는 alloc_dbginfo_t 의 내용과 더불어,
 * memory overrun를 검사하기 위해, 할당받는 공간의 앞뒤로
 * _ALLOC_REDZONE_SIZE (8byte) 만큼의 redzone을 둔다. (alloc할 때 특정 값
 * 으로 세팅한 뒤, free할 때 검사하는 방식으로 memory overrun을 검사한다.
 *
 * 따라서 _ALLOC_USE_DBGINFO를 사용했을 때, 할당하는 메모리는 실제 다음과
 * 같은 모양이 된다.
 *
 *               +------------------------------------+ <------------+
 *               |           alloc_dbginfo_t          |              |
 *               |          (32byte or 48byte)        |              |
 *               +------------------------------------+           allocator
 *               |         FRONT_REDZONE (8byte)      |           가 할당한
 *               +------------------------------------+ <-+       공간
 *     --------->|                                    |   |          |
 *  실제 돌려주는                   .                   실제         |
 *  주소                            .                   사용하는     |
 *                                  .                   공간         |
 *               |                                    |   |          |
 *               +------------------------------------+ <-+          |
 *               |          REAR_REDZONE (8byte)      |              |
 *               +------------------------------------+ <------------+
 */

#define _FILE_AND_LINE_VAR , file, line

#define _ALLOC_REDZONE_SIZE 8

/* alloc_dbginfo_t
 *   allocator에게 할당받는 모든 memory chunk의 앞에 붙는 header정보이다.
 *   나중에 추적을 위해, 그 chunk가 언제(time), 어디서(file, line), 얼마만한
 *   크기로(size) 불렸는지를 같이 저장한다.
 *   그 외에, 모든 memory들을 추적하기 위한 link와, parent allocator를 위한
 *   포인터가 저장된다.
 *
 *   sizeof(alloc_dbginfo_t) = 32byte (if 32bit OS)
 *                           = 48byte (if 32bit OS)
 *   이다.
 */
struct alloc_dbginfo_s {
#ifdef TB_DEBUG
    allocator_t *allocator;
    int64_t time;           /* malloc을 부른 시간을 저장 */
#endif

    const char *file;       /* malloc을 부른 code 의 위치를 저장 (filename) */
    int64_t size;           /* 요청한 memory의 크기 */
    uint32_t line;          /* malloc을 부른 code 의 위치를 저장 (line) */
};

#define _ALLOC_DBGINFO_HEADER_SIZE                                             \
    (TB_ALIGN64(sizeof(alloc_dbginfo_t)) + _ALLOC_REDZONE_SIZE)
#define _ALLOC_DBGINFO_FOOTER_SIZE                                             \
    (_ALLOC_REDZONE_SIZE)

#define _ALLOC_DBGINFO2MEM(ptr)                                                \
    (void *) ((char *) (ptr) + _ALLOC_DBGINFO_HEADER_SIZE)

#define _ALLOC_MEM2DBGINFO(ptr)                                                \
    (void *) ( (alloc_dbginfo_t *)                                             \
               ((char *) (ptr) - _ALLOC_DBGINFO_HEADER_SIZE) )

#define _ALLOC_ADD_DBGINFO_SIZE(bytes)                                         \
    ((bytes) + _ALLOC_DBGINFO_HEADER_SIZE + _ALLOC_DBGINFO_FOOTER_SIZE)
#define _ALLOC_GET_ALIGN_OFFSET(size)   ((8 - (size & 7)) & 7)

#define _ALLOC_FRONT_REDZONE_PATTERN 0xa7
#define _ALLOC_REAR_REDZONE_PATTERN  0x9d

#define TB_THR_ASSERT_UNLOCK(cond)                                             \
    do {                                                                       \
        if (!(cond)) {                                                         \
            if (allocator->use_mutex)                                          \
            MUTEX_UNLOCK(&allocator->mutex);                                   \
            TB_THR_ASSERT(!#cond);                                             \
        }                                                                      \
    } while (0)

#ifndef INT64_MAX
#define INT64_MAX  0x7fffffffffffffffLL
#endif

/**
 * @brief   할당받은 공간에 dbginfo를 설정하고, redzone을 만들어준다.
 *
 * @param[in]   allocator
 * @param[in]   ptr       할당받은 공간의 시작 주소
 * @param[in]   bytes     할당받기를 원하는 크기
 * @param[in]   file
 * @param[in]   line
 *
 * @warning ptr은 언제나 alloc받은 시작 위치이어야 한다.
 *          (사용자가 실제 돌려받게 되는 위치와는 차이가 있다!)
 */
static inline void
alloc_init_redzone(allocator_t *allocator, void *ptr, int64_t bytes,
                   const tb_bool_t valloc, const char *file, int line)
{
    alloc_dbginfo_t *dbginfo;

    dbginfo = (alloc_dbginfo_t *) (ptr);

    dbginfo->file = file;
    dbginfo->line = (uint32_t) line;
#ifdef TB_DEBUG
    dbginfo->allocator = allocator;
    dbginfo->time = (int64_t) time(NULL);
#endif
    dbginfo->size = (int64_t) bytes;

    /* front redzone setting */
    memset((char *) ptr + TB_ALIGN64(sizeof(alloc_dbginfo_t)),
           _ALLOC_FRONT_REDZONE_PATTERN,
           _ALLOC_REDZONE_SIZE);

    /* rear redzone setting */
    memset((char *) ptr +
           TB_ALIGN64(sizeof(alloc_dbginfo_t)) + _ALLOC_REDZONE_SIZE + bytes,
           _ALLOC_REAR_REDZONE_PATTERN,
           _ALLOC_REDZONE_SIZE);
} /* alloc_init_redzone */

/**
 * @brief   redzone이 깨지지 않았는지 검사해 주는 루틴
 *
 * @param[in]   allocator
 * @param[in]   ptr
 *
 * @warning ptr 은 할당받은 공간의 시작 주소이다! (dbginfo_t 의 구조체가
 *          시작하는 위치이지, 사용자에게 돌려주는 위치의 시작점이 아니다!)
 */
static inline void
alloc_check_redzone(allocator_t *allocator, void *ptr, tb_bool_t clear)
{
    alloc_dbginfo_t *dbginfo;
    int i;
    int64_t size;
    char flag;
    char *front;
    char *rear;

    dbginfo = (alloc_dbginfo_t *) (ptr);

#ifdef TB_DEBUG
    TB_THR_ASSERT(dbginfo->allocator == allocator);
#endif

    size = dbginfo->size;
    TB_THR_ASSERT(size < INT64_MAX);
    TB_THR_ASSERT(size >= 0);
    flag = 0;

    /* front redzone check */
    front = (char *) ptr + TB_ALIGN64(sizeof(alloc_dbginfo_t));
    for (i = 0; i < _ALLOC_REDZONE_SIZE; i++)
        flag |= (front[i] ^ _ALLOC_FRONT_REDZONE_PATTERN);

    /* rear redzone check */
    rear = (char *) ptr + TB_ALIGN64(sizeof(alloc_dbginfo_t))
         + _ALLOC_REDZONE_SIZE + size;
    for (i = 0; i < _ALLOC_REDZONE_SIZE; i++)
        flag |= (rear[i] ^ _ALLOC_REAR_REDZONE_PATTERN);

    if (flag != 0) {
        fprintf(stderr, "REDZONE CHECK ERROR. (malloc from:"
                "filename=%s, line=%d, size="LLD"\n",
                dbginfo->file, dbginfo->line, dbginfo->size);

        fprintf(stderr, "FRONT REDZONE:\n");
        /* SEWOONG
         * fdump_data(stderr, front, _ALLOC_REDZONE_SIZE);
         */
        fprintf(stderr, "REAR REDZONE:\n");
        /* SEWOONG
         * fdump_data(stderr, rear, _ALLOC_REDZONE_SIZE);
         */

        TB_THR_ASSERT_UNLOCK(false);
    }

#ifdef TB_DEBUG
    /* front redzone 에서 rear redzone 까지 garbage 값으로 밀어버린다. */
    if (clear)
        memset(front, 0xCA, _ALLOC_REDZONE_SIZE * 2 + size);
#endif
} /* alloc_check_redzone */

static inline tb_bool_t
redzone_is_valid(allocator_t *allocator, void *ptr, tb_bool_t clear)
{
    alloc_dbginfo_t *dbginfo;
    int i;
    int64_t size;
    char flag;
    char *front;
    char *rear;

    dbginfo = (alloc_dbginfo_t *) (ptr);

#ifdef TB_DEBUG
    if (dbginfo->allocator != allocator) {
        fprintf(stderr, "alloc_check_redzone_simple failed. "
                "dbginfo->allocator: %p allocator: %p",
                dbginfo->allocator, allocator);
        return false;
    }
#endif

    size = dbginfo->size;

    flag = 0;

    /* front redzone check */
    front = (char *) ptr + TB_ALIGN64(sizeof(alloc_dbginfo_t));
    for (i = 0; i < _ALLOC_REDZONE_SIZE; i++)
        flag |= (front[i] ^ _ALLOC_FRONT_REDZONE_PATTERN);

    /* rear redzone check */
    rear = (char *) ptr + TB_ALIGN64(sizeof(alloc_dbginfo_t))
         + _ALLOC_REDZONE_SIZE + size;

    for (i = 0; i < _ALLOC_REDZONE_SIZE; i++)
        flag |= (rear[i] ^ _ALLOC_REAR_REDZONE_PATTERN);

    if (flag != 0) {
        fprintf(stderr, "REDZONE CHECK ERROR. (malloc from:"
                "filename=%s, line=%d, size="LLD"\n",
                dbginfo->file, dbginfo->line, dbginfo->size);

        fprintf(stderr, "FRONT REDZONE:\n");
        /* SEWOONG
         * fdump_data(stderr, front, _ALLOC_REDZONE_SIZE);
         */
        fprintf(stderr, "REAR REDZONE:\n");
        /* SEWOONG
         * fdump_data(stderr, rear, _ALLOC_REDZONE_SIZE);
         */

        return false;
    }

    return true;

} /* alloc_check_redzone */

#else /* not _ALLOC_USE_DBGINFO */

#define _FILE_AND_LINE_VAR

#define alloc_init_redzone(allocator, ptr, bytes, valloc, file, line) (void) 0

#define _ALLOC_DBGINFO2MEM(ptr) ((char *) (ptr))
#define _ALLOC_MEM2DBGINFO(ptr) ((void *) (ptr))
#define _ALLOC_ADD_DBGINFO_SIZE(bytes) (bytes)

#endif /* not _ALLOC_USE_DBGINFO */
/*************************************************************************
 * }}} _ALLOC_USE_DBGINFO
 *************************************************************************/
#endif /* _ALLOC_DBGINFO_H */
