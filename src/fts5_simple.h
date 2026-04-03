#ifndef FTS5_SIMPLE_H
#define FTS5_SIMPLE_H

#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1

#ifdef __cplusplus
extern "C" {
#endif

/* ×¢²á Simple ·Ö´ÊÆ÷ */
int sqlite3Fts5SimpleInit(sqlite3 *db, char **pzErrMsg, 
                          const sqlite3_api_routines *pApi);

#ifdef __cplusplus
}
#endif

#endif /* FTS5_SIMPLE_H */