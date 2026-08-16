/* core/statusapi.h — GET /api/status: per-source health + credential
 * configuration. Port of server/src/routes/status.js (serializeRow +
 * summary) + utils/apiCredentials.getCredentialStatus + the layers.js
 * STRIP_LAYER_IDS filter. JSON in exact Node key order (== JSON.stringify). */
#ifndef JO_STATUSAPI_H
#define JO_STATUSAPI_H
#include "db.h"

/* Returns malloc'd {summary,apis,timestamp} (200). NULL → caller 500.
 *
 * `include_breach` appends the breach_meta catalog (~1k rows) to `apis` as
 * synthesized sources with category "breach", so the app's Sources screen can
 * show them next to real collectors. It is operator-only and the caller decides:
 * httpd.c passes opgate_check(&usr)==0. Ordinary users get the payload they
 * always got — this is why it is a parameter and not a query flag. `summary`
 * always carries breachTotal/breachMaterialized (0 when not included) so a
 * client can decode them unconditionally. */
char *statusapi_build(db_handle *db, int include_breach);

/* GET /api/status/:id — malloc'd single serializeRow object, or NULL when
 * no source has that id (caller → 404 {"error":"Source not found"}). */
char *statusapi_one(db_handle *db, const char *id);

/* layers.js STRIP_LAYER_IDS membership (static list + INTEL_SOURCE_IDS).
 * 1 if `id` should be hidden from /api/layers and /api/status; 0 for NULL.
 * Shared so /api/layers filters identically (Node imported the same set). */
int statusapi_strip_has(const char *id);

/* POST /api/status/:id/probe — one GET of the source's OWN registered endpoint,
 * recording request + response into the sources row's probe_* columns. The URL
 * comes from the registry, never from the caller, and the fetch goes through
 * http_request()'s hostgate/protocol pins; no collector credential is sent, so
 * nothing secret can land in the columns status_row() serves. Operator-only.
 * Returns malloc'd JSON; *st is the HTTP status. */
char *statusapi_probe(db_handle *db, const char *id, int *st);

/* POST /api/status/:id/consent {"consent":bool} — set probe_consent, which
 * status_row()'s `gated` flag already reads. Operator-only. */
char *statusapi_set_consent(db_handle *db, const char *id, int consent, int *st);


#endif
