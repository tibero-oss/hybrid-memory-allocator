/**
 * @file    tb_common.h
 * @brief   common header
 *
 * @author
 * @version $Id$
 */

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

#define __BASENAME__    TB_BASENAME(__FILE__)
#define TB_BASENAME(f)      (strrchr((f), '/') ? strrchr(f, '/') + 1 : (f))

/* no operation */
#define TB_NOP do {} while (0)


#define TB_MAX(a, b)    (((a) >= (b)) ? (a) : (b))
#define TB_MIN(a, b)    (((a) <= (b)) ? (a) : (b))
#define TB_ABS(a)       (((a) < 0) ? (-(a)) : (a))
#define TB_SIGN(c)      (((c) > 0) ? (+1) : (((c) < 0) ? (-1) : (0)))

#define TB_MOD(expr, mod)       ((expr) % (mod))
#define TB_MOD_DEC(expr, mod)   (((expr) - 1 + (mod)) % (mod))
#define TB_MOD_INC(expr, mod)   (((expr) + 1) % (mod))
#define TB_MOD_DIFF(a, b, mod)  (((a) - (b) + (mod)) % (mod))
#define TB_MOD_ADD(a, b, mod)   (((a) + (b)) % (mod))

/* get conatining struct of "type" from "ptr" which is "member" of "type" */
/*
#define TB_CONTAINEROF(ptr, type, member)       \
        ((type *)((char *)(ptr) - offsetof(type, member)))
        */

/* alignment */
#define TB_ALIGN(n, m)  (((n) + ((m) - 1)) & ~((m) - 1))

#define TB_ALIGN16(n)   TB_ALIGN(n, 2)
#define TB_ALIGN32(n)   TB_ALIGN(n, 4)
#define TB_ALIGN64(n)   TB_ALIGN(n, 8)

#define TB_ALIGNP(n, m)                                                        \
        ((void *)((ptrdiff_t)((char *)(n) + ((m) - 1)) & ~((m) - 1)))

#define TB_ALIGNP16(n)  TB_ALIGNP(n, 2)
#define TB_ALIGNP32(n)  TB_ALIGNP(n, 4)
#define TB_ALIGNP64(n)  TB_ALIGNP(n, 8)

#define IS_PATH_TOK(pathch) (pathch == '/')

#define tb_bool_t unsigned char
#ifndef true
#   define true  1
#   define false 0
#endif

#define LLD "%" PRId64
#define LLU "%" PRIu64
#define LLX "%" PRIx64

#define tb_get_thrid() pthread_self()
