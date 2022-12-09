/**
 * @file    dstream.c
 * @brief   dummy dump stream
 *
 * @author
 * @version $Id$
 */

#include "tb_common.h"

#include "dstream.h"

dstream_t debug_dstream = { 2 };

void
dprint(dstream_t *dstream, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, fmt, ap);
    va_end(ap);
}
/* end of dstream.c */
