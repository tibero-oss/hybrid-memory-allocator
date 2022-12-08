/**
 * @file    iparam.c
 * @brief   dummy init parameter
 *
 * @author
 * @version $Id$
 */
#include "tb_common.h"
#include "iparam.h"

int IPARAM(_ROOT_ALLOCATOR_CNT) = 4;
int64_t IPARAM(_ROOT_ALLOCATOR_RESERVED_SIZE) = 0;
int64_t IPARAM(_ROOT_ALLOCATOR_RUSZE_SIZE) = 0;

uint64_t IPARAM(_SYSTEM_MEMORY_EXPAND_SIZE) = 4 * 1024 * 1024;

/* 상위 allocator가 있는 region allocator를 사용 시 최소 요청 크기 */
uint64_t IPARAM(_REGION_ALLOC_MIN_EXPAND_LOWER_BOUND) = 4096;
uint64_t IPARAM(_REGION_ALLOC_MIN_EXPAND_UPPER_BOUND) = 1048576;

tb_bool_t IPARAM(_FORCE_NATIVE_ALLOC_USE) = false;

/* unlimited */
uint64_t IPARAM(_MAX_REQ_MEMORY_SIZE) = 0;


char *IPARAM(PMEM_DIR) = "/pmem/tmp";
uint64_t IPARAM(PMEM_MAX_SIZE) = 1024 * 1024 * 1024;
uint64_t IPARAM(PMEM_ALLOC_SIZE) = 1024 * 1024 * 1024;