/* core/service_vec.c — see service_vec.h for why this exists. */
#include "service_vec.h"
#include "embed_pod.h"
#include "llm.h"
#include "../source.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

/* One embedding request per this many services. Same default as the intel
 * pod: large enough that ~1,500 services is ~48 requests, small enough that a
 * failure costs little and the server is not handed a huge body. */
#define SVEC_BATCH 32
/* Services listed for a query. 60 × (id + description) lands around 12 KB —
 * well inside the 32 KB prompt budget, with the descriptions KEPT, which is
 * the whole point: the registry-order path had to drop them at this size. */
#define SVEC_TOPK_DEF 60

/* Rank-fusion constant, the same value and meaning as semsearchapi.c's RRF_K:
 * a rank-1 hit contributes 1/61, rank-10 1/70, so a service both arms like
 * outranks one that only one arm likes, and neither arm's score scale (cosine
 * distance vs bm25) has to be normalised against the other. */
#define SVEC_RRF_K 60.0

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
 * unconfigured deployment behaves exactly as before. The pair is recorded in
 * service_vec_meta and a change to either rebuilds the index — embedding the
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
 * `is_entity_pivot` in osint_dispatch.c is static, and its rule is the one
 * this index must agree with exactly — indexing a different set than the
 * catalogue lists would route to services the prompt never mentions. The rule
 * is house rule 3's own distinction and is restated here rather than
 * duplicated loosely: collector=="osint" AND update_interval_sec==0. If that
 * definition ever moves, both copies must move together. */
static int svec_is_pivot(const source_def *d) {
  return d && d->collector && strcmp(d->collector, "osint") == 0
         && d->update_interval_sec == 0;
}

/* A cheap signature of the registered pivot set: count plus an FNV-1a of the
 * ids in registry order. Any add, removal or rename changes it, which is what
 * triggers a rebuild — the index must never answer from a registry that no
 * longer exists. */
static unsigned long long svec_registry_sig(int *out_count) {
  unsigned long long h = 1469598103934665603ULL;
  const source_def **all = registry_all();
  int n = 0, total = registry_count();
  for (int i = 0; i < total; i++) {
    const source_def *d = all[i];
    if (!svec_is_pivot(d) || !d->id) continue;
    for (const char *p = d->id; *p; p++) {
      h ^= (unsigned char)*p;
      h *= 1099511628211ULL;
    }
    h ^= 0xff; h *= 1099511628211ULL;
    n++;
  }
  if (out_count) *out_count = n;
  return h;
}

/* The text a service is indexed BY. Name and description are the same prose
 * the catalogue shows the model, so what the index matches on is what the
 * model will read — not a private summary that could rank a service the
 * briefing then fails to justify. */
static char *svec_text(const source_def *d) {
  const char *nm = d->name ? d->name : d->id;
  const char *ds = d->description ? d->description : "";
  const char *px = svec_doc_prefix();
  size_t n = strlen(px) + strlen(d->id) + strlen(nm) + strlen(ds) + 10;
  char *t = malloc(n);
  if (!t) return NULL;
  snprintf(t, n, "%s%s: %s. %s", px, d->id, nm, ds);
  char *b = embed_bound_text(t);       /* same bound the query will use */
  free(t);
  return b;
}

/* ---- meta ---------------------------------------------------------------- */

static void svec_meta_set(db_handle *db, const char *k, const char *v) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO " SVEC_META_TABLE "(k,v) VALUES(?1,?2) "
        "ON CONFLICT(k) DO UPDATE SET v=excluded.v", -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(s, 1, k, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, v, -1, SQLITE_STATIC);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

static char *svec_meta_get(db_handle *db, const char *k) {
  sqlite3_stmt *s = NULL;
  char *out = NULL;
  if (sqlite3_prepare_v2(db->h, "SELECT v FROM " SVEC_META_TABLE " WHERE k=?1",
                         -1, &s, NULL) != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, k, -1, SQLITE_STATIC);
  if (sqlite3_step(s) == SQLITE_ROW) {
    const unsigned char *v = sqlite3_column_text(s, 0);
    if (v) out = strdup((const char *)v);
  }
  sqlite3_finalize(s);
  return out;
}

/* ---- build --------------------------------------------------------------- */

int service_vec_build(db_handle *db) {
  if (!db || !svec_enabled()) return 0;

  sqlite3_exec(db->h, "CREATE TABLE IF NOT EXISTS " SVEC_META_TABLE
               "(k TEXT PRIMARY KEY, v TEXT)", NULL, NULL, NULL);

  int want = 0;
  unsigned long long sig = svec_registry_sig(&want);
  if (want <= 0) return 0;

  char sigs[32];
  snprintf(sigs, sizeof sigs, "%llu", sig);

  llm_client llm = { .http = NULL, .base_url = embed_base_url(), .interactive = 0 };

  /* The model name, when the operator states it. embed_index_dim() must NOT be
   * used here: it reads the INTEL pod's intel_vec_meta, a different index with
   * its own lifecycle, so a service index would inherit that pod's model and
   * never notice its own changing. (It did exactly that until
   * test_service_vec.c's model-change case caught it.) */
  const char *envm = getenv("JO_EMBED_MODEL");
  char model[128] = {0};
  if (envm && *envm) snprintf(model, sizeof model, "%s", envm);

  /* Already current? The registry signature, the model name (when known) and
   * the DIMENSION must all match. The dimension is verified against the server
   * as it is right now — one short probe embed — because that is the check
   * that cannot be fooled: a different model behind the same URL is a
   * different vector space, and a nearest neighbour across two spaces is a
   * number, not a similarity (embed_pod.h refuses for the same reason). A
   * probe costs one tiny request per build check and only when an index
   * already exists. */
  char *have_sig = svec_meta_get(db, "registry_sig");
  char *have_model = svec_meta_get(db, "model");
  char *have_dim = svec_meta_get(db, "dim");
  char *have_state = svec_meta_get(db, "state");
  char *have_px = svec_meta_get(db, "doc_prefix");
  int px_same = (have_px ? !strcmp(have_px, svec_doc_prefix())
                         : !*svec_doc_prefix());
  int current = 0;
  if (have_sig && !strcmp(have_sig, sigs) &&
      have_state && !strcmp(have_state, "ready") && px_same &&
      (!model[0] || !have_model || !strcmp(have_model, model))) {
    const char *probe[1] = { "probe" };
    float *pv = NULL; int pdim = 0; llm_status pst;
    if (llm_embed(&llm, probe, 1, &pv, &pdim, 15000, &pst) == 0 &&
        have_dim && pdim == atoi(have_dim))
      current = 1;
    else if (have_dim && pdim > 0 && pdim != atoi(have_dim))
      fprintf(stderr, "[svec] embedding server now answers %d-d, index is %s-d "
                      "— rebuilding rather than mixing spaces\n", pdim, have_dim);
    free(pv);
  }
  free(have_sig); free(have_model); free(have_dim); free(have_state); free(have_px);
  if (current) return want;

  /* Collect the pivots once so the batch loop and the insert agree on order. */
  const source_def **svc = calloc((size_t)want, sizeof *svc);
  if (!svc) return -1;
  const source_def **all = registry_all();
  int n = 0, total = registry_count();
  for (int i = 0; i < total && n < want; i++) {
    const source_def *d = all[i];
    if (svec_is_pivot(d) && d->id) svc[n++] = d;
  }

  /* All-or-nothing WITHOUT renaming the table.
   *
   * The obvious shape — build into service_vec_new, then ALTER TABLE RENAME on
   * success — does not work for a sqlite-vec table. vec0 owns shadow tables
   * whose names derive from the virtual table's name, and renaming the virtual
   * table does not rename them, so the renamed table is no longer queryable.
   * tests/unit/test_service_vec.c caught exactly that: the build reported
   * 1,838 services indexed and the very next KNN query returned nothing.
   *
   * So the table is built in place and `state` carries the guarantee instead:
   * "building" while rows go in, "ready" only after the last one. The query
   * path refuses anything not "ready" for the CURRENT registry signature, so a
   * failed or interrupted build is never queried — which is the property that
   * actually matters, since a partial index answers confidently from whatever
   * fraction happened to embed. */
  int dim = 0, indexed = 0, failed = 0;
  svec_meta_set(db, "state", "building");
  sqlite3_exec(db->h, "DROP TABLE IF EXISTS " SVEC_TABLE, NULL, NULL, NULL);

  /* The LEXICAL arm of the router, built from the same rows in the same pass
   * so the two arms always describe the same corpus (an id in one and not the
   * other would make a fused rank meaningless).
   *
   * Why there is a second arm at all — measured 2026-09-11 against
   * multilingual-e5-large over all 1,838 pivots, at the production K of 60:
   * "who owns example.com" put DNS_RECORDS at 57 and did NOT return
   * DOMAIN_WHOIS at all. A longer phrasing of the same question ranks it
   * first (the A/B recorded above the prefix knob), so the vector arm is not
   * broken — it is simply weak on a terse query whose words do not appear in
   * the description. The word "whois" IS in that service's card, which is
   * exactly what a keyword index is for. Semantic recall and lexical recall
   * fail on different queries; fusing them is cheaper and more honest than
   * tuning either alone. */
  sqlite3_exec(db->h, "DROP TABLE IF EXISTS " SVEC_FTS_TABLE, NULL, NULL, NULL);
  int fts_ok = sqlite3_exec(db->h,
      "CREATE VIRTUAL TABLE " SVEC_FTS_TABLE " USING fts5("
      "id UNINDEXED, text, tokenize='unicode61')", NULL, NULL, NULL) == SQLITE_OK;
  if (!fts_ok)
    fprintf(stderr, "[svec] no fts5 here — routing runs on the vector arm "
                    "alone (recall is lower on terse queries)\n");

  for (int off = 0; off < n && !failed; off += SVEC_BATCH) {
    int cnt = (n - off < SVEC_BATCH) ? (n - off) : SVEC_BATCH;
    const char **texts = calloc((size_t)cnt, sizeof *texts);
    char **owned = calloc((size_t)cnt, sizeof *owned);
    if (!texts || !owned) { free((void *)texts); free(owned); failed = 1; break; }
    int m = 0;
    for (int j = 0; j < cnt; j++) {
      owned[m] = svec_text(svc[off + j]);
      if (owned[m]) { texts[m] = owned[m]; m++; }
    }
    float *vecs = NULL; int vdim = 0; llm_status st;
    int rc = m ? llm_embed(&llm, texts, m, &vecs, &vdim, 30000, &st) : -1;
    if (rc != 0 || vdim <= 0) {
      fprintf(stderr, "[svec] embedding failed at %d/%d (%s) — index not built\n",
              off, n, m ? llm_status_code(st) : "no text");
      for (int j = 0; j < m; j++) free(owned[j]);
      free((void *)texts); free(owned); free(vecs);
      failed = 1;
      break;
    }
    if (!dim) {
      dim = vdim;
      char sql[256];
      snprintf(sql, sizeof sql,
        "CREATE VIRTUAL TABLE " SVEC_TABLE " USING vec0("
        "id TEXT PRIMARY KEY, embedding float[%d])", dim);
      if (sqlite3_exec(db->h, sql, NULL, NULL, NULL) != SQLITE_OK) failed = 1;
    } else if (vdim != dim) {
      /* One model must not change dimension mid-build. */
      fprintf(stderr, "[svec] server answered %d-d after %d-d — refusing\n", vdim, dim);
      failed = 1;
    }
    if (!failed) {
      sqlite3_stmt *ins = NULL;
      if (sqlite3_prepare_v2(db->h, "INSERT INTO " SVEC_TABLE "(id,embedding) "
                             "VALUES(?1,?2)", -1, &ins, NULL) == SQLITE_OK) {
        sqlite3_stmt *fins = NULL;
        if (fts_ok)
          sqlite3_prepare_v2(db->h, "INSERT INTO " SVEC_FTS_TABLE "(id,text) "
                             "VALUES(?1,?2)", -1, &fins, NULL);
        for (int j = 0; j < m; j++) {
          sqlite3_reset(ins);
          sqlite3_bind_text(ins, 1, svc[off + j]->id, -1, SQLITE_STATIC);
          sqlite3_bind_blob(ins, 2, vecs + (size_t)j * dim,
                            (int)(dim * sizeof(float)), SQLITE_STATIC);
          if (sqlite3_step(ins) == SQLITE_DONE) indexed++;
          if (fins) {
            /* The same card the model is shown and the vector arm embedded —
             * id, name, description — minus the embedding prefix, which is a
             * model instruction and not part of the service. */
            const source_def *d = svc[off + j];
            const char *nm = d->name ? d->name : "";
            const char *ds = d->description ? d->description : "";
            size_t tn = strlen(d->id) + strlen(nm) + strlen(ds) + 4;
            char *t = malloc(tn);
            if (t) {
              snprintf(t, tn, "%s %s %s", d->id, nm, ds);
              sqlite3_reset(fins);
              sqlite3_bind_text(fins, 1, d->id, -1, SQLITE_STATIC);
              sqlite3_bind_text(fins, 2, t, -1, SQLITE_TRANSIENT);
              sqlite3_step(fins);
              free(t);
            }
          }
        }
        if (fins) sqlite3_finalize(fins);
        sqlite3_finalize(ins);
      } else failed = 1;
    }
    for (int j = 0; j < m; j++) free(owned[j]);
    free((void *)texts); free(owned); free(vecs);
  }
  free(svc);

  if (failed || indexed <= 0) {
    /* `state` stays "building": whatever rows landed are on disk but the query
     * path will not touch them, and the next run rebuilds. The reason is
     * recorded as data, not only on stderr. */
    svec_meta_set(db, "last_error",
                  "build failed; index marked not-ready and will be rebuilt");
    return -1;
  }

  char buf[64];
  svec_meta_set(db, "registry_sig", sigs);
  snprintf(buf, sizeof buf, "%d", indexed); svec_meta_set(db, "count", buf);
  snprintf(buf, sizeof buf, "%d", dim);     svec_meta_set(db, "dim", buf);
  if (model[0]) svec_meta_set(db, "model", model);
  svec_meta_set(db, "doc_prefix", svec_doc_prefix());
  svec_meta_set(db, "last_error", "");
  svec_meta_set(db, "state", "ready");   /* last: everything above landed */
  fprintf(stderr, "[svec] indexed %d of %d entity-pivot services (%d-d)\n",
          indexed, n, dim);
  return indexed;
}

/* ---- query --------------------------------------------------------------- */

void service_vec_free_ids(char **ids, int n) {
  if (!ids) return;
  for (int i = 0; i < n; i++) free(ids[i]);
  free(ids);
}

/* registry_get() is the canonical id lookup and is what the dispatcher itself
 * uses, so a service the index names resolves the same way here as it will
 * when it is actually dispatched. */
static const source_def *svec_lookup(const char *id) {
  return registry_get(id);
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

/* READY means: the all-or-nothing build completed (state=ready) AND it was
 * built for the registry as it stands right now. A failed or interrupted
 * build leaves rows on disk; answering from those is answering from an
 * arbitrary fraction of the catalogue, which is the failure this module was
 * written to avoid. */
static int svec_ready_fresh(db_handle *db) {
  char *state = svec_meta_get(db, "state");
  int ready = state && !strcmp(state, "ready");
  free(state);
  if (!ready) return 0;
  int now = 0;
  char cur[32];
  snprintf(cur, sizeof cur, "%llu", svec_registry_sig(&now));
  char *sg = svec_meta_get(db, "registry_sig");
  int fresh = sg && !strcmp(sg, cur);
  free(sg);
  return fresh;
}

static int svec_fts_ready(db_handle *db) {
  sqlite3_stmt *s = NULL;
  int ok = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='"
        SVEC_FTS_TABLE "'", -1, &s, NULL) == SQLITE_OK) {
    ok = (sqlite3_step(s) == SQLITE_ROW);
    sqlite3_finalize(s);
  }
  return ok;
}

/* One candidate service, with its rank in each arm. rank 0 = that arm did not
 * return it, which is why the score is summed from the ranks that exist
 * rather than requiring both. */
typedef struct { char *id; int vrank, frank; double score; } svec_cand;

static void svec_cand_add(svec_cand *c, int *n, int cap, const char *id,
                          int vrank, int frank) {
  for (int i = 0; i < *n; i++) {
    if (strcmp(c[i].id, id)) continue;
    if (vrank && !c[i].vrank) { c[i].vrank = vrank; c[i].score += 1.0 / (SVEC_RRF_K + vrank); }
    if (frank && !c[i].frank) { c[i].frank = frank; c[i].score += 1.0 / (SVEC_RRF_K + frank); }
    return;
  }
  if (*n >= cap) return;
  char *dup = strdup(id);
  if (!dup) return;
  c[*n].id = dup;
  c[*n].vrank = vrank;
  c[*n].frank = frank;
  c[*n].score = (vrank ? 1.0 / (SVEC_RRF_K + vrank) : 0.0)
              + (frank ? 1.0 / (SVEC_RRF_K + frank) : 0.0);
  (*n)++;
}

static int svec_cand_cmp(const void *a, const void *b) {
  const svec_cand *x = a, *y = b;
  if (x->score > y->score) return -1;
  if (x->score < y->score) return 1;
  /* Deterministic tie-break, so the same query twice lists the same services
   * in the same order: the vector rank, then the id. */
  int xv = x->vrank ? x->vrank : 1 << 20, yv = y->vrank ? y->vrank : 1 << 20;
  if (xv != yv) return xv < yv ? -1 : 1;
  return strcmp(x->id, y->id);
}

char *service_vec_catalogue(db_handle *db, const char *query, int k,
                            osint_catalogue_note *note,
                            char ***out_ids, int *out_n) {
  if (out_ids) *out_ids = NULL;
  if (out_n) *out_n = 0;
  if (!db || !query || !*query || !svec_enabled()) return NULL;

  if (k <= 0) k = svec_topk();

  /* The index is built by the _maint pod (core/service_vec_pod.c), NOT on this
   * thread. Building it costs one embedding request per 32 services —
   * measured twice on 2026-09-11 at 273 s and 280 s for 1,838 services against
   * multilingual-e5-large on CPU — and this function is called from the
   * search request path. Doing it inline stalls the first search after any
   * registry change for minutes while the user watches a spinner, and a
   * request that times out mid-build leaves the work to be redone by the next
   * one. Not-ready is the documented fallback: registry order, with the bound
   * disclosed in-band, exactly as when no embedding server is configured.
   *
   * JO_ROUTE_BUILD_INLINE=1 restores the old behaviour for a context that has
   * no scheduler — a one-shot CLI run, or a test. */
  if (!svec_ready_fresh(db)) {
    const char *inl = getenv("JO_ROUTE_BUILD_INLINE");
    if (inl && inl[0] == '1' && inl[1] == 0) {
      if (service_vec_build(db) <= 0) return NULL;
      if (!svec_ready_fresh(db)) return NULL;
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
  if (!qb) return NULL;
  llm_client llm = { .http = NULL, .base_url = embed_base_url(), .interactive = 1 };
  const char *texts[1] = { qb };
  float *qv = NULL; int qdim = 0; llm_status st;
  int rc = llm_embed(&llm, texts, 1, &qv, &qdim, 15000, &st);
  free(qb);
  if (rc != 0 || !qv) {
    fprintf(stderr, "[svec] query embedding failed (%s) — falling back\n",
            llm_status_code(st));
    free(qv);
    return NULL;
  }

  char *dims = svec_meta_get(db, "dim");
  int dim = dims ? atoi(dims) : 0;
  free(dims);
  if (dim && qdim != dim) {
    fprintf(stderr, "[svec] query is %d-d, index is %d-d — refusing to compare\n",
            qdim, dim);
    free(qv);
    return NULL;
  }

  /* ---- the two arms, fused ------------------------------------------------
   * Vector KNN first, then the keyword index over the same cards, then
   * reciprocal-rank fusion. Each arm contributes at most `k` candidates and
   * the fused list is cut back to `k`, so the prompt budget is unchanged. */
  svec_cand *cands = calloc((size_t)k * 2, sizeof *cands);
  int nc = 0;
  if (!cands) { free(qv); return NULL; }

  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT id FROM " SVEC_TABLE " WHERE embedding MATCH ?1 AND k = ?2 "
        "ORDER BY distance", -1, &s, NULL) != SQLITE_OK) {
    free(cands);
    free(qv);
    return NULL;
  }
  sqlite3_bind_blob(s, 1, qv, (int)(qdim * sizeof(float)), SQLITE_STATIC);
  sqlite3_bind_int(s, 2, k);
  for (int rank = 1; sqlite3_step(s) == SQLITE_ROW; rank++) {
    const unsigned char *id = sqlite3_column_text(s, 0);
    if (id) svec_cand_add(cands, &nc, k * 2, (const char *)id, rank, 0);
  }
  sqlite3_finalize(s);
  free(qv);

  int lex = 0;
  char *fexpr = svec_fts_expr(query);
  if (fexpr && svec_fts_ready(db)) {
    sqlite3_stmt *f = NULL;
    if (sqlite3_prepare_v2(db->h,
          "SELECT id FROM " SVEC_FTS_TABLE " WHERE " SVEC_FTS_TABLE
          " MATCH ?1 ORDER BY bm25(" SVEC_FTS_TABLE ") LIMIT ?2",
          -1, &f, NULL) == SQLITE_OK) {
      sqlite3_bind_text(f, 1, fexpr, -1, SQLITE_STATIC);
      sqlite3_bind_int(f, 2, k);
      for (int rank = 1; sqlite3_step(f) == SQLITE_ROW; rank++) {
        const unsigned char *id = sqlite3_column_text(f, 0);
        if (id) { svec_cand_add(cands, &nc, k * 2, (const char *)id, 0, rank); lex++; }
      }
      sqlite3_finalize(f);
    }
  }
  free(fexpr);

  qsort(cands, (size_t)nc, sizeof *cands, svec_cand_cmp);

  char **ids = calloc((size_t)k, sizeof *ids);
  int n = 0;
  size_t cap = 4096, len = 0;
  char *out = malloc(cap);
  if (!ids || !out) {
    free(ids); free(out);
    for (int i = 0; i < nc; i++) free(cands[i].id);
    free(cands);
    return NULL;
  }
  out[0] = 0;

  for (int i = 0; i < nc && n < k; i++) {
    const char *id = cands[i].id;
    const source_def *d = svec_lookup(id);
    if (!d) continue;                       /* registry moved under us */
    const char *ds = d->description ? d->description : "";
    size_t need = strlen(id) + strlen(ds) + 8;
    if (len + need + 1 > cap) {
      size_t ncap = (len + need + 1) * 2;
      char *nb = realloc(out, ncap);
      if (!nb) break;
      out = nb; cap = ncap;
    }
    len += (size_t)snprintf(out + len, cap - len, "%s: %s\n", id, ds);
    ids[n++] = strdup(id);
  }
  for (int i = 0; i < nc; i++) free(cands[i].id);
  free(cands);

  if (n == 0) { free(ids); free(out); return NULL; }

  int total = 0;
  svec_registry_sig(&total);

  /* State the bound in-band. The model is being shown a SUBSET and must know
   * it is a subset and why, exactly as the registry-order path says so — a
   * bounded view that does not announce its bound is the violation rule 2 is
   * about. */
  char tail[400];
  int tl = snprintf(tail, sizeof tail,
    "\n(%d of %d registered entity-pivot services, selected as the closest "
    "matches to this query by embedding similarity%s rather than by registry "
    "order. Services not listed exist and may be relevant; ask for a different "
    "phrasing if none of these fit.)\n", n, total,
    lex ? " fused with keyword match" : "");
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
