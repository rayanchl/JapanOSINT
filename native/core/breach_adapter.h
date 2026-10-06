/* core/breach_adapter.h — presents breach_items records THROUGH the intel API.
 *
 * A breach record is served as a normal intel item (same JSON envelope as
 * intelapi.c's row_to_item / list) without ever being copied into intel_items.
 * The synthetic item uid is "breach:" + breach_items.keyid; breach items are
 * reachable only via ?source=<breach> or /items/:uid, never the unfiltered feed.
 * Secrets are always redacted here — plaintext only via the operator reveal path. */
#ifndef JO_BREACH_ADAPTER_H
#define JO_BREACH_ADAPTER_H
#include "db.h"

/* Single breach record → {data:{intel-item}}. `uid` must begin "breach:".
 * NULL if not a breach uid or not found. Caller frees. */
char *breach_adapter_item_by_uid(db_handle *db, const char *uid);

/* A breach source's records → {data:[...],page,meta}, keyset-paginated by row id
 * (cursor is the opaque decimal id this function emits). `q` optional FTS over
 * the identifier value. NULL on bad args. Caller frees. */
char *breach_adapter_list(db_handle *db, const char *source_id,
                          const char *q, const char *cursor, int limit);

/* Operator-gated reveal: decrypts the leaked secret(s) for a breach uid via
 * breach_index_lookup(reveal=1) → {found,count,type,breaches:[{breach,secret?}]}.
 * Requires SECRETS_MASTER_KEY. NULL if not a breach uid / not found. Caller frees. */
char *breach_adapter_reveal_by_uid(db_handle *db, const char *uid);

/* THE route-facing reveal: requires a non-empty `reason` (400 otherwise),
 * writes the 'platform' audit row action "breach.reveal", target = uid,
 * payload {requester, requester_email?, reason} BEFORE decrypting, and reveals
 * nothing (503) if that row cannot be written. 404 for an unknown uid. Always
 * returns a JSON body and sets *status. The caller has already established
 * that the requester is a platform operator. Caller frees. */
char *breach_adapter_reveal_audited(db_handle *db, const char *uid,
                                    const char *requester_id,
                                    const char *requester_email,
                                    const char *reason, const char *ip,
                                    int *status);

#endif
