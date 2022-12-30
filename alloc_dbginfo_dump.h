/**
 * @file    alloc_dbginfo_dump.h
 * @brief   allocator dbginfo dump header
 *
 * @author
 * @version $Id$
 */

#ifndef _ALLOC_DBGINFO_DUMP_H
#define _ALLOC_DBGINFO_DUMP_H

/*{{{ Headers ----------------------------------------------------------------*/
#include "alloc_dbginfo.h"
/*---------------------------------------------------------------- Headers }}}*/

static inline void
dbginfo_struct_dump(dstream_t *dstream, alloc_dbginfo_t *dbginfo)
{
#ifdef _ALLOC_USE_DBGINFO
    const char *filename, *s;
#ifdef TB_DEBUG
    time_t timeval;
    struct tm tmbuf;
    char buf[40];
#endif

    dprint(dstream, "----------------------------------------------------------------------\n");
    dprint(dstream, "Dbginfo Struct Dump 0x%X\n", dbginfo);

    filename = dbginfo->file;
    for (s = filename; *s; s++) if (IS_PATH_TOK(*s)) filename = s + 1;

#ifdef TB_DEBUG
    timeval = (time_t) (dbginfo->time);
    localtime_r(&timeval, &tmbuf);
    strftime(buf, sizeof(buf), "%d %T", &tmbuf);
    dprint(dstream, "mem ptr: 0x%X mem size: %d loc: %s:%d time: %s)\n",
                    _ALLOC_DBGINFO2MEM(dbginfo), dbginfo->size, filename,
                    dbginfo->line, buf);
#else
    dprint(dstream, "mem ptr: 0x%X mem size: %d loc: %s:%d\n",
                    _ALLOC_DBGINFO2MEM(dbginfo), dbginfo->size, filename,
                    dbginfo->line);
#endif
    dprint(dstream, "----------------------------------------------------------------------\n");
#endif
}

#endif /* no _ALLOC_DBGINFO_DUMP_H */
