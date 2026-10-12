/* core/service_vec.h — semantic routing over the entity-pivot service
 * catalogue.
 *
 * THE PROBLEM THIS EXISTS FOR. osint_services_list_bounded() has to fit ~1,535
 * entity-pivot services into an LLM prompt. Unbounded that text is 207,353
 * tokens against a 16,384-token context, so it degrades in announced steps:
 * every service with its description, else bare ids, else the first K ids.
 * Step 3 truncates in REGISTRY ORDER, and registry order is link order — the
 * batch-generated regional registries register first and the hand-written
 * entity services register LAST. Measured over 1,535 pivots: DNS_RECORDS was
 * #1201, IP_GEOLOCATION #1288, DOMAIN_WHOIS #1529. Truncation therefore drops
 * precisely the services a person asking "who owns example.com" wants.
 *
 * Registry order has nothing to do with the query. This module replaces it
 * with RELEVANCE: each service's `name: description` is embedded ONCE into a
 * sqlite-vec `service_vec` table (the text is static registry prose, not
 * fetched, so this costs one pass and then nothing), and at query time the
 * user's text is embedded and the K nearest services are listed WITH their
 * descriptions. A small relevant menu beats a large arbitrary one, and it fits
 * the budget with room to spare.
 *
 * WHAT IT DOES NOT DO. It never invents a service, never widens the set beyond
 * what is registered, and never silently substitutes itself: when the
 * embedding server is not configured, the index has not been built, the
 * embedding call fails, the server's model (JO_EMBED_MODEL, else GET
 * /v1/models) is not the one the index was built with, or the dimension does
 * not match, it returns NULL and
 * the caller falls back to osint_services_list_bounded() exactly as before.
 * That is the same inert-unless-configured contract core/embed_pod.c has.
 *
 * HONESTY. The returned text states in-band that the menu is the K most
 * semantically similar services out of N, so the model is never left to
 * assume it saw the whole registry — the same rule
 * osint_services_list_bounded() follows for its own bound. `note.semantic`
 * lets the pipeline report the same fact to the user.
 *
 * KEEPING THE BRIEFING AND THE VOCABULARY IN SYNC. The analysis schema's
 * service enum must hold exactly the services the catalogue listed, or the
 * model is briefed on a service it is not permitted to name. The registry-order
 * path pairs osint_services_list_bounded() with
 * osint_analysis_schema_dynamic_limited(note.shown); the semantic path pairs
 * this with osint_analysis_schema_dynamic_ids(), below. Use them in pairs.
 *
 * ENV
 *   JO_EMBED_URL              (shared with embed_pod) empty = this is inert
 *   JO_ROUTE_SEMANTIC=0       force the registry-order path even when able
 *   JO_ROUTE_TOPK             services listed for a query (default 60)
 */
#ifndef JO_SERVICE_VEC_H
#define JO_SERVICE_VEC_H

#include "db.h"
#include "osint_dispatch.h"

#define SVEC_TABLE      "service_vec"
#define SVEC_META_TABLE "service_vec_meta"
/* The lexical half of the router: the same service cards in an fts5 index,
 * built in the same pass as the vectors and fused with them by RRF at query
 * time. Separate table rather than a column because vec0 has no text index
 * and fts5 has no vectors; they are two views of one corpus. */
#define SVEC_FTS_TABLE  "service_fts"
/* lib/vecindex's incremental state: one row per embedded card with its text
 * hash, one per card the server refused. */
#define SVEC_DONE_TABLE "service_vec_done"
#define SVEC_FAIL_TABLE "service_vec_failed"

/* WIDENED 2026-10-12 to EVERY registered source (~18,900: pivots, scheduled
 * feeds, pods, manual runs — core/source_card.h defines the four kinds and
 * the card text). The catalogue functions below still answer with PIVOTS
 * ONLY unless a kind is asked for, because the pipeline's analysis schema is
 * built from what they return and a scheduled feed cannot be dispatched on an
 * entity. The other kinds serve GET /api/sources/search and
 * service_vec_route_sources(). */

/* Build or refresh the service index, INCREMENTALLY: a card whose text hash
 * is unchanged costs a hash compare, a changed one is re-embedded, at most
 * JO_SVEC_MAX_PER_RUN (default 4096; 0 = no cap) embeds per call, pivots
 * first. Each kind answers queries only once all of its cards are settled for
 * the registry as it stands (meta sig_<kind>), so a partial build never
 * answers from a fraction of a kind. A model or dimension change starts the
 * index over. Returns the number of cards indexed, 0 when inert (not an
 * error), negative on failure. */
int service_vec_build(db_handle *db);

/* What a search may return. `kind` NULL/"" = every kind that is ready. */
typedef struct {
  const char *kind;                 /* "pivot" | "scheduled" | "pod" | "manual" */
} service_vec_filter;

typedef struct {
  char *id;
  double score;                     /* RRF score, higher = closer            */
  int vrank, frank;                 /* 1-based rank per arm; 0 = absent      */
  double distance;                  /* vector arm's L2 distance (vrank > 0)  */
} service_vec_hit;

/* What a search could see — the coverage a caller must disclose. */
typedef struct {
  unsigned searched_kinds;          /* bit (1 << source_kind) per kind used  */
  unsigned unready_kinds;           /* asked for but not ready: NOT searched */
  int total;                        /* cards in the searched kinds           */
  int semantic, lexical;            /* candidates each arm contributed       */
  char reason[160];                 /* why -1, when it is                    */
} service_vec_info;

/* The k cards most relevant to `query` (RRF of the vector arm and the keyword
 * arm, both restricted to `f`'s kind and to kinds that are READY). Returns
 * the hit count (*out malloc'd, service_vec_hits_free) or -1 with
 * info->reason saying why (inert, not built, model changed, …). `info` may
 * be NULL. Embeds the query: call it off the event loop. */
int service_vec_search(db_handle *db, const char *query,
                       const service_vec_filter *f, int k,
                       service_vec_hit **out, service_vec_info *info);
void service_vec_hits_free(service_vec_hit *h, int n);

/* The SCHEDULED sources most relevant to a question that names no entity
 * ("what is happening with earthquakes in Kyushu"): a scheduled source
 * cannot be dispatched on an entity, but its stored records are what such a
 * question is answered from, so this says WHICH sources' records to read.
 * `kind` NULL = "scheduled"; another kind is honoured, never widened. Up to k
 * ids (malloc'd; service_vec_free_ids) in relevance order, or -1 when the
 * index cannot answer (inert, the kind not ready, model changed) — the caller
 * then has no ranking and must say so, not guess one. Embeds the query: call
 * it off the event loop. Function only: the pipeline wiring comes later. */
int service_vec_route_sources(db_handle *db, const char *query, const char *kind,
                              int k, char ***out_ids);

/* Every stored card vector, once: fn(id, vec, dim, user) per row, vectors
 * borrowed for the call. For read-only consumers that compare sources with
 * each other (GET /api/admin/layers/review). Returns rows visited, -1 when
 * there is no index. Whatever is stored is visited — ready or not; a caller
 * that needs completeness checks service_vec_ready_all() and says which. */
typedef void (*service_vec_scan_fn)(const char *id, const float *vec, int dim,
                                    void *user);
int service_vec_scan(db_handle *db, service_vec_scan_fn fn, void *user);

/* 1 when EVERY kind is indexed for the registry as it stands, with `sig`
 * (may be NULL) set to a string naming that exact index (card signature,
 * model, dim) — for a consumer that derives something from the vectors
 * (core/source_category.c) and must redo it when, and only when, they
 * change. 0 otherwise. Reads meta only: no embedding call. */
int service_vec_ready_all(db_handle *db, char *sig, size_t cap);

/* lib/vecindex's coverage object plus `kinds`: {pivot:{registered,ready},…}. */
#include "../third_party/cJSON.h"
cJSON *service_vec_coverage_json(db_handle *db);

/* The K services most similar to `query`, as the same catalogue text
 * osint_services_list_bounded() produces (one `id: description` per line, with
 * the bound stated in-band). NULL when the semantic path is unavailable — the
 * caller must fall back. `note` may be NULL. `out_ids`/`out_n` (may be NULL)
 * receive the chosen ids so the caller can build a matching schema enum; the
 * array and its strings are malloc'd and owned by the caller. */
char *service_vec_catalogue(db_handle *db, const char *query, int k,
                            osint_catalogue_note *note,
                            char ***out_ids, int *out_n);

/* service_vec_catalogue() under a prompt byte budget: listing stops before
 * the text (disclosure line included) would exceed `max_bytes`, and the
 * disclosure says the list was cut to fit. 0 = no budget. NULL when not even
 * one service fits — the caller falls back as for any other NULL. */
char *service_vec_catalogue_bounded(db_handle *db, const char *query, int k,
                                    size_t max_bytes,
                                    osint_catalogue_note *note,
                                    char ***out_ids, int *out_n);

/* service_vec_catalogue_bounded() for an explicit filter: `filter->kind`
 * "pivot" is exactly service_vec_catalogue_bounded(); another kind lists
 * those sources instead, and NULL lists every ready kind. The in-band bound
 * names what was listed ("… registered scheduled sources"). */
char *service_vec_catalogue_filtered(db_handle *db, const char *query, int k,
                                     size_t max_bytes,
                                     const service_vec_filter *filter,
                                     osint_catalogue_note *note,
                                     char ***out_ids, int *out_n);

/* Free what service_vec_catalogue() returned through out_ids/out_n. */
void service_vec_free_ids(char **ids, int n);

#endif
