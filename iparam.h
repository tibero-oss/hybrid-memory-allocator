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
/* region allocator의 상위로 os로 부터 받아온 메모리를 직접 관리하고 유지하는 
 * root allocator를 사용할지 여부와 사용한다면 생성 갯수 */
extern int IPARAM(_ROOT_ALLOCATOR_CNT);
/* root allocator를 생성 시 미리 os로부터 메모리를 확보하는 크기 */
extern int64_t IPARAM(_ROOT_ALLOCATOR_RESERVED_SIZE);
/* os로 반납하지 않고 메모리를 유지하는 최대 사이즈 */
extern int64_t IPARAM(_ROOT_ALLOCATOR_RUSZE_SIZE);

/* allocator가 os로 부터 메모리를 할당 받는 최소 크기 */
extern uint64_t IPARAM(_SYSTEM_MEMORY_EXPAND_SIZE);

/* 상위 allocator가 있는 region allocator(pga, pmem)를 사용 시 최소 요청 크기 범위*/
extern uint64_t IPARAM(_REGION_ALLOC_MIN_EXPAND_LOWER_BOUND);
extern uint64_t IPARAM(_REGION_ALLOC_MIN_EXPAND_UPPER_BOUND);

/* root allocator를 사용하더라도 mmap을 사용하지 않고 malloc을 통해 메모리를 받아올지 결정 */
extern tb_bool_t IPARAM(_FORCE_NATIVE_ALLOC_USE);

/* region allocator들의 최대 요청 사이즈 */
extern uint64_t IPARAM(_MAX_REQ_MEMORY_SIZE);

/* PMEM */
/* pmem directory */
extern char *IPARAM(PMEM_DIR);
/* pmem 최대 할당 크기 */
extern uint64_t IPARAM(PMEM_MAX_SIZE);
/* 사용 가능한 pmem의 최대 크기 */
extern uint64_t IPARAM(PMEM_ALLOC_SIZE);

#endif /* _IPARAM_H */
