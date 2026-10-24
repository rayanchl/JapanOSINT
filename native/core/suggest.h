/* core/suggest.{c,h} — /api/search/suggest with a retrieval arm.
 *
 * WHY. Every suggestion used to be generated: one /completion call on the
 * suggest server per settled keystroke, for nine strings the model makes up.
 * Most of what a user is about to type in an investigation workspace has been
 * typed before — by them or a colleague (search_history), or kept on purpose
 * (saved_searches) — or names a service that answers it. Those are REAL
 * suggestions, and finding them is a nearest-neighbour query, not a
 * generation.
 *
 * WHAT IT DOES, with an embedding server:
 *   - candidates: the workspace's recent search_history (SUGGEST_HISTORY_SCAN
 *     rows) and saved_searches, deduplicated, plus the service router's
 *     SUGGEST_SERVICE_K nearest services (service_vec_catalogue);
 *   - every candidate is scored by cosine to the typed text; those at or above
 *     SUGGEST_MIN_COS are ranked, and each suggestion says where it came from
 *     (items[].source: "history" | "saved" | "service" | "llm");
 *   - the LLM is asked only with expand=1, and its suggestions come after the
 *     retrieved ones;
 *   - the RETRIEVED list is cut at SUGGEST_MAX (9, the client's list), and the
 *     cut is stated: meta {candidates, shown, cut_at, truncated, llm}; the
 *     LLM's ideas asked for with expand=1 follow the cut in full.
 * Tenancy: history and saved searches are read for the caller's workspace
 * only (the same rule savedsearchapi.c applies to its lists).
 *
 * INERT. No JO_EMBED_URL → suggest_build() returns NULL and the caller
 * answers exactly as before (LLM, {"suggestions":[…]}). An embed failure
 * does the same. `suggestions` stays an array of strings in every case, so
 * existing clients read the new answer unchanged. */
#ifndef JO_SUGGEST_H
#define JO_SUGGEST_H

#include "db.h"
#include "llm.h"

#define SUGGEST_MAX           9      /* the clients' list                     */
#define SUGGEST_HISTORY_SCAN  200    /* newest history rows read per call      */
#define SUGGEST_SAVED_SCAN    200    /* saved searches read per call           */
#define SUGGEST_SERVICE_K     5      /* services asked of the router           */
#define SUGGEST_MIN_COS       0.50   /* JO_SUGGEST_MIN_COS overrides           */

/* The whole response body (malloc'd), or NULL when the retrieval arm cannot
 * answer — the caller then takes the generation-only path. `tenant` may be
 * NULL/"" (no history arm). */
char *suggest_build(db_handle *db, llm_client *llm, const char *tenant,
                    const char *query, int expand);

#endif
