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

#endif /* _THR_ASSERT_H */
