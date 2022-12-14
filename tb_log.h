/**
 * @file    alloc_log.h
 * @brief   dummy logger
 *
 * @author
 * @version $Id$
 */

#ifndef _TB_LOG_H
#define _TB_LOG_H

#include "tb_common.h"
#include <stdarg.h>

#define TB_LOG(...)                                                          \
    if (allocator->logging)                                                  \
        tb_log_write(__BASENAME__, __LINE__, __VA_ARGS__)

void
tb_log_write(char *file, int line, char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "[%s:%d] ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

#endif /* _TB_LOG_H */
