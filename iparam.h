/**
 * @file    iparam.h
 * @brief   dummy init parameter
 *
 * @author
 * @version $Id$
 */
#ifndef _IPARAM_H
#define _IPARAM_H

#include "tb_common.h"

#define IPARAM(X) X

/* root allocator */
extern int IPARAM(_ROOT_ALLOCATOR_CNT);
extern int64_t IPARAM(_ROOT_ALLOCATOR_RESERVED_SIZE);
extern int64_t IPARAM(_ROOT_ALLOCATOR_RUSZE_SIZE);

/* os로 부터 받아오는 최소 크기 */
extern uint64_t IPARAM(_SYSTEM_MEMORY_EXPAND_SIZE);

/* 상위 allocator가 있는 region allocator를 사용 시 최소 요청 크기 */
extern uint64_t IPARAM(_REGION_ALLOC_MIN_EXPAND_LOWER_BOUND);
extern uint64_t IPARAM(_REGION_ALLOC_MIN_EXPAND_UPPER_BOUND);

/* root allocator를 사용하더라도 mmap을 사용하지 않고 malloc을 통해 메모리를 받아올지 결정 */
extern tb_bool_t IPARAM(_FORCE_NATIVE_ALLOC_USE);

/* region allocator들의 최대 요청 사이즈 */
extern uint64_t IPARAM(_MAX_REQ_MEMORY_SIZE);

/* PMEM */
extern char *IPARAM(PMEM_DIR);
extern uint64_t IPARAM(PMEM_MAX_SIZE);
extern uint64_t IPARAM(PMEM_ALLOC_SIZE);

#endif /* _IPARAM_H */
