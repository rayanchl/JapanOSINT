/* core/breach_store.h — bulk writer that materializes breach datapoints into
 * the dedicated `breach_items` table (+ external-content `breach_fts` index).
 *
 * This is the eager-materialization path: every accepted datapoint from
 * breach_index_ingest becomes a searchable intel row. It is deliberately NOT
 * the intel_items sink — breach volume (millions–billions of rows) is kept in
 * its own lean table + FTS so it never dilutes operational intel search.
 *
 * Throughput design: one prepared upsert reused per row, batched into large
 * transactions, aggressive bulk-load pragmas, and a single deferred FTS
 * `rebuild` at finish (no per-row index maintenance). See
 * docs/breach-ingest-revamp-plan.md. */
#ifndef JO_BREACH_STORE_H
#define JO_BREACH_STORE_H
#include "db.h"

typedef struct breach_store breach_store;

/* Open a bulk writer over db: sets bulk-load pragmas, prepares the keyid upsert,
 * and begins the first batch transaction. Returns NULL on error. */
breach_store *breach_store_open(db_handle *db);

/* Upsert one datapoint keyed by keyid ("<type>:<SHA1>|<source>"). The SHA-1 is
 * UNSALTED and so dictionary-reversible for identifiers — and `value` stores
 * the cleartext identifier for email/username/phone anyway (FTS substring
 * search over it is the feature; docs/breach-check-pipeline.md §2a). `value`
 * MUST be NULL for passwords (hash-only — plaintext never enters the DB).
 * `hash` is the full SHA-1 hex; `lookup_hash` the keyed HMAC hash or NULL.
 * Batches internally; commits every BREACH_BATCH rows. 0 on success. */
int breach_store_put(breach_store *s, const char *keyid, const char *type,
                     const char *value, const char *source_id, const char *hash,
                     const char *lookup_hash, int has_secret, long long count);

/* breach_items.lookup_hash (the keyed HMAC identifier hash — see
 * breach_lookup_hash() in breach_index.h; NULL when no master key was set at
 * ingest, and for passwords) plus its partial index. Idempotent; called by
 * breach_store_open(). */
void breach_store_migrate(db_handle *db);

/* Commit the final batch, rebuild the FTS index over the loaded rows, restore
 * durable pragmas, and free s. Returns 0 on success. */
int breach_store_finish(breach_store *s);

/* Search materialized breach datapoints. `q` is a full-text term matched against
 * breach_fts (value); `type` optionally filters (email/username/phone/password).
 * At least one of q/type must be non-empty. One page of `limit` (<=0 → 50,
 * max 500) rows from `offset`, in rowid order. Returns a malloc'd JSON string
 * {query,type?,count,shown,offset,limit,total,total_is_floor,has_more,
 *  next_offset?,results:[{keyid,type,value,source_id,has_secret,count}]} —
 * `total` is exact up to 100,000 matches and a floor past that
 * (total_is_floor). Metadata only, never leaked secrets. NULL on bad args /
 * error. Caller frees. */
char *breach_search(db_handle *db, const char *q, const char *type, int limit,
                    long long offset);

#endif
