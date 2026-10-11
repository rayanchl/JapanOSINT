/* core/audit.h — the one shared audit-row writer.
 *
 * Extracted from tenantapi.c's private member_audit() so every tenant-scoped
 * mutation surface (cases, annotations, exports, entity merges, alert rules)
 * writes audit_events the same way instead of re-implementing it per module.
 *
 * Every row written here is CHAINED, per tenant: chain_seq = last + 1,
 * prev_hash = the previous row's row_hash ("GENESIS" for the first), and
 * row_hash = audit_row_hash() over the row — the scheme
 * tenantapi_audit_verify() re-walks. It used to leave all three NULL, and the
 * verifier only walks rows WHERE row_hash IS NOT NULL, so every tenant's chain
 * "verified" over zero rows: /api/audit/verify answered ok:true for a log
 * that could have been edited freely. Rows from before this change stay NULL
 * and the verifier now counts them out loud ("unchained") instead of skipping
 * them in silence.
 */
#ifndef JO_AUDIT_H
#define JO_AUDIT_H
#include "db.h"

/* INSERT one audit_events row. target/payload_json may be NULL (payload
 * defaults to "{}"). Never fails loudly — an audit write must not abort the
 * mutation it is describing; failures are silent by design. */
void audit_write(db_handle *db, const char *tenant_id, const char *user_id,
                 const char *action, const char *target,
                 const char *payload_json);

/* audit_write() plus the ip / ua columns (break-glass logins and requests).
 * Returns 0 when the row was written chained, 1 when it could only be written
 * UNCHAINED (the chain tail could not be locked — the record is kept, the
 * verifier reports it), -1 when nothing was written. */
int  audit_write_ex(db_handle *db, const char *tenant_id, const char *user_id,
                    const char *action, const char *target,
                    const char *payload_json, const char *ip, const char *ua);

/* row_hash = sha256 hex of the canonical JSON of the row (keys sorted:
 * action, chain_seq, id, ip, payload_json, prev_hash, target, tenant_id, ts,
 * ua, user_id; NULL columns as JSON null) — == auditChain.js. The ONE copy:
 * the writer above and tenantapi_audit_verify() both call it, so the two
 * cannot disagree about what a row hashes to. */
void audit_row_hash(const char *id, const char *tenant_id,
                    const char *user_id, const char *action, const char *target,
                    const char *payload_json, const char *ts, const char *ip,
                    const char *ua, const char *prev_hash, long chain_seq,
                    char out[65]);

#endif
