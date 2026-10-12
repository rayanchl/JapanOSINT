/* core/service_vec.c — see service_vec.h for why this exists. */
#include "service_vec.h"
#include "embed_pod.h"
#include "llm.h"
#include "source_card.h"
#include "../source.h"
#include "../lib/vecindex.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>

/* One embedding request per this many cards. Same default as the intel pod:
 * large enough that ~18,900 cards is ~590 requests, small enough that a
 * failure costs little and the server is not handed a huge body. */
#define SVEC_BATCH 32
/* Services listed for a query. 60 × (id + description) lands around 12 KB —
 * well inside the 32 KB prompt budget, with the descriptions KEPT, which is
 * the whole point: the registry-order path had to drop them at this size. */
#define SVEC_TOPK_DEF 60
/* Cards EMBEDDED per build tick (unchanged cards cost a hash compare and are
 * not counted). The whole registry is ~18,900 cards and was measured at about
 * 45 minutes of CPU embedding against multilingual-e5-large; one tick holding
 * the _maint lane that long would starve the intel backfill pod that shares
 * it. 4,096 is ~10 minutes, and covers every pivot (embedded FIRST, see the
 * build) in the first tick. JO_SVEC_MAX_PER_RUN overrides; 0 = no cap. */
#define SVEC_MAX_PER_RUN_DEF 4096

/* The index, as lib/vecindex sees it. It used to be rebuilt WHOLE (no
 * done/fail tables) because it held the ~2,300 pivots and a rebuild took four
 * minutes; at the whole registry that is 45, so it is now INCREMENTAL: the
 * done table holds each card's text hash and only a card whose text changed
 * is re-embedded. `kind` is the PARTITION KEY — the pivot catalogue, which
 * runs inside every search, then scans the pivot partition's chunks only,
 * never the 16,500 scheduled cards it can never return. */
static const vecindex_col svec_cols[] = { { "kind", VI_TEXT, 1 } };
static const vecindex_spec svec_spec = {
  .name = "service", .table = SVEC_TABLE, .key_col = "id",
  .meta_table = SVEC_META_TABLE, .done_table = SVEC_DONE_TABLE,
  .fail_table = SVEC_FAIL_TABLE, .cols = svec_cols, .ncols = 1,
  .rebuild_hint = " (the _svec_index pod rebuilds it on the next tick)" };
VECINDEX_REGISTER(svec_spec)

static int svec_enabled(void) {
  const char *e = getenv("JO_ROUTE_SEMANTIC");
  if (e && e[0] == '0' && e[1] == 0) return 0;
  const char *b = embed_base_url();
  return b && *b;
}

static int svec_topk(void) {
  const char *e = getenv("JO_ROUTE_TOPK");
  int v = (e && *e) ? atoi(e) : 0;
  return v > 0 ? v : SVEC_TOPK_DEF;
}

static long svec_max_per_run(void) {
  const char *e = getenv("JO_SVEC_MAX_PER_RUN");
  if (e && *e) { long v = atol(e); return v > 0 ? v : 0; }
  return SVEC_MAX_PER_RUN_DEF;
}

/* ASYMMETRIC-RETRIEVAL PREFIXES.
 *
 * The E5 family (multilingual-e5-*, and BGE similarly) is trained with the
 * query and the document marked differently — "query: ..." against
 * "passage: ..." — and its model card says retrieval quality drops when they
 * are omitted. The knob exists so that contract can be honoured.
 *
 * MEASURED ON THIS CORPUS, THE PREFIXES MADE ROUTING WORSE, so they default
 * to OFF. A/B against multilingual-e5-large over all 1,838 services,
 * 2026-09-08:
 *
 *   "who owns the domain example.com and who registered it"
 *      without: HISTORICAL_WHOIS 1, DOMAIN_HISTORY 2, DNS_RECORDS 5
 *      with:    HISTORICAL_WHOIS 8, DNS_RECORDS absent from the top 8
 *   "company registration and corporate filings for a business"
 *      without: COMPANY_LOOKUP 1
 *      with:    COMPANY_LOOKUP absent from the top 8
 *
 * The likely reason is that these "documents" are not passages: a service
 * entry is an id, a short name and one or two sentences, so the asymmetry the
 * prefixes are meant to encode is not there to encode. Recorded rather than
 * quietly dropped, because "the model card says to" is exactly the kind of
 * plausible reasoning that should lose to a measurement — and because the
 * next person to read the model card will otherwise turn it on.
 *
 * Keep it a KNOB, not a hardcoded string: it is model-specific, a model not
 * trained this way must not have the words prepended, and a future corpus
 * (longer descriptions) could flip the result. Empty by default, so an
 * unconfigured deployment behaves exactly as before. The doc prefix is part of
 * every embedded card's text, so changing it changes every card's hash and
 * re-embeds the corpus; it is also folded into the kind signatures, so no kind
 * reads as ready until all of its cards are in the new prefix — embedding the
 * corpus with one prefix and the query with another is the same mixed-space
 * error as changing the model.
 *
 *   JO_EMBED_QUERY_PREFIX=query:      (note the trailing space when set)
 *   JO_EMBED_DOC_PREFIX=passage:
 */
static const char *svec_query_prefix(void) {
  const char *e = getenv("JO_EMBED_QUERY_PREFIX");
  return e ? e : "";
}
static const char *svec_doc_prefix(void) {
  const char *e = getenv("JO_EMBED_DOC_PREFIX");
  return e ? e : "";
}

/* ---- the registry side ---------------------------------------------------
 *
 * The cards, their kinds and their text all come from core/source_card.c, so
 * the vector arm, the keyword arm and `--dump-source-cards` (which the Python
 * manifest checker embeds) read one text. The pivot rule there is the one
 * osint_dispatch.c's is_entity_pivot applies — collector=="osint" AND
 * update_interval_sec==0 — and an index that disagreed with the catalogue
 * about what a pivot is would route to services the prompt never mentions. */

static unsigned long long fnv_bytes(unsigned long long h, const void *p, size_t n) {
  const unsigned char *b = p;
  for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
  return h;
}

/* A signature of one kind's cards (kind -1 = every kind): ids, card hashes
 * and the doc prefix, in registry order. Any add, removal, rename, card edit
 * or prefix change moves it, and a kind answers queries only while its
 * recorded signature equals this one — the index must never answer from a
 * registry, or a card text, that no longer exists. */
static unsigned long long svec_kind_sig(int kind, int *out_count) {
  unsigned long long h = 1469598103934665603ULL;
  const source_card *c = NULL;
  int n = source_cards_all(&c), cnt = 0;
  for (int i = 0; c && i < n; i++) {
    if (!c[i].text || (kind >= 0 && (int)c[i].kind != kind)) continue;
    h = fnv_bytes(h, c[i].def->id, strlen(c[i].def->id) + 1);
    h = fnv_bytes(h, &c[i].hash, sizeof c[i].hash);
    cnt++;
  }
  const char *px = svec_doc_prefix();
  h = fnv_bytes(h, px, strlen(px) + 1);
  if (out_count) *out_count = cnt;
  return h;
}

/* The text a card is EMBEDDED as: the doc prefix, then the card, bounded the
 * way the query will be. */
static char *svec_text(const source_card *c) {
  const char *px = svec_doc_prefix();
  size_t n = strlen(px) + strlen(c->text) + 1;
  char *t = malloc(n);
  if (!t) return NULL;
  snprintf(t, n, "%s%s", px, c->text);
  char *b = embed_bound_text(t);
  free(t);
  return b;
}

/* ---- meta ---------------------------------------------------------------- */

static void svec_meta_set(db_handle *db, const char *k, const char *v) {
  vecindex_meta_set(db, &svec_spec, k, v ? v : "");
}

static char *svec_meta_get(db_handle *db, const char *k) {
  return vecindex_meta_dup(db, &svec_spec, k);
}

static void svec_sig_key(char *out, size_t cap, int kind) {
  snprintf(out, cap, "sig_%s", source_kind_name((source_kind)kind));
}

static void now_iso(char *at, size_t cap) {
  time_t t = time(NULL);
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(at, cap, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

/* Is kind `kind` ready for the registry as it stands right now? */
static int svec_kind_ready(db_handle *db, int kind) {
  char key[32], cur[32];
  svec_sig_key(key, sizeof key, kind);
  snprintf(cur, sizeof cur, "%llu", svec_kind_sig(kind, NULL));
  char *have = svec_meta_get(db, key);
  int ok = have && !strcmp(have, cur);
  free(have);
  return ok;
}

/* ---- build --------------------------------------------------------------- */

/* Start over: the vector table, the keyword table, every done/fail row and
 * the identity. Used when the space changed (model, dimension) or the table
 * predates `kind` — never on a server that merely failed to answer. vec0
 * owns shadow tables named after the virtual table, so this DROPs; nothing
 * here renames (lib/vecindex.h, lesson 1). */
static void svec_wipe(db_handle *db, const char *why) {
  fprintf(stderr, "[svec] starting the service index over: %s\n", why);
  vecindex_drop(db, &svec_spec);
  sqlite3_exec(db->h, "DROP TABLE IF EXISTS " SVEC_FTS_TABLE, NULL, NULL, NULL);
  vecindex_ensure_side_tables(db, &svec_spec);
  sqlite3_exec(db->h, "DELETE FROM " SVEC_DONE_TABLE, NULL, NULL, NULL);
  sqlite3_exec(db->h, "DELETE FROM " SVEC_FAIL_TABLE, NULL, NULL, NULL);
  vecindex_meta_set(db, &svec_spec, "model", NULL);
  vecindex_meta_set(db, &svec_spec, "dim", NULL);
  vecindex_meta_set(db, &svec_spec, "refused", NULL);
  for (int k = 0; k < SC_NKINDS; k++) {
    char key[32];
    svec_sig_key(key, sizeof key, k);
    svec_meta_set(db, key, "");
  }
  svec_meta_set(db, "registry_sig", "");
  svec_meta_set(db, "fts_sig", "");
  svec_meta_set(db, "state", "building");
}

/* Does an existing vec0 table have the `kind` partition column? An index from
 * before the widening has none, and vec0 cannot ALTER, so it starts over. */
static int svec_schema_current(db_handle *db) {
  if (!vecindex_exists(db, &svec_spec)) return 1;
  sqlite3_stmt *s = NULL;
  int ok = 0;
  if (sqlite3_prepare_v2(db->h, "SELECT sql FROM sqlite_master WHERE name='"
                         SVEC_TABLE "'", -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *sql = (const char *)sqlite3_column_text(s, 0);
      ok = sql && strstr(sql, "kind") != NULL;
    }
    sqlite3_finalize(s);
  }
  return ok && vecindex_table_exists(db, SVEC_DONE_TABLE);
}

/* Rows for ids the registry no longer has: their vectors, done and fail rows
 * go. An index that keeps a retired source routes to something that cannot
 * be dispatched. */
static int svec_drop_stale(db_handle *db) {
  int dropped = 0;
  const char *tabs[2] = { SVEC_DONE_TABLE, SVEC_FAIL_TABLE };
  for (int t = 0; t < 2; t++) {
    char sql[160];
    snprintf(sql, sizeof sql, "SELECT id FROM \"%s\"", tabs[t]);
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) continue;
    char **gone = NULL; int ng = 0, cg = 0;
    while (sqlite3_step(s) == SQLITE_ROW) {
      const char *id = (const char *)sqlite3_column_text(s, 0);
      if (!id || source_card_get(id)) continue;
      if (ng == cg) {
        int nc = cg ? cg * 2 : 16;
        char **ng2 = realloc(gone, (size_t)nc * sizeof *ng2);
        if (!ng2) break;
        gone = ng2; cg = nc;
      }
      gone[ng++] = strdup(id);
    }
    sqlite3_finalize(s);
    if (ng && vecindex_begin(db) == 0) {
      sqlite3_stmt *dv = NULL, *dd = NULL;
      int have_vec = vecindex_exists(db, &svec_spec);
      if (have_vec)
        sqlite3_prepare_v2(db->h, "DELETE FROM " SVEC_TABLE " WHERE id=?1", -1, &dv, NULL);
      snprintf(sql, sizeof sql, "DELETE FROM \"%s\" WHERE id=?1", tabs[t]);
      sqlite3_prepare_v2(db->h, sql, -1, &dd, NULL);
      for (int i = 0; i < ng; i++) {
        if (!gone[i]) continue;
        if (dv) { sqlite3_reset(dv); sqlite3_bind_text(dv, 1, gone[i], -1, SQLITE_STATIC); sqlite3_step(dv); }
        if (dd) { sqlite3_reset(dd); sqlite3_bind_text(dd, 1, gone[i], -1, SQLITE_STATIC); sqlite3_step(dd); }
        dropped++;
      }
      sqlite3_finalize(dv); sqlite3_finalize(dd);
      vecindex_end(db, 1);
    }
    for (int i = 0; i < ng; i++) free(gone[i]);
    free(gone);
  }
  if (dropped)
    fprintf(stderr, "[svec] dropped %d index rows for sources no longer registered\n",
            dropped);
  return dropped;
}

/* The LEXICAL arm of the router: every card in an fts5 table, with its kind
 * beside it so a kind-filtered search filters both arms the same way.
 *
 * Why there is a second arm at all — measured 2026-09-11 against
 * multilingual-e5-large over all 1,838 pivots, at the production K of 60:
 * "who owns example.com" put DNS_RECORDS at 57 and did NOT return
 * DOMAIN_WHOIS at all. A longer phrasing of the same question ranks it first
 * (the A/B recorded above the prefix knob), so the vector arm is not broken —
 * it is simply weak on a terse query whose words do not appear in the
 * description. The word "whois" IS in that service's card, which is exactly
 * what a keyword index is for. Semantic recall and lexical recall fail on
 * different queries; fusing them is cheaper and more honest than tuning
 * either alone.
 *
 * It costs no embedding, so it is rebuilt whole whenever the cards change
 * (one transaction, ~19k rows) rather than kept incrementally. */
static int svec_build_fts(db_handle *db, const char *allsig) {
  char *have = svec_meta_get(db, "fts_sig");
  int same = have && !strcmp(have, allsig) &&
             vecindex_table_exists(db, SVEC_FTS_TABLE);
  free(have);
  if (same) return 0;
  sqlite3_exec(db->h, "DROP TABLE IF EXISTS " SVEC_FTS_TABLE, NULL, NULL, NULL);
  if (sqlite3_exec(db->h,
        "CREATE VIRTUAL TABLE " SVEC_FTS_TABLE " USING fts5("
        "id UNINDEXED, kind UNINDEXED, text, tokenize='unicode61')",
        NULL, NULL, NULL) != SQLITE_OK) {
    fprintf(stderr, "[svec] no fts5 here — routing runs on the vector arm "
                    "alone (recall is lower on terse queries)\n");
    return -1;
  }
  const source_card *c = NULL;
  int n = source_cards_all(&c), ok = 1;
  if (vecindex_begin(db) != 0) return -1;
  sqlite3_stmt *ins = NULL;
  if (sqlite3_prepare_v2(db->h, "INSERT INTO " SVEC_FTS_TABLE "(id,kind,text) "
                         "VALUES(?1,?2,?3)", -1, &ins, NULL) != SQLITE_OK) ok = 0;
  for (int i = 0; ok && i < n; i++) {
    if (!c[i].text) continue;
    sqlite3_reset(ins);
    sqlite3_bind_text(ins, 1, c[i].def->id, -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, source_kind_name(c[i].kind), -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, c[i].text, -1, SQLITE_STATIC);
    if (sqlite3_step(ins) != SQLITE_DONE) ok = 0;
  }
  sqlite3_finalize(ins);
  if (vecindex_end(db, ok) != 0 || !ok) {
    fprintf(stderr, "[svec] keyword index build failed: %s\n", sqlite3_errmsg(db->h));
    return -1;
  }
  svec_meta_set(db, "fts_sig", allsig);
  return 0;
}

/* Embed what changed in one kind, at most *budget cards. Returns the
 * vecindex_sync code (0 / -1 / -2); *complete = every card of the kind is
 * settled (none held back by the budget). */
static int svec_sync_kind(db_handle *db, llm_client *llm, const char *model,
                          int kind, long *budget, const char *now,
                          int *complete, vecindex_stats *st) {
  *complete = 0;
  const source_card *c = NULL;
  int n = source_cards_all(&c), m = 0;
  for (int i = 0; i < n; i++) if (c[i].text && (int)c[i].kind == kind) m++;
  if (m == 0) { *complete = 1; return 0; }
  vecindex_item *items = calloc((size_t)m, sizeof *items);
  char **texts = calloc((size_t)m, sizeof *texts);
  vecindex_val *vals = calloc((size_t)m, sizeof *vals);
  if (!items || !texts || !vals) { free(items); free(texts); free(vals); return -1; }
  int k = 0, held = 0, built = 1;
  for (int i = 0; i < n && built; i++) {
    if (!c[i].text || (int)c[i].kind != kind) continue;
    texts[k] = svec_text(&c[i]);
    /* A card that cannot be built fails the tick: skipping it would mark the
     * kind complete without it. */
    if (!texts[k]) { built = 0; break; }
    vals[k] = VI_S(source_kind_name(c[i].kind));
    items[k].key = c[i].def->id;
    items[k].text = texts[k];
    items[k].vals = &vals[k];
    vecindex_item_state(db, &svec_spec, &items[k]);
    int unchanged = vecindex_item_unchanged(&items[k]);
    if (!unchanged) {
      if (*budget == 0) { held++; free(texts[k]); texts[k] = NULL; continue; }
      if (*budget > 0) (*budget)--;
    }
    k++;
  }
  int rc = -1;
  if (built) {
    rc = vecindex_sync(db, &svec_spec, llm, model, items, k, SVEC_BATCH, now, now, st);
    if (rc == 0 && held == 0) *complete = 1;
    if (held)
      fprintf(stderr, "[svec] %s: %d cards wait for the next tick "
                      "(JO_SVEC_MAX_PER_RUN)\n", source_kind_name((source_kind)kind), held);
  }
  for (int i = 0; i < m; i++) free(texts[i]);
  free(items); free(texts); free(vals);
  return rc;
}

int service_vec_build(db_handle *db) {
  if (!db || !svec_enabled()) return 0;

  vecindex_ensure_meta(db, &svec_spec);

  int want = 0;
  unsigned long long sig = svec_kind_sig(-1, &want);
  if (want <= 0) return 0;

  char sigs[32];
  snprintf(sigs, sizeof sigs, "%llu", sig);

  llm_client llm = { .http = NULL, .base_url = embed_base_url(), .interactive = 0 };

  /* The model's identity, by the same rule the intel pod uses
   * (embed_detect_model): JO_EMBED_MODEL when the operator states it, else
   * what the server calls itself in GET /v1/models. It used to be compared
   * ONLY when JO_EMBED_MODEL was set — and launch.sh does not set it — so a
   * same-dimension model swap kept an index built in the old space "current"
   * for good. embed_index_dim() must NOT be used here: it reads the INTEL
   * pod's intel_vec_meta, a different index with its own lifecycle.
   *
   * Unidentifiable is not "changed": a server that does not answer right now
   * is a reason to do nothing, never a reason to drop a good index. */
  char model[256] = {0};
  if (vecindex_detect_model(NULL, embed_base_url(), model, sizeof model) != 0) {
    fprintf(stderr, "[svec] cannot identify the embedding model (%s) — index "
                    "left as it is\n", model);
    char msg[320];
    snprintf(msg, sizeof msg, "model check failed: %.250s; index untouched", model);
    svec_meta_set(db, "last_error", msg);
    return -1;
  }

  if (!svec_schema_current(db))
    svec_wipe(db, "the table predates the kind column (pivots-only index)");

  /* Already current? The registry signature (every card, every kind), the
   * model name and the DIMENSION must all match. The dimension is verified
   * against the server as it is right now — one short probe embed — because
   * that is the check that cannot be fooled: a different model behind the
   * same URL is a different vector space, and a nearest neighbour across two
   * spaces is a number, not a similarity. */
  char *have_sig = svec_meta_get(db, "registry_sig");
  char *have_model = svec_meta_get(db, "model");
  char *have_dim = svec_meta_get(db, "dim");
  char *have_state = svec_meta_get(db, "state");
  int current = 0, probe_failed = 0, wipe = 0;
  if (have_model && *have_model && strcmp(have_model, model) != 0) {
    fprintf(stderr, "[svec] index model '%s' vs server '%s' — rebuilding "
                    "rather than mixing spaces\n", have_model, model);
    wipe = 1;
  } else if (have_sig && !strcmp(have_sig, sigs) &&
             have_state && !strcmp(have_state, "ready")) {
    if (!have_model || !*have_model) {
      /* No recorded model (an index from before identity was recorded) is a
       * space nobody can vouch for: rebuild once rather than guess. */
      wipe = 1;
    } else {
      llm_status pst;
      int pdim = vecindex_probe_dim(&llm, 15000, &pst);
      if (pdim <= 0) {
        /* The probe could not be answered. That says the SERVER is unwell,
         * not that the index is wrong — the old code fell through to the
         * rebuild, set state=building and DROPPED a good index on one 503. */
        probe_failed = 1;
        fprintf(stderr, "[svec] dimension probe failed (%s) — index left as it "
                        "is\n", llm_status_code(pst));
      } else if (have_dim && pdim == atoi(have_dim)) {
        current = 1;
      } else {
        fprintf(stderr, "[svec] embedding server now answers %d-d, index is %s-d "
                        "— rebuilding rather than mixing spaces\n", pdim,
                have_dim ? have_dim : "?");
        wipe = 1;
      }
    }
  }
  free(have_sig); free(have_model); free(have_dim); free(have_state);
  if (current) {
    long cnt = vecindex_meta_long(db, &svec_spec, "count");
    return cnt > 0 ? (int)cnt : want;
  }
  if (probe_failed) {
    svec_meta_set(db, "last_error", "dimension probe failed; index untouched");
    return -1;
  }
  if (wipe) svec_wipe(db, "the embedding model or dimension changed");

  /* INCREMENTAL, KIND BY KIND, PIVOTS FIRST.
   *
   * `state` is "building" until every kind is complete, and the index-wide
   * guarantee the old all-or-nothing build gave is now per KIND: a kind
   * answers queries only once its recorded signature (sig_<kind>) equals the
   * signature of its cards right now, i.e. once every one of its cards is
   * settled in this space. A tick cut short (server down, budget spent)
   * leaves the cards it did embed on disk — they are correct vectors for
   * their exact text, recorded in the done table — and the next tick
   * resumes; it does not leave a kind answering from a fraction of itself.
   * Pivots go first because the search pipeline routes on them: they are
   * ready after the first tick even while 16,500 scheduled cards wait. */
  vecindex_ensure_side_tables(db, &svec_spec);
  svec_meta_set(db, "state", "building");
  char now[40];
  now_iso(now, sizeof now);
  long budget = svec_max_per_run();
  if (budget == 0) budget = -1;                  /* -1 = no cap */
  vecindex_stats st = {0};
  int rc = 0, all_ready = 1, retried = 0;
  static const int ORDER[SC_NKINDS] = { SC_KIND_PIVOT, SC_KIND_SCHEDULED,
                                        SC_KIND_POD, SC_KIND_MANUAL };
  for (int oi = 0; oi < SC_NKINDS; oi++) {
    int kind = ORDER[oi], complete = 0;
    rc = svec_sync_kind(db, &llm, model, kind, &budget, now, &complete, &st);
    if (rc == -2 && !retried) {
      /* The probe inside the sync saw a different dimension than the one
       * recorded: a different space. Start over once, from the first kind. */
      retried = 1;
      svec_wipe(db, "the server's dimension differs from the index's");
      oi = -1; all_ready = 1;
      continue;
    }
    if (rc != 0) { all_ready = 0; break; }
    char key[32];
    svec_sig_key(key, sizeof key, kind);
    if (complete) {
      char ks[32];
      snprintf(ks, sizeof ks, "%llu", svec_kind_sig(kind, NULL));
      svec_meta_set(db, key, ks);
    } else {
      all_ready = 0;
    }
  }
  svec_drop_stale(db);
  svec_build_fts(db, sigs);

  long done = vecindex_count(db, SVEC_DONE_TABLE);
  {
    char nb[32];
    snprintf(nb, sizeof nb, "%d", want);
    svec_meta_set(db, "eligible_count", nb);
    svec_meta_set(db, "eligible_counted_at", now);
    svec_meta_set(db, "last_run_at", now);
    vecindex_meta_set_long(db, &svec_spec, "embedded_count", done);
    vecindex_meta_set_long(db, &svec_spec, "count", done);
    /* What THIS tick spent: the number that says the index is incremental
     * (a steady registry embeds 0) rather than a claim that it is. */
    vecindex_meta_set_long(db, &svec_spec, "last_embedded", st.embedded);
  }
  if (rc != 0) {
    /* The reason vecindex_sync recorded (last_error) stays; this only adds
     * that the index as a whole is not complete. */
    char *le = svec_meta_get(db, "last_error");
    if (!le || !*le)
      svec_meta_set(db, "last_error", "build stopped; kinds not yet complete "
                                      "keep refusing queries and resume next tick");
    free(le);
    return -1;
  }
  if (!all_ready) {
    fprintf(stderr, "[svec] %ld of %d cards indexed this far (%ld embedded this "
                    "tick); the rest follow on later ticks\n", done, want, st.embedded);
    return (int)(done > 0 ? done : 0);
  }
  svec_meta_set(db, "registry_sig", sigs);
  svec_meta_set(db, "last_error", "");
  svec_meta_set(db, "state", "ready");   /* last: everything above landed */
  fprintf(stderr, "[svec] indexed %ld of %d source cards (%ld embedded, %ld "
                  "unchanged, %ld failed)\n", done, want, st.embedded,
          st.unchanged, st.failed);
  return (int)done;
}

/* ---- query --------------------------------------------------------------- */

void service_vec_free_ids(char **ids, int n) {
  if (!ids) return;
  for (int i = 0; i < n; i++) free(ids[i]);
  free(ids);
}

void service_vec_hits_free(service_vec_hit *h, int n) {
  if (!h) return;
  for (int i = 0; i < n; i++) free(h[i].id);
  free(h);
}

/* ---- the lexical arm ------------------------------------------------------
 *
 * NOT core/fts.h's fts_query_expr(): that one AND-joins every token, which is
 * right for a search box over documents and wrong here — "who owns
 * example.com" AND-ed against one-sentence service cards matches nothing at
 * all. Routing wants OR with bm25 deciding, so that a query which happens to
 * say "whois" or "sanctions" or "vessel" pulls the card carrying that word up
 * regardless of the other words around it.
 *
 * Tokens are unicode61's own notion of a word (letters and digits), so the
 * expression matches how the index was built. Bytes ≥ 0x80 are kept inside a
 * token: that makes accented Latin work; it does NOT segment Japanese, which
 * arrives as one long token and simply finds nothing — the vector arm is what
 * serves Japanese queries, as it did before this arm existed. Stated because
 * an arm that silently contributes nothing for a whole language should be a
 * known limitation, not a surprise. */
#define SVEC_FTS_MAX_TOKENS 12

static char *svec_fts_expr(const char *raw) {
  if (!raw || !*raw) return NULL;
  size_t cap = strlen(raw) * 3 + 32, len = 0;
  char *out = malloc(cap);
  if (!out) return NULL;
  out[0] = 0;
  int used = 0;
  const char *p = raw;
  while (*p && used < SVEC_FTS_MAX_TOKENS) {
    while (*p && !(isalnum((unsigned char)*p) || (unsigned char)*p >= 0x80)) p++;
    const char *s = p;
    while (*p && (isalnum((unsigned char)*p) || (unsigned char)*p >= 0x80)) p++;
    size_t n = (size_t)(p - s);
    if (n < 3) continue;                  /* "of", "in", "a" carry no signal */
    if (n > 64) n = 64;
    len += (size_t)snprintf(out + len, cap - len, "%s\"%.*s\"",
                            used ? " OR " : "", (int)n, s);
    used++;
  }
  if (!used) { free(out); return NULL; }
  return out;
}

/* The keyword arm restricted to the kinds in `mask` — the same restriction
 * the vector arm gets from the partition key, so neither arm can rank a card
 * of a kind the other was not allowed to see. Returns rows added, -1 when the
 * arm has nothing to say (no table, bad expression). */
static int svec_fts_arm(db_handle *db, const char *expr, unsigned mask, int k,
                        vecindex_fused *f) {
  if (!expr || !vecindex_table_exists(db, SVEC_FTS_TABLE)) return -1;
  char sql[400];
  size_t len = (size_t)snprintf(sql, sizeof sql, "SELECT id FROM " SVEC_FTS_TABLE
                                " WHERE " SVEC_FTS_TABLE " MATCH ?1 AND kind IN (");
  int arg = 2, nk = 0;
  for (int i = 0; i < SC_NKINDS; i++)
    if (mask & (1u << i)) len += (size_t)snprintf(sql + len, sizeof sql - len,
                                                  "%s?%d", nk++ ? "," : "", arg++);
  if (!nk) return 0;
  snprintf(sql + len, sizeof sql - len, ") ORDER BY bm25(" SVEC_FTS_TABLE
           ") LIMIT ?%d", arg);
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(st, 1, expr, -1, SQLITE_STATIC);
  arg = 2;
  for (int i = 0; i < SC_NKINDS; i++)
    if (mask & (1u << i))
      sqlite3_bind_text(st, arg++, source_kind_name((source_kind)i), -1, SQLITE_STATIC);
  sqlite3_bind_int(st, arg, k);
  int rank = 0;
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char *key = (const char *)sqlite3_column_text(st, 0);
    if (!key) continue;
    if (!vecindex_fused_add(f, key, 0, ++rank, 0)) break;
  }
  sqlite3_finalize(st);
  return rank;
}

static void info_reason(service_vec_info *info, const char *why) {
  if (info) snprintf(info->reason, sizeof info->reason, "%s", why);
}

int service_vec_search(db_handle *db, const char *query,
                       const service_vec_filter *f, int k,
                       service_vec_hit **out, service_vec_info *info) {
  if (out) *out = NULL;
  if (info) memset(info, 0, sizeof *info);
  if (!db || !query || !*query || !out) { info_reason(info, "no query"); return -1; }
  if (!svec_enabled()) {
    info_reason(info, "semantic routing is off (JO_EMBED_URL unset or "
                      "JO_ROUTE_SEMANTIC=0)");
    return -1;
  }
  if (k <= 0) k = svec_topk();

  /* Which kinds were asked for, and which of them may answer. */
  unsigned want = 0, ready = 0;
  if (f && f->kind && *f->kind) {
    int kd = source_kind_parse(f->kind);
    if (kd < 0) { info_reason(info, "unknown kind"); return -1; }
    want = 1u << kd;
  } else want = (1u << SC_NKINDS) - 1;
  if (!vecindex_table_exists(db, SVEC_META_TABLE)) {
    info_reason(info, "the service index has not been built yet");
    if (info) info->unready_kinds = want;
    return -1;
  }
  for (int i = 0; i < SC_NKINDS; i++)
    if ((want & (1u << i)) && svec_kind_ready(db, i)) ready |= 1u << i;
  int total = 0;
  {
    const source_card *c = NULL;
    int n = source_cards_all(&c);
    for (int i = 0; c && i < n; i++)
      if (c[i].text && (ready & (1u << c[i].kind))) total++;
  }
  if (info) {
    info->searched_kinds = ready;
    info->unready_kinds = want & ~ready;
    info->total = total;
  }
  if (!ready) {
    info_reason(info, "the service index is still being built for the kinds "
                      "asked for");
    return -1;
  }

  /* The index answers only for the model it was built with. The build pod
   * checks this every 15 minutes; a swap in between would otherwise route by
   * comparing vectors from two spaces until it noticed. */
  {
    char live[256];
    char *have = svec_meta_get(db, "model");
    int same = have && *have && embed_live_query_model(live, sizeof live) == 0 &&
               !strcmp(have, live);
    if (!same) {
      fprintf(stderr, "[svec] index model '%s' is not the server's (%s) — "
                      "refusing to rank\n", have ? have : "(not recorded)",
              have ? live : "-");
      info_reason(info, "the embedding server's model is not the one the "
                        "index was built with");
    }
    free(have);
    if (!same) return -1;
  }

  /* The query gets the QUERY prefix; the corpus got the DOC prefix at build
   * time. Bounded after prefixing so the bound covers what is actually sent. */
  const char *qpx = svec_query_prefix();
  char *qtmp = NULL;
  if (*qpx) {
    size_t qn = strlen(qpx) + strlen(query) + 1;
    qtmp = malloc(qn);
    if (qtmp) snprintf(qtmp, qn, "%s%s", qpx, query);
  }
  char *qb = embed_bound_text(qtmp ? qtmp : query);
  free(qtmp);
  if (!qb) { info_reason(info, "out of memory"); return -1; }
  /* bound_queue_wait: this runs inside a user's search, and the embedding
   * worker is shared with the backfill pod's 120 s batches. */
  llm_client llm = { .http = NULL, .base_url = embed_query_url(), .interactive = 1,
                     .bound_queue_wait = 1 };
  const char *texts[1] = { qb };
  float *qv = NULL; int qdim = 0; llm_status lst;
  int rc = llm_embed(&llm, texts, 1, &qv, &qdim, 15000, &lst);
  free(qb);
  if (rc != 0 || !qv) {
    fprintf(stderr, "[svec] query embedding failed (%s)\n", llm_status_code(lst));
    info_reason(info, "the query could not be embedded");
    free(qv);
    return -1;
  }
  int dim = (int)vecindex_meta_long(db, &svec_spec, "dim");
  if (dim && qdim != dim) {
    fprintf(stderr, "[svec] query is %d-d, index is %d-d — refusing to compare\n",
            qdim, dim);
    info_reason(info, "the query's dimension is not the index's");
    free(qv);
    return -1;
  }

  /* ---- the two arms, fused ------------------------------------------------
   * Vector KNN first (restricted to the ready kinds by the partition key),
   * then the keyword index over the same cards and kinds, then reciprocal-
   * rank fusion. Each arm contributes at most `k` candidates and the fused
   * list is cut back to `k`. */
  vecindex_fused cands = {0};
  {
    vecindex_val kv[SC_NKINDS];
    int nkv = 0;
    for (int i = 0; i < SC_NKINDS; i++)
      if (ready & (1u << i)) kv[nkv++] = VI_S(source_kind_name((source_kind)i));
    vecindex_filter kf = { .col = "kind", .op = nkv == 1 ? "=" : "IN",
                           .vals = kv, .nvals = nkv };
    vecindex_hit *h = NULL;
    char err[200] = {0};
    int nh = vecindex_knn(db, &svec_spec, qv, qdim, k, &kf, 1, &h, err, sizeof err);
    free(qv);
    if (nh < 0) {
      fprintf(stderr, "[svec] knn: %s\n", err);
      info_reason(info, "the vector index could not be queried");
      return -1;
    }
    vecindex_fused_add_hits(&cands, h, nh);
    vecindex_hits_free(h, nh);
    if (info) info->semantic = nh;
  }
  char *fexpr = svec_fts_expr(query);
  if (fexpr) {
    int fl = svec_fts_arm(db, fexpr, ready, k, &cands);
    if (fl > 0 && info) info->lexical = fl;
  }
  free(fexpr);
  vecindex_fused_sort(&cands);

  service_vec_hit *hits = calloc((size_t)(k > 0 ? k : 1), sizeof *hits);
  int n = 0;
  for (int i = 0; hits && i < cands.n && n < k; i++) {
    /* registry_get() through the card: a key the registry no longer has is
     * skipped, never reported as a source that exists. */
    if (!source_card_get(cands.v[i].key)) continue;
    hits[n].id = strdup(cands.v[i].key);
    if (!hits[n].id) break;
    hits[n].score = cands.v[i].score;
    hits[n].vrank = cands.v[i].vrank;
    hits[n].frank = cands.v[i].frank;
    hits[n].distance = cands.v[i].distance;
    n++;
  }
  vecindex_fused_free(&cands);
  if (!hits) { info_reason(info, "out of memory"); return -1; }
  *out = hits;
  return n;
}

char *service_vec_catalogue(db_handle *db, const char *query, int k,
                            osint_catalogue_note *note,
                            char ***out_ids, int *out_n) {
  return service_vec_catalogue_bounded(db, query, k, 0, note, out_ids, out_n);
}

char *service_vec_catalogue_bounded(db_handle *db, const char *query, int k,
                                    size_t max_bytes,
                                    osint_catalogue_note *note,
                                    char ***out_ids, int *out_n) {
  service_vec_filter f = { .kind = "pivot" };
  return service_vec_catalogue_filtered(db, query, k, max_bytes, &f, note,
                                        out_ids, out_n);
}

char *service_vec_catalogue_filtered(db_handle *db, const char *query, int k,
                                     size_t max_bytes,
                                     const service_vec_filter *filter,
                                     osint_catalogue_note *note,
                                     char ***out_ids, int *out_n) {
  if (out_ids) *out_ids = NULL;
  if (out_n) *out_n = 0;
  if (!db || !query || !*query || !svec_enabled()) return NULL;

  if (k <= 0) k = svec_topk();
  const char *kind = (filter && filter->kind && *filter->kind) ? filter->kind : NULL;
  int is_pivot = kind && !strcmp(kind, "pivot");

  /* The index is built by the _maint pod (core/service_vec_pod.c), NOT on this
   * thread. Building it costs one embedding request per 32 cards — measured
   * twice on 2026-09-11 at 273 s and 280 s for the 1,838 pivots of the time
   * against multilingual-e5-large on CPU — and this function is called from
   * the search request path. Doing it inline stalls the first search after
   * any registry change for minutes while the user watches a spinner. Not-
   * ready is the documented fallback: registry order, with the bound
   * disclosed in-band, exactly as when no embedding server is configured.
   *
   * JO_ROUTE_BUILD_INLINE=1 restores the old behaviour for a context that has
   * no scheduler — a one-shot CLI run, or a test. */
  if (is_pivot && vecindex_table_exists(db, SVEC_META_TABLE) &&
      svec_kind_ready(db, SC_KIND_PIVOT)) {
    /* ready: fall through */
  } else if (is_pivot) {
    const char *inl = getenv("JO_ROUTE_BUILD_INLINE");
    if (inl && inl[0] == '1' && inl[1] == 0) {
      if (service_vec_build(db) <= 0) return NULL;
      if (!svec_kind_ready(db, SC_KIND_PIVOT)) return NULL;
    } else {
      static int told = 0;
      if (!told) {
        told = 1;
        fprintf(stderr, "[svec] service index not ready yet — routing by "
                        "registry order until the _maint pod finishes "
                        "building it\n");
      }
      return NULL;
    }
  }

  service_vec_hit *hits = NULL;
  service_vec_info info;
  int nc = service_vec_search(db, query, filter, k, &hits, &info);
  if (nc <= 0) { service_vec_hits_free(hits, nc > 0 ? nc : 0); return NULL; }

  char **ids = calloc((size_t)k, sizeof *ids);
  int n = 0, trimmed = 0;
  size_t cap = 4096, len = 0;
  /* The disclosure line appended below is at most ~400 bytes; the budget, when
   * there is one, has to leave room for it. */
  const size_t tail_room = 420;
  char *out = malloc(cap);
  if (!ids || !out) {
    free(ids); free(out);
    service_vec_hits_free(hits, nc);
    return NULL;
  }
  out[0] = 0;

  for (int i = 0; i < nc && n < k; i++) {
    const char *id = hits[i].id;
    /* registry_get() is the canonical id lookup and is what the dispatcher
     * itself uses, so a service the index names resolves the same way here as
     * it will when it is actually dispatched. */
    const source_def *d = registry_get(id);
    if (!d) continue;                       /* registry moved under us */
    const char *ds = d->description ? d->description : "";
    size_t need = strlen(id) + strlen(ds) + 8;
    /* THE PROMPT BUDGET APPLIES HERE TOO. Only the registry-order fallback
     * used to be budgeted; 60 semantic cards WITH descriptions went in
     * unmeasured and could overflow a 16k context (400 -> analysis degraded).
     * K is cut to what fits, and the cut is disclosed in the tail below. */
    if (max_bytes && len + need + tail_room > max_bytes) { trimmed = 1; break; }
    if (len + need + 1 > cap) {
      size_t ncap = (len + need + 1) * 2;
      char *nb = realloc(out, ncap);
      if (!nb) break;
      out = nb; cap = ncap;
    }
    len += (size_t)snprintf(out + len, cap - len, "%s: %s\n", id, ds);
    ids[n++] = strdup(id);
  }
  int lex = info.lexical;
  service_vec_hits_free(hits, nc);

  if (n == 0) { free(ids); free(out); return NULL; }

  int total = info.total;

  /* State the bound in-band. The model is being shown a SUBSET and must know
   * it is a subset and why, exactly as the registry-order path says so — a
   * bounded view that does not announce its bound is the violation rule 2 is
   * about. */
  char what[64];
  if (is_pivot) snprintf(what, sizeof what, "entity-pivot services");
  else if (kind) snprintf(what, sizeof what, "%s sources", kind);
  else snprintf(what, sizeof what, "sources");
  char tail[400];
  int tl = snprintf(tail, sizeof tail,
    "\n(%d of %d registered %s, selected as the closest matches to this query "
    "by embedding similarity%s rather than by registry order%s. Services not "
    "listed exist and may be relevant; ask for a different phrasing if none of "
    "these fit.)\n", n, total, what,
    lex ? " fused with keyword match" : "",
    trimmed ? ", cut to fit the model's context" : "");
  if (tl > 0 && len + (size_t)tl + 1 > cap) {
    char *nb = realloc(out, len + (size_t)tl + 1);
    if (nb) { out = nb; cap = len + (size_t)tl + 1; }
  }
  if (tl > 0 && len + (size_t)tl + 1 <= cap) snprintf(out + len, cap - len, "%s", tail);

  if (note) {
    note->total = total;
    note->shown = n;
    note->descriptions = 1;               /* the point: descriptions survive */
    note->truncated = (n < total);
  }
  if (out_ids && out_n) { *out_ids = ids; *out_n = n; }
  else service_vec_free_ids(ids, n);
  return out;
}

int service_vec_route_sources(db_handle *db, const char *query, const char *kind,
                              int k, char ***out_ids) {
  if (out_ids) *out_ids = NULL;
  if (!out_ids) return -1;
  service_vec_filter f = { .kind = (kind && *kind) ? kind : "scheduled" };
  service_vec_hit *hits = NULL;
  int n = service_vec_search(db, query, &f, k > 0 ? k : 10, &hits, NULL);
  if (n < 0) return -1;
  char **ids = calloc((size_t)(n > 0 ? n : 1), sizeof *ids);
  if (!ids) { service_vec_hits_free(hits, n); return -1; }
  int m = 0;
  for (int i = 0; i < n; i++) {
    /* hand the id over rather than copying it */
    ids[m++] = hits[i].id;
    hits[i].id = NULL;
  }
  service_vec_hits_free(hits, n);
  *out_ids = ids;
  return m;
}

cJSON *service_vec_coverage_json(db_handle *db) {
  cJSON *o = vecindex_coverage_json(db, &svec_spec);
  cJSON *kinds = cJSON_CreateObject();
  int have = db && vecindex_table_exists(db, SVEC_META_TABLE);
  for (int i = 0; i < SC_NKINDS; i++) {
    int cnt = 0;
    svec_kind_sig(i, &cnt);
    cJSON *k = cJSON_CreateObject();
    cJSON_AddNumberToObject(k, "registered", cnt);
    cJSON_AddBoolToObject(k, "ready", have && svec_kind_ready(db, i));
    cJSON_AddItemToObject(kinds, source_kind_name((source_kind)i), k);
  }
  cJSON_AddItemToObject(o, "kinds", kinds);
  return o;
}
