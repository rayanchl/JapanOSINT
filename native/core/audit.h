/* core/audit.h — the one shared audit-row writer.
 *
 * Extracted from tenantapi.c's private member_audit() so every tenant-scoped
 * mutation surface (cases, annotations, exports, entity merges, alert rules)
 * writes audit_events the same way instead of re-implementing it per module.
 *
 * Rows written here are RAW/unchained: row_hash and chain_seq are left NULL,
 * which excludes them from tenantapi_audit_verify()'s walk (same stance as
 * break-glass logins and member_audit). Chaining is a separate concern owned
 * by the verify path; a writer that half-chains would break the walk. */
#ifndef JO_AUDIT_H
#define JO_AUDIT_H
#include "db.h"

/* INSERT one audit_events row. target/payload_json may be NULL (payload
 * defaults to "{}"). Never fails loudly — an audit write must not abort the
 * mutation it is describing; failures are silent by design. */
void audit_write(db_handle *db, const char *tenant_id, const char *user_id,
                 const char *action, const char *target,
                 const char *payload_json);

#endif
