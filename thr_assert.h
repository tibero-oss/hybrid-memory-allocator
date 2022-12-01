/**
 * @file    thr_assert.h
 * @brief   dummy assert
 *
 * @author
 * @version $Id$
 */

#ifndef _THR_ASSERT_H
#define _THR_ASSERT_H

#include <assert.h>

#define TB_THR_ASSERT(cond) assert(cond)
#define TB_THR_ASSERT1(cond, ...) assert(cond)
#define TB_THR_ASSERT2(cond, ...) assert(cond)
#define TB_THR_ASSERT3(cond, ...) assert(cond)
#define TB_THR_ASSERT4(cond, ...) assert(cond)
#define TB_THR_ASSERT5(cond, ...) assert(cond)

#if 0
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include "tb_common.h"
#include "iparam.h"
#define LOCINFO __BASENAME__, __LINE__, (char *)__func__

#define ASSERT_MSG(cond) assert_msg(cond, __BASENAME__, __LINE__)
void tb_assert_msg(char *msg, char *filename, int lineno);

#define ASSERT_MSG_DETAIL(cond, argc, filename, line, func, ...)               \
            assert_msg_detail(cond, argc, filename, line, func, __VA_ARGS__)
void tb_assert_msg_detail(char *cond, int argc,
                          char *filename, int line, char *func, ...);

#ifdef TB_DEBUG
    #define ASSERT_LOOP()    assert_loop()
#else
    #define ASSERT_LOOP()
#endif

#define THR_ASSERT_CMD  abort

void
assert_loop(void)
{
    while (IPARAM(_SLEEP_ON_ASSERT))
        usleep(500000);
}

#define TB_THR_ASSERT(cond)                                                    \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ASSERT_MSG(#cond);                                                 \
            ASSERT_LOOP();                                                     \
            THR_ASSERT_CMD();                                                  \
        }                                                                      \
    } while (0)

#define TB_THR_ASSERT1(cond, p1)/*;*/                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ASSERT_MSG_DETAIL(#cond, 1, LOCINFO, #p1, sizeof(p1), p1);         \
            ASSERT_LOOP();                                                     \
            THR_ASSERT_CMD();                                                  \
        }                                                                      \
    } while (0)

#define TB_THR_ASSERT2(cond, p1, p2)/*;*/                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ASSERT_MSG_DETAIL(#cond, 2, LOCINFO, #p1, sizeof(p1), p1, #p2, sizeof(p2), p2);\
            ASSERT_LOOP();                                                     \
            THR_ASSERT_CMD();                                                  \
        }                                                                      \
    } while (0)

#define TB_THR_ASSERT3(cond, p1, p2, p3)/*;*/                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ASSERT_MSG_DETAIL(#cond, 3, LOCINFO, #p1, sizeof(p1), p1, #p2, sizeof(p2), p2, #p3, sizeof(p3), p3);\
            ASSERT_LOOP();                                                     \
            THR_ASSERT_CMD();                                                  \
        }                                                                      \
    } while (0)

#define TB_THR_ASSERT4(cond, p1, p2, p3, p4)/*;*/                              \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ASSERT_MSG_DETAIL(#cond, 4, LOCINFO, #p1, sizeof(p1), p1, #p2, sizeof(p2), p2, #p3, sizeof(p3), p3, #p4, sizeof(p4), p4);\
            ASSERT_LOOP();                                                     \
            THR_ASSERT_CMD();                                                  \
        }                                                                      \
    } while (0)

#define TB_THR_ASSERT5(cond, p1, p2, p3, p4, p5)/*;*/                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ASSERT_MSG_DETAIL(#cond, 5, LOCINFO, #p1, sizeof(p1), p1, #p2, sizeof(p2), p2, #p3, sizeof(p3), p3, #p4, sizeof(p4), p4, #p5, sizeof(p5), p5);\
            ASSERT_LOOP();                                                     \
            THR_ASSERT_CMD();                                                  \
        }                                                                      \
    } while (0)

void
assert_msg(char *msg, char *filename, int lineno)
{
    fprintf(stderr,
            "%s:%d: Assertion fail with condition "
            "'%s' (PID=%d, pthread id=%u)\n",
            filename, lineno, msg, getpid(), (uint32_t)tb_get_thrid());
    fflush(stderr);
}

void
assert_msg_detail(char *cond, int argc,
                  char *filename, int line, char *func, ...)
{
    int i;
    va_list ap;

    va_start(ap, func);
    fprintf(stderr, "%s:%d:%s: Assert failed: %s (%d args)",
            filename, line, func, cond, argc);
    for (i = 0; i < argc; i++) {
        char *expr;
        int size;
        char val_c;
        uint32_t val_i;
        uint64_t val_d;

        expr = va_arg(ap, char *);
        size = va_arg(ap, int);
        fprintf(stderr, "\n\t%s = ", expr);
        switch (size) {
        case 1:
            val_c = (char)va_arg(ap, int);
            fprintf(stderr, "'%c' = 0x%x", val_c, val_c);
            break;

        case 2:
            val_i = va_arg(ap, uint32_t);
            fprintf(stderr, "%hd = %hu = 0x%hx", val_i, val_i, val_i);
            break;

        case 4:
            val_i = va_arg(ap, uint32_t);
            fprintf(stderr, "%d = %u = 0x%x", val_i, val_i, val_i);
            if (!strcmp(expr, "errno"))
                fprintf(stderr, " %s", strerror(errno));
            break;

        case 8:
            val_d = va_arg(ap, uint64_t);
            fprintf(stderr, LLD " = " LLU " = 0x" LLX, val_d, val_d, val_d);
            break;

        default:
            i = argc;
        }
    }
    fprintf(stderr, "\n");
    va_end(ap);

    fflush(stderr);
}
#endif

#endif /* _THR_ASSERT_H */
