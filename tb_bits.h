/**
 * @file    tb_bits.h
 * @brief   TODO brief documentation here.
 *
 * @author
 * @version $Id$
 */

#ifndef _TB_BITS_H
#define _TB_BITS_H

#include "tb_common.h"

#if defined(__GNUC__) && defined(i386)
#define compute_bit2idx(X, I)                                                  \
    do {                                                                       \
        unsigned int J;                                                        \
        __asm__("bsfl %1,%0\n\t" : "=r" (J) : "rm" ((uint) X));                \
        I = (uint)J;                                                           \
    } while(0)

#else
#if  USE_BUILTIN_FFS
#define compute_bit2idx(X, I) I = ffs(X)-1

#else
#define compute_bit2idx(X, I)                                                  \
    do {                                                                       \
        unsigned int Y = X - 1;                                                \
        unsigned int K = Y >> (16-4) & 16;                                     \
        unsigned int N = K;        Y >>= K;                                    \
                                                                               \
        N += K = Y >> (8-3) &  8;  Y >>= K;                                    \
        N += K = Y >> (4-2) &  4;  Y >>= K;                                    \
        N += K = Y >> (2-1) &  2;  Y >>= K;                                    \
        N += K = Y >> (1-0) &  1;  Y >>= K;                                    \
        I = (uint)(N + Y);                                                     \
    } while(0)
#endif
#endif

#define set_nth_bit(ptr, nth)   ( (ptr)[(nth) >> 3] |= (1 << ((nth) & 7)) )
#define clr_nth_bit(ptr, nth)   ( (ptr)[(nth) >> 3] &= ~(1 << ((nth) & 7)) )
#define isset_nth_bit(ptr, nth) ( (ptr)[(nth) >> 3] & (1 << ((nth) & 7)) )

#endif /* no _TB_BITS_H */
