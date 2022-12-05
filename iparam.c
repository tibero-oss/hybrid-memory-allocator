/**
 * @file    iparam.c
 * @brief   dummy init parameter
 *
 * @author
 * @version $Id$
 */
#include "tb_common.h"
#include "iparam.h"

int IPARAM(_TOTAL_SYS_MEM_SIZE) = (1 << 20 );
int IPARAM(_SYSTEM_MEMORY_INIT_SIZE) = 1 * 1024 * 1024;
int IPARAM(_SYSTEM_MEMORY_EXPAND_SIZE) = 4 * 1024 * 1024;
int IPARAM(_SYSTEM_ROOT_ALLOCATOR_CNT) = 4;

int IPARAM(_SYSTEM_MEMORY_RESERVED_SIZE) = 0;
int IPARAM(_SYSTEM_MEMORY_REUSE_SIZE) = 4 * 1024 * 1024;
tb_bool_t IPARAM(_FORCE_NATIVE_ALLOC_USE) = false;

tb_bool_t IPARAM(_SLEEP_ON_ASSERT) = true;

char *IPARAM(PMEM_DIR) = "/pmem/tmp";