/**
 * @file    tb_mutex.h
 * @brief   dummy lock
 *
 * @author
 * @version $Id$
 */

#ifndef _TB_MUTEX_H
#define _TB_MUTEX_H

#include "tb_common.h"
#include "pthread.h"
#include "thr_assert.h"

static pthread_key_t locked_count_key;
static pthread_once_t locked_count_key_once = PTHREAD_ONCE_INIT;

static void
initialize_locked_count_key() {
    pthread_key_create(&locked_count_key, NULL);
}

#define INITIAL_TLS_LOCKED_COUNT 0
#define INIT_LOCKED_COUNT_KEY()                                                \
    do {                                                                       \
        pthread_once(&locked_count_key_once, initialize_locked_count_key);     \
        if ((pthread_getspecific(locked_count_key)) == NULL) {                 \
            pthread_setspecific(locked_count_key,                              \
                                (void *)((ptrdiff_t) INITIAL_TLS_LOCKED_COUNT));\
        }                                                                      \
    } while(0)

#define INCREASE_LOCK_COUNT()                                                  \
    do {                                                                       \
        uint64_t count = 0;                                                    \
        count = (uint64_t)((ptrdiff_t)(pthread_getspecific(locked_count_key)));\
        pthread_setspecific(locked_count_key, (void*)((ptrdiff_t)(count + 1)));\
    } while(0)

#define DECREASE_LOCK_COUNT()                                                  \
    do {                                                                       \
        uint64_t count = 0;                                                    \
        count = (uint64_t)((ptrdiff_t)(pthread_getspecific(locked_count_key)));\
        pthread_setspecific(locked_count_key, (void*)((ptrdiff_t)(count - 1)));\
    } while(0)

#ifndef _TB_THREAD_MUTEX_T
#define _TB_THREAD_MUTEX_T
typedef pthread_mutex_t tb_thread_mutex_t;
#endif /* _TB_THREAD_MUTEX_T */

#define MUTEX_INIT(mutex)       pthread_mutex_init(mutex, NULL)
#define MUTEX_DESTROY(mutex)    pthread_mutex_destroy(mutex)
#define MUTEX_LOCK(mutex)       tb_mutex_lock(mutex)
#define MUTEX_TRYLOCK(mutex)  tb_mutex_trylock(mutex)
#define MUTEX_UNLOCK(mutex)     tb_mutex_unlock(mutex)

void tb_mutex_lock(tb_thread_mutex_t *mutex) {
    INIT_LOCKED_COUNT_KEY();
    pthread_mutex_lock(mutex);
    INCREASE_LOCK_COUNT();
}

tb_bool_t tb_mutex_trylock(tb_thread_mutex_t *mutex) {
    INIT_LOCKED_COUNT_KEY();
    if (pthread_mutex_trylock(mutex) < 0)
        return false;
    INCREASE_LOCK_COUNT();
    return true;
}

int mutex_array_lockany(tb_thread_mutex_t *locks, int num, int start)
{
    int slot;

    if (start >= num)
        start = 0;

    slot = start;

    INIT_LOCKED_COUNT_KEY();
    do {
        if (MUTEX_TRYLOCK(&(locks[slot]))) {
            INCREASE_LOCK_COUNT();
            return slot;
        }
        slot = TB_MOD(slot + 1, num);
    } while (slot != start);

    MUTEX_LOCK(&(locks[slot]));
    INCREASE_LOCK_COUNT();

    return slot;

}

uint64_t check_mutex_locked() {
    uint64_t locked;
    locked = (uint64_t)((ptrdiff_t)(pthread_getspecific(locked_count_key)));
    return locked;
}

void tb_mutex_unlock(tb_thread_mutex_t *mutex) {
    TB_THR_ASSERT(check_mutex_locked() > 0);
    pthread_mutex_unlock(mutex);
    DECREASE_LOCK_COUNT();
}

#endif /* no _TB_MUTEX_H */
