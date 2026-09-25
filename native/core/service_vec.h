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
 * embedding call fails, or the dimension does not match, it returns NULL and
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

/* Build or refresh the service index. Safe to call repeatedly: it returns
 * immediately unless the registry signature, model or dimension changed. A
 * partial build is discarded rather than left as a half-index that would
 * answer confidently from a subset. Returns the number of services indexed,
 * 0 when inert (not an error), negative on failure. */
int service_vec_build(db_handle *db);

/* The K services most similar to `query`, as the same catalogue text
 * osint_services_list_bounded() produces (one `id: description` per line, with
 * the bound stated in-band). NULL when the semantic path is unavailable — the
 * caller must fall back. `note` may be NULL. `out_ids`/`out_n` (may be NULL)
 * receive the chosen ids so the caller can build a matching schema enum; the
 * array and its strings are malloc'd and owned by the caller. */
char *service_vec_catalogue(db_handle *db, const char *query, int k,
                            osint_catalogue_note *note,
                            char ***out_ids, int *out_n);

/* Free what service_vec_catalogue() returned through out_ids/out_n. */
void service_vec_free_ids(char **ids, int n);

#endif
