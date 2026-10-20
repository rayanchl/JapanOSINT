/* core/alert_sem.{c,h} — the vector half of semantic ("Describe it in words")
 * alert rules: predicate {"mode":"llm","nl_query":"…","threshold":0.82}.
 *
 * alertsapi accepted mode:"llm" rules for months while alert_eval skipped
 * them live and in preview — a rule that saved, listed, and never fired. They
 * now fire from the embedding pod's post-batch hook (alert_eval.c,
 * eval_semantic): an item fires a rule when cos(rule vector, item vector) >=
 * the rule's threshold AND every other term of its predicate (q, sources,
 * tags, spatial, entities) holds through the SAME matcher the live path uses.
 *
 * THE RULE VECTOR. nl_query is embedded at save time on the interactive query
 * slot (core/qembed.h) — on a detached thread, never on the event loop — and
 * stored here, keyed by rule, with the hash of the text it was made from and
 * the model that made it. A vector whose text hash no longer matches the
 * rule's nl_query, or whose model differs from the intel index's, is not
 * used: the rule is reported "not evaluable" with the reason, never matched
 * across two vector spaces. The alert sweep thread re-tries missing or stale
 * vectors every pass, so a rule saved while the embedding server was down
 * starts firing once it is back.
 *
 * LIMITS, stated in every rule response (alert_sem_status): a semantic rule
 * fires only AFTER the backfill pod embeds an item (minutes, not ingest
 * time), and an item the pod never embeds — outside JO_EMBED_SINCE_DAYS, a
 * record type outside JO_EMBED_RECORD_TYPES — is never evaluated against it.
 *
 * Inert without JO_EMBED_URL: nothing is embedded, no thread is started, and
 * the rule's status says why it cannot fire. */
#ifndef JO_ALERT_SEM_H
#define JO_ALERT_SEM_H

#include <stddef.h>
#include "db.h"
#include "../third_party/cJSON.h"

/* Query-to-document cosine is lower than document-to-document for the same
 * topic (a one-line description against a 1,000-byte item): with
 * multilingual-e5 a relevant pair sits around 0.82–0.88 and an unrelated item
 * of the same genre around 0.74–0.80. 0.82 starts at the relevant band, so a
 * new rule errs towards silence rather than a firehose; "threshold" in the
 * predicate overrides it per rule (0 < t <= 1). */
#define ALERT_SEM_THRESHOLD_DEFAULT 0.82

/* CREATE TABLE IF NOT EXISTS alert_rule_vec. Called from db_open(). */
void alert_sem_migrate(db_handle *db);

/* 1 when the predicate is a semantic rule (mode "llm" + non-empty nl_query). */
int  alert_sem_is_semantic(const cJSON *pred);
/* The predicate's threshold, or the default. */
double alert_sem_threshold(const cJSON *pred);

/* Embed rule `rule_id`'s nl_query (if it is a semantic rule and its stored
 * vector is missing, stale or from another model) and store it. 1 embedded,
 * 0 nothing to do, -1 failed (reason stored in alert_rule_vec.error). */
int  alert_sem_embed_rule(db_handle *db, const char *rule_id);
/* alert_sem_embed_rule() for every enabled semantic rule. Returns embedded. */
int  alert_sem_embed_missing(db_handle *db);
/* The same, on a detached thread with its own connection — what the save
 * path calls. No-op without JO_EMBED_URL. */
void alert_sem_embed_async(const char *rule_id);

/* The stored vector for `rule_id` when it was made from `nl_query` (text hash
 * match). 1 with *vec (malloc'd) / *dim / model filled, else 0. */
int  alert_sem_load(db_handle *db, const char *rule_id, const char *nl_query,
                    float **vec, int *dim, char *model, size_t cap);

/* The in-band status block every rule response carries for a semantic rule
 * (NULL for any other rule): {threshold, novelty_min, embedded, model,
 * embedded_at, evaluable, reason, evaluates}. */
cJSON *alert_sem_status(db_handle *db, const char *rule_id,
                        const char *predicate_json);

#endif
