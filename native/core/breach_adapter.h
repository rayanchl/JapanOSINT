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

#endif
