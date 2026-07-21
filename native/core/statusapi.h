/* core/statusapi.h — GET /api/status: per-source health + credential
 * configuration. Port of server/src/routes/status.js (serializeRow +
 * summary) + utils/apiCredentials.getCredentialStatus + the layers.js
 * STRIP_LAYER_IDS filter. JSON in exact Node key order (== JSON.stringify). */
#ifndef JO_STATUSAPI_H
#define JO_STATUSAPI_H
#include "db.h"

/* Returns malloc'd {summary,apis,timestamp} (200). NULL → caller 500. */
char *statusapi_build(db_handle *db);

/* GET /api/status/:id — malloc'd single serializeRow object, or NULL when
 * no source has that id (caller → 404 {"error":"Source not found"}). */
char *statusapi_one(db_handle *db, const char *id);

/* layers.js STRIP_LAYER_IDS membership (static list + INTEL_SOURCE_IDS).
 * 1 if `id` should be hidden from /api/layers and /api/status; 0 for NULL.
 * Shared so /api/layers filters identically (Node imported the same set). */
int statusapi_strip_has(const char *id);

#endif
