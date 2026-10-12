/* core/source_category.c — see source_category.h. */
#include "source_category.h"
#include "service_vec.h"
#include "source_card.h"
#include "../lib/vecindex.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>

/* source.h's list, verbatim and in its order. test_source_category.c checks
 * that every value still appears in source.h's comment, so the two cannot
 * drift without a failing test. */
const char *const SOURCE_CATEGORIES[SOURCE_CATEGORY_COUNT] = {
  "agriculture", "breach", "classifieds", "commercial", "crime",
  "culture", "cyber", "defense", "economy", "environment",
  "food", "geospatial", "government", "health", "industry",
  "infrastructure", "intelligence", "investigation", "marketplace", "media",
  "news", "ocean", "safety", "satellite", "seismic",
  "social", "statistics", "telecom", "tourism", "transport",
  "wildlife" };

/* Neighbours that vote. 15 is enough that one odd neighbour cannot decide a
 * label, and few enough that the vote stays local: a "research" source sits
 * among other research-adjacent sources, not the whole government bloc. */
#define SCAT_K 15

#define SCAT_META "source_category_meta"

int source_category_is_canonical(const char *c) {
  if (!c) return 0;
  for (int i = 0; i < SOURCE_CATEGORY_COUNT; i++)
    if (!strcmp(SOURCE_CATEGORIES[i], c)) return 1;
  return 0;
}

static int cat_index(const char *c) {
  for (int i = 0; c && i < SOURCE_CATEGORY_COUNT; i++)
    if (!strcmp(SOURCE_CATEGORIES[i], c)) return i;
  return -1;
}

static void meta_ensure(db_handle *db) {
  sqlite3_exec(db->h, "CREATE TABLE IF NOT EXISTS " SCAT_META
               "(k TEXT PRIMARY KEY, v TEXT)", NULL, NULL, NULL);
}

static char *meta_get(db_handle *db, const char *k) {
  sqlite3_stmt *s = NULL;
  char *out = NULL;
  if (sqlite3_prepare_v2(db->h, "SELECT v FROM " SCAT_META " WHERE k=?1", -1,
                         &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, k, -1, SQLITE_STATIC);
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0))
    out = strdup((const char *)sqlite3_column_text(s, 0));
  sqlite3_finalize(s);
  return out;
}

static void meta_set(db_handle *db, const char *k, const char *v) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, "INSERT INTO " SCAT_META "(k,v) VALUES(?1,?2) "
                         "ON CONFLICT(k) DO UPDATE SET v=excluded.v", -1, &s,
                         NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, k, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, v, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* ---- declared ------------------------------------------------------------ */

int source_category_declare(db_handle *db) {
  if (!db || !db->h) return -1;
  const source_card *c = NULL;
  int n = source_cards_all(&c), changed = 0, ok = 1;
  if (!c) return 0;
  /* Write only what differs, so a steady tick is reads. A source that
   * stopped declaring a canonical category loses its declared label (a
   * derived one, if any, comes from the next derivation). */
  sqlite3_stmt *set = NULL, *clr = NULL;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE sources SET category_canonical=?2, category_provenance='declared', "
        "category_confidence=1.0 WHERE id=?1 AND (category_canonical IS NOT ?2 "
        "OR category_provenance IS NOT 'declared')", -1, &set, NULL) != SQLITE_OK ||
      sqlite3_prepare_v2(db->h,
        "UPDATE sources SET category_canonical=NULL, category_provenance=NULL, "
        "category_confidence=NULL WHERE id=?1 AND category_provenance='declared'",
        -1, &clr, NULL) != SQLITE_OK) {
    sqlite3_finalize(set);
    return -1;                       /* the columns are not there (old DB) */
  }
  if (sqlite3_exec(db->h, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
    sqlite3_finalize(set); sqlite3_finalize(clr);
    return -1;
  }
  for (int i = 0; ok && i < n; i++) {
    if (!c[i].text) continue;
    int canon = c[i].kind != SC_KIND_POD && source_category_is_canonical(c[i].category);
    sqlite3_stmt *s = canon ? set : clr;
    sqlite3_reset(s);
    sqlite3_bind_text(s, 1, c[i].def->id, -1, SQLITE_STATIC);
    if (canon) sqlite3_bind_text(s, 2, c[i].category, -1, SQLITE_STATIC);
    if (sqlite3_step(s) != SQLITE_DONE) ok = 0;
    else changed += sqlite3_changes(db->h);
  }
  sqlite3_finalize(set); sqlite3_finalize(clr);
  sqlite3_exec(db->h, ok ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL);
  return ok ? changed : -1;
}

/* ---- derived ------------------------------------------------------------- */

typedef struct { float *v; int cat; } scat_label;

static void normalise(float *v, int dim) {
  double s = 0;
  for (int i = 0; i < dim; i++) s += (double)v[i] * v[i];
  if (s <= 0) return;
  float inv = (float)(1.0 / sqrt(s));
  for (int i = 0; i < dim; i++) v[i] *= inv;
}

int source_category_derive(db_handle *db) {
  if (!db || !db->h) return -1;
  char basis[160];
  if (!service_vec_ready_all(db, basis, sizeof basis)) return 0;
  meta_ensure(db);
  char *have = meta_get(db, "derived_basis");
  int same = have && !strcmp(have, basis);
  free(have);
  if (same) return 0;

  /* Pass 1: the labelled set — every non-pod source declaring one of the 31,
   * with its vector. */
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, "SELECT id, embedding FROM " SVEC_TABLE, -1, &s,
                         NULL) != SQLITE_OK) {
    fprintf(stderr, "[category] cannot read the service index: %s\n",
            sqlite3_errmsg(db->h));
    return -1;
  }
  scat_label *lab = NULL;
  int nl = 0, cl = 0, dim = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *id = (const char *)sqlite3_column_text(s, 0);
    const source_card *c = id ? source_card_get(id) : NULL;
    if (!c || c->kind == SC_KIND_POD) continue;
    int ci = cat_index(c->category);
    if (ci < 0) continue;
    int bytes = sqlite3_column_bytes(s, 1);
    if (bytes <= 0 || bytes % (int)sizeof(float)) continue;
    if (!dim) dim = bytes / (int)sizeof(float);
    if (bytes / (int)sizeof(float) != dim) continue;
    if (nl == cl) {
      int nc = cl ? cl * 2 : 1024;
      scat_label *nlab = realloc(lab, (size_t)nc * sizeof *nlab);
      if (!nlab) break;
      lab = nlab; cl = nc;
    }
    lab[nl].v = malloc((size_t)bytes);
    if (!lab[nl].v) break;
    memcpy(lab[nl].v, sqlite3_column_blob(s, 1), (size_t)bytes);
    normalise(lab[nl].v, dim);
    lab[nl].cat = ci;
    nl++;
  }
  sqlite3_finalize(s);
  if (nl < SCAT_K) {
    for (int i = 0; i < nl; i++) free(lab[i].v);
    free(lab);
    fprintf(stderr, "[category] only %d declared sources have vectors — too few "
                    "to vote; nothing derived\n", nl);
    return 0;
  }

  /* Pass 2: every other non-pod source with a vector gets the vote of its
   * SCAT_K nearest labelled neighbours, weighted by cosine. Written in one
   * transaction; the old derived labels are cleared first so a source that
   * lost its vector does not keep a label from a space that no longer
   * exists. */
  int derived = 0, ok = 1;
  sqlite3_stmt *up = NULL;
  if (sqlite3_exec(db->h, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) ok = 0;
  if (ok) ok = sqlite3_exec(db->h, "UPDATE sources SET category_canonical=NULL, "
                            "category_provenance=NULL, category_confidence=NULL "
                            "WHERE category_provenance='derived'", NULL, NULL,
                            NULL) == SQLITE_OK;
  if (ok) ok = sqlite3_prepare_v2(db->h,
                 "UPDATE sources SET category_canonical=?2, "
                 "category_provenance='derived', category_confidence=?3 "
                 "WHERE id=?1", -1, &up, NULL) == SQLITE_OK;
  if (ok) ok = sqlite3_prepare_v2(db->h, "SELECT id, embedding FROM " SVEC_TABLE,
                                  -1, &s, NULL) == SQLITE_OK;
  float *q = ok ? malloc((size_t)dim * sizeof *q) : NULL;
  if (!q) ok = 0;
  while (ok && sqlite3_step(s) == SQLITE_ROW) {
    const char *id = (const char *)sqlite3_column_text(s, 0);
    const source_card *c = id ? source_card_get(id) : NULL;
    if (!c || c->kind == SC_KIND_POD || cat_index(c->category) >= 0) continue;
    if (sqlite3_column_bytes(s, 1) != dim * (int)sizeof(float)) continue;
    memcpy(q, sqlite3_column_blob(s, 1), (size_t)dim * sizeof *q);
    normalise(q, dim);
    double best_sim[SCAT_K];
    int best_cat[SCAT_K], nb = 0;
    for (int j = 0; j < nl; j++) {
      const float *v = lab[j].v;
      double d = 0;
      for (int t = 0; t < dim; t++) d += (double)q[t] * v[t];
      if (nb == SCAT_K && d <= best_sim[nb - 1]) continue;
      int pos = nb < SCAT_K ? nb++ : SCAT_K - 1;
      while (pos > 0 && best_sim[pos - 1] < d) {
        best_sim[pos] = best_sim[pos - 1]; best_cat[pos] = best_cat[pos - 1]; pos--;
      }
      best_sim[pos] = d; best_cat[pos] = lab[j].cat;
    }
    double w[SOURCE_CATEGORY_COUNT] = {0}, tot = 0;
    for (int j = 0; j < nb; j++) {
      double x = best_sim[j] > 0 ? best_sim[j] : 0;
      w[best_cat[j]] += x; tot += x;
    }
    int win = -1;
    for (int ci = 0; ci < SOURCE_CATEGORY_COUNT; ci++)
      if (w[ci] > 0 && (win < 0 || w[ci] > w[win])) win = ci;
    if (win < 0 || tot <= 0) continue;        /* no positive neighbour: none */
    sqlite3_reset(up);
    sqlite3_bind_text(up, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 2, SOURCE_CATEGORIES[win], -1, SQLITE_STATIC);
    sqlite3_bind_double(up, 3, w[win] / tot);
    if (sqlite3_step(up) != SQLITE_DONE) { ok = 0; break; }
    if (sqlite3_changes(db->h)) derived++;
  }
  free(q);
  sqlite3_finalize(s);
  sqlite3_finalize(up);
  for (int i = 0; i < nl; i++) free(lab[i].v);
  free(lab);
  if (sqlite3_exec(db->h, ok ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL) != SQLITE_OK)
    ok = 0;
  if (!ok) {
    fprintf(stderr, "[category] derivation failed: %s\n", sqlite3_errmsg(db->h));
    return -1;
  }
  char at[40];
  time_t t = time(NULL);
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(at, sizeof at, "%Y-%m-%dT%H:%M:%SZ", &tm);
  meta_set(db, "derived_basis", basis);
  meta_set(db, "derived_at", at);
  fprintf(stderr, "[category] %d sources labelled by a %d-nearest vote over %d "
                  "declared sources\n", derived, SCAT_K, nl);
  return derived;
}

cJSON *source_category_distribution(db_handle *db) {
  cJSON *o = cJSON_CreateObject();
  cJSON *dec = cJSON_CreateObject(), *der = cJSON_CreateObject();
  long unl = 0;
  sqlite3_stmt *s = NULL;
  if (db && db->h && sqlite3_prepare_v2(db->h,
        "SELECT category_provenance, category_canonical, count(*) FROM sources "
        "GROUP BY 1, 2 ORDER BY 3 DESC", -1, &s, NULL) == SQLITE_OK) {
    while (sqlite3_step(s) == SQLITE_ROW) {
      const char *p = (const char *)sqlite3_column_text(s, 0);
      const char *c = (const char *)sqlite3_column_text(s, 1);
      long n = sqlite3_column_int64(s, 2);
      if (!p || !c) { unl += n; continue; }
      cJSON_AddNumberToObject(!strcmp(p, "declared") ? dec : der, c, (double)n);
    }
    sqlite3_finalize(s);
  }
  cJSON_AddItemToObject(o, "declared", dec);
  cJSON_AddItemToObject(o, "derived", der);
  cJSON_AddNumberToObject(o, "unlabelled", (double)unl);
  char *at = db ? (meta_ensure(db), meta_get(db, "derived_at")) : NULL;
  if (at) cJSON_AddStringToObject(o, "derived_at", at); else cJSON_AddNullToObject(o, "derived_at");
  free(at);
  cJSON_AddNumberToObject(o, "vote_k", SCAT_K);
  return o;
}
