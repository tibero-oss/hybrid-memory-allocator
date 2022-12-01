/**
 * @file    dstream.h
 * @brief   dummy dump stream
  *
 * @author
 * @version $Id$
 */

#ifndef _DSTREAM_H
#define _DSTREAM_H

#include <stdarg.h>
#include <stdio.h>

#ifndef _DSTREAM_T
#define _DSTREAM_T
typedef struct dstream_s dstream_t;
#endif
struct dstream_s {
    int dummy;
};

extern dstream_t debug_dstream;

void
dprint(dstream_t *dstream, char *fmt, ...);

#endif /* no _DSTREAM_T */
