/* core/dbexplorerapi.h — operator-gated DB explorer (split from w4api):
 * GET /api/db/tables, /api/db/tables/:name, /api/db/scheduler. */
#ifndef JO_DBEXPLORERAPI_H
#define JO_DBEXPLORERAPI_H
#include "db.h"
char *dbexplorer_tables(db_handle *db);
char *dbexplorer_table(db_handle *db, const char *name, int limit, int offset,
                       const char *q, const char *orderBy, const char *orderDir);
char *dbexplorer_scheduler(db_handle *db);
#endif
