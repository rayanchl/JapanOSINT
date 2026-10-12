/* core/story.{c,h} — stories and novelty, from the embedding pod's post-batch
 * hook (core/embed_pod.h, embed_hook_register).
 *
 * STORIES. simhash's cluster_id groups NEAR-DUPLICATE text (the same wire
 * story re-published). A story is looser: items that say the same thing in
 * different words, inside a few days of each other — a JMA bulletin, the NHK
 * article about it and a prefecture's evacuation notice. For every newly
 * embedded item the hook asks intel_vec_v2 for its STORY_K nearest
 * neighbours of the SAME tenant within ±STORY_WINDOW_SEC (a KNN filter on
 * `ts`, so a neighbour outside the window is never even ranked), and any
 * neighbour with cosine >= story_tau() puts the item in a story:
 *   - the item joins the story of its most similar neighbour that has one;
 *   - neighbours above tau that have NO story yet are pulled into it;
 *   - two existing stories are NEVER merged — that is the bound: one item
 *     can only add itself and up to STORY_K singletons, so a chain of
 *     "A is like B is like C" cannot fold a week of news into one story;
 *   - a story at story_max_members() takes no new members (the next-best
 *     neighbour's story, else a new one) — stated in the story response;
 *   - an item already in a story keeps it (re-embedding after an edit does
 *     not move it).
 * The column is intel_items.story_id, separate from cluster_id; NULL means
 * "no neighbour above tau", never a placeholder. The story id is "st-" + 16
 * hex of the seed item's uid hash, so it names no uid.
 *
 * Tenancy: a story never crosses tenants (the KNN is constrained to the
 * item's own tenant partition), so a story id can never pull another
 * tenant's row into a member list. /api/stories/:id reads "this tenant +
 * the shared corpus", like every intel route.
 *
 * NOVELTY. intel_items.novelty = 1 − the highest cosine to any item of the
 * same scope (the tenant + 'legacy') dated within the previous novelty_days()
 * days, up to and including the item's own date (self excluded). 1.0 means
 * nothing in the window resembles it; it is measured, and a row the hook has
 * not reached is NULL, not a guess.
 *
 * BUDGET. A hook must be fast (embed_pod.h). Two KNNs per row on a large
 * partition are not free, so the hook stops after story_budget_ms() and puts
 * the rest into story_pending; each later call drains pending rows (loading
 * their stored vectors) with whatever budget its own batch left. Nothing is
 * skipped: a row is either processed or pending, and both counts are in
 * story_meta.
 *
 *   JO_STORY_TAU             cosine to join a story (default 0.85)
 *   JO_STORY_MAX_MEMBERS     members per story (default 500)
 *   JO_NOVELTY_DAYS          novelty look-back (default 7)
 *   JO_STORY_BUDGET_MS       hook time per batch (default 2000)
 *   JO_STORY_HOOK=0          disables both (nothing registered)
 */
#ifndef JO_STORY_H
#define JO_STORY_H

#include <stddef.h>
#include "db.h"

/* 0.85: multilingual-e5 puts two reports of the SAME event in different
 * words around 0.86–0.93 and two unrelated items of the same genre (two
 * different earthquakes, two different company filings) around 0.78–0.84 —
 * the boundary that matters for a story is the second one, so tau sits just
 * above it. A wrong join is visible (the member list) and does not chain. */
#define STORY_TAU_DEFAULT        0.85
#define STORY_WINDOW_SEC         (72 * 3600)
#define STORY_K                  10
#define STORY_MAX_MEMBERS_DEFAULT 500
#define NOVELTY_DAYS_DEFAULT     7
#define STORY_BUDGET_MS_DEFAULT  2000

double story_tau(void);
int    story_max_members(void);
int    novelty_days(void);

/* ensure_column intel_items.story_id / .novelty, the story index and the
 * pending/meta tables. Idempotent; called from db_open(). */
void story_migrate(db_handle *db);

/* The story id for a seed uid ("st-" + 16 hex). out >= 20 bytes. */
void story_id_for(const char *seed_uid, char *out, size_t cap);

/* Novelty of one embedded row, computed from the index (no embed). The
 * stored value when the hook already wrote one. 0 with *out set, -1 when the
 * index cannot answer. Used by the alert hook's optional novelty gate. */
int story_novelty(db_handle *db, const char *uid, const char *tenant,
                  long long ts, const float *vec, int dim, double *out);

/* The hook body, exposed for tests (normally reached via the embed pod). */
void story_process_batch(db_handle *db, int n, const char *const *uids,
                         const float *vectors, int dim, const char *const *tenants,
                         const long long *ts);

#endif
