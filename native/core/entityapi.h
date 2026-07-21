/* core/entityapi.h — P7 Wave 2: /api/entities/* (read paths of
 * entityStore.js). Pure SQLite over the shared entity graph; no tenant /
 * pipeline / collector-framework dependency. Single C backend (faithful
 * behaviour, not Node byte-parity). */
#ifndef JO_ENTITYAPI_H
#define JO_ENTITYAPI_H
#include "db.h"

/* GET /api/entities/stats — corpus/graph counters. malloc'd, never NULL. */
char *entityapi_stats(db_handle *db);

/* GET /api/entities/search?q&type&limit — FTS (MeCab-segmented).
 * Empty q → {"results":[]}. NULL only on a SQL/MATCH failure (caller 500). */
char *entityapi_search(db_handle *db, const char *q, const char *type, int limit);

/* GET /api/entities/:type/:id — profile. NULL if missing or type mismatch
 * (caller → 404 {"error":"not_found"}). */
char *entityapi_get(db_handle *db, const char *type, const char *id);

/* GET /api/entities/:type/:id/graph?depth — ego-network. NULL → 404. */
char *entityapi_graph(db_handle *db, const char *type, const char *id, int depth);

/* GET /api/entities/:type/:id/mentions?limit&offset — NULL → 404. */
char *entityapi_mentions(db_handle *db, const char *type, const char *id,
                         int limit, int offset);

#endif
