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
extern int IPARAM(_TOTAL_SYS_MEM_SIZE);
extern int IPARAM(_SYSTEM_MEMORY_INIT_SIZE);
extern int IPARAM(_SYSTEM_MEMORY_EXPAND_SIZE);
extern int IPARAM(_SYSTEM_ROOT_ALLOCATOR_CNT);

extern int IPARAM(_SYSTEM_MEMORY_RESERVED_SIZE);
extern int IPARAM(_SYSTEM_MEMORY_REUSE_SIZE);
extern tb_bool_t IPARAM(_FORCE_NATIVE_ALLOC_USE);

extern tb_bool_t IPARAM(_SLEEP_ON_ASSERT);

extern char *IPARAM(PMEM_DIR);

#endif /* _IPARAM_H */
