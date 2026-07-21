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

/* Upsert one datapoint keyed by the non-reversible keyid ("<type>:<sha1prefix>").
 * `value` is the cleartext identifier for email/username/phone and MUST be NULL
 * for passwords (hash-only — plaintext never enters the DB). `hash` is the full
 * SHA-1 hex. Batches internally; commits every BREACH_BATCH rows. 0 on success. */
int breach_store_put(breach_store *s, const char *keyid, const char *type,
                     const char *value, const char *source_id, const char *hash,
                     int has_secret, long long count);

/* Commit the final batch, rebuild the FTS index over the loaded rows, restore
 * durable pragmas, and free s. Returns 0 on success. */
int breach_store_finish(breach_store *s);

/* Search materialized breach datapoints. `q` is a full-text term matched against
 * breach_fts (value); `type` optionally filters (email/username/phone/password).
 * At least one of q/type must be non-empty. Returns a malloc'd JSON string
 * {query,type?,count,results:[{keyid,type,value,source_id,has_secret,count}]} —
 * metadata only, never leaked secrets. NULL on bad args / error. Caller frees. */
char *breach_search(db_handle *db, const char *q, const char *type, int limit);

#endif
