/* core/breach_index.h — self-hosted breach corpus: sharded hashed index + ingest.
 *
 * Model (docs/breach-check-pipeline.md): datasets are ingested ahead of time into
 * a prefix-sharded, SHA-1-keyed on-disk store under $JO_BREACH_DIR (default
 * <repo>/data/breach). Lookups scan a single shard — NO network, ever.
 *
 * Privacy: the anonymous lookup path returns hit/count/which-breach only (reveal=0).
 * Leaked plaintext secrets are stored AES-256-GCM encrypted at rest and are returned
 * ONLY via reveal=1, which the caller must gate behind proof that the requester OWNS
 * the identifier (SpyCloud/Constella model). Never call reveal=1 on an anonymous query. */
#ifndef JO_BREACH_INDEX_H
#define JO_BREACH_INDEX_H
#include "../third_party/cJSON.h"
#include "db.h"
#include <stddef.h>

typedef enum {
  BT_EMAIL = 0, BT_USERNAME = 1, BT_PHONE = 2, BT_PASSWORD = 3, BT_AUTO = 4
} breach_type;

breach_type  breach_type_parse(const char *s);   /* unknown/"auto" -> BT_AUTO */
const char  *breach_type_name(breach_type t);

/* Ingest one staged dump into the sharded index. type is the declared pivot;
 * BT_AUTO detects per line (email/username/phone). Password ingest expects the
 * Pwned Passwords "SHA1HEX:count" format. Returns 0 on success (sets counters),
 * <0 if the file can't be opened.
 *
 * When db != NULL and materialize != 0, each accepted datapoint is ALSO written
 * to the breach_items table via core/breach_store.c (eager intel indexing).
 * db == NULL / materialize == 0 preserves the original offline, DB-free behavior.
 *
 * When dry_run != 0 nothing is persisted (no shard writes, no bloom save, no DB) —
 * the function only tallies rows_in / rows_new for a pre-ingest projection. */
int breach_index_ingest(const char *source_id, const char *path, breach_type type,
                        unsigned long long *rows_in, unsigned long long *rows_new,
                        db_handle *db, int materialize, int dry_run);

/* Look up one value. reveal!=0 decrypts leaked plaintext into each breach entry
 * — ONLY after verifying the caller owns the identifier. *out gets a new cJSON
 * {found,count,type,breaches:[{breach,secret?}]} (password: {found,count,pwn_count,
 * compromised}). Caller cJSON_Delete(*out). Returns match count, <0 on error. */
int breach_index_lookup(breach_type type, const char *value, int reveal, cJSON **out);

/* Stable, non-reversible dedup/upsert key for a value ("<type>:<sha1prefix>") —
 * never echoes the plaintext. out should be >= 24 bytes. */
void breach_index_keyid(breach_type t, const char *value, char *out, size_t n);

#endif
