/* test_list_paging.c — the evidence, saved-search, search-history and
 * maintenance lists page, and say how much exists.
 *
 * Every one of these routes used to answer a fixed slice (evidence 100,
 * saved searches / history `limit`, maintenance LIMIT 50/100/30/20/30) with
 * nothing in the response but the size of that slice, so "50 rows" and "the
 * first 50 of 900" were the same answer (house rule 2). Each now answers
 *   page:{limit, offset, count, total, has_more}
 * with `total` a COUNT(*) over the same predicate as the rows.
 *
 * Holds:
 *   - walking a list by offset visits every row exactly once, and `total`
 *     and `has_more` agree with what the walk saw;
 *   - ?mine=1 / kind= / pinned= narrow the total exactly as they narrow the
 *     rows, and another workspace's rows count for nothing;
 *   - the digest's verified buckets are no longer carved out of the newest 50
 *     verified repairs: 5 staged url_swaps older than 55 auto-dismissals used
 *     to be invisible, and are now listed with total 5;
 *   - the digest keeps serving the sizes it always served (so a client that
 *     reads only the arrays sees no change) and the next pages come from
 *     maintenance_list();
 *   - unknown list / unknown source answer 404. */
#include "core/savedsearchapi.h"
#include "core/maintenanceapi.h"
#include "core/evidence.h"
#include "core/tenantapi.h"
#include "core/db.h"
#include "third_party/cJSON.h"
#include "third_party/sqlite3.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tenant_ctx who(const char *tid, const char *uid, const char *role) {
  tenant_ctx t; memset(&t, 0, sizeof t);
  snprintf(t.tenant_id, sizeof t.tenant_id, "%s", tid);
  snprintf(t.user_id, sizeof t.user_id, "%s", uid);
  snprintf(t.role, sizeof t.role, "%s", role);
  return t;
}
static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}

/* Parse a response and pull out page{} fields. total -1 == null. */
typedef struct { int count, offset, limit, has_more; long total; } pg;
static cJSON *parse(char *body) {
  assert(body);
  cJSON *j = cJSON_Parse(body);
  if (!j) { fprintf(stderr, "unparseable: %s\n", body); exit(1); }
  free(body);
  return j;
}
static pg page_of(cJSON *page) {
  pg p = { -1, -1, -1, -1, -2 };
  assert(page && cJSON_IsObject(page));
  cJSON *x;
  if ((x = cJSON_GetObjectItem(page, "count"))  && cJSON_IsNumber(x)) p.count  = x->valueint;
  if ((x = cJSON_GetObjectItem(page, "offset")) && cJSON_IsNumber(x)) p.offset = x->valueint;
  if ((x = cJSON_GetObjectItem(page, "limit"))  && cJSON_IsNumber(x)) p.limit  = x->valueint;
  if ((x = cJSON_GetObjectItem(page, "has_more"))) p.has_more = cJSON_IsTrue(x);
  x = cJSON_GetObjectItem(page, "total");
  assert(x && "every paged list reports a total (null only if the count failed)");
  p.total = cJSON_IsNumber(x) ? (long)x->valuedouble : -1;
  return p;
}
static int arr_len(cJSON *j, const char *k) {
  cJSON *a = cJSON_GetObjectItem(j, k);
  assert(a && cJSON_IsArray(a));
  return cJSON_GetArraySize(a);
}

/* Walk a saved-search / history list by offset; return the number of
 * distinct ids seen and check total/has_more on every page. */
typedef char *(*list_fn)(db_handle *, const tenant_ctx *, const char *qs, int *st);
static db_handle *g;
static char *ss_list(db_handle *db, const tenant_ctx *t, const char *qs, int *st) {
  return savedsearchapi(db, t, "GET", "", "", qs, NULL, st);
}
static char *hist_list(db_handle *db, const tenant_ctx *t, const char *qs, int *st) {
  return searchhistoryapi(db, t, "GET", qs, NULL, st);
}
static int walk(list_fn fn, const tenant_ctx *t, const char *filter, int lim,
                long expect_total) {
  char seen[256][64]; int nseen = 0;
  for (int off = 0; ; off += lim) {
    char qs[160];
    snprintf(qs, sizeof qs, "limit=%d&offset=%d%s%s", lim, off,
             filter[0] ? "&" : "", filter);
    int st = 0;
    cJSON *j = parse(fn(g, t, qs, &st));
    assert(st == 200);
    pg p = page_of(cJSON_GetObjectItem(j, "page"));
    assert(p.total == expect_total && "total is the whole filtered set");
    assert(p.offset == off && p.limit == lim);
    cJSON *d = cJSON_GetObjectItem(j, "data"), *r;
    assert(p.count == cJSON_GetArraySize(d));
    cJSON_ArrayForEach(r, d) {
      cJSON *id = cJSON_GetObjectItem(r, "id");
      char key[64];
      if (cJSON_IsString(id)) snprintf(key, sizeof key, "%s", id->valuestring);
      else                    snprintf(key, sizeof key, "%d", id->valueint);
      for (int i = 0; i < nseen; i++)
        assert(strcmp(seen[i], key) != 0 && "no row is listed twice");
      assert(nseen < 256);
      snprintf(seen[nseen++], sizeof seen[0], "%s", key);
    }
    int more = p.has_more;
    assert(more == (off + p.count < expect_total) && "has_more agrees with total");
    cJSON_Delete(j);
    if (!more) break;
  }
  return nseen;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  g = &db;
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tA','ta','A'),('tB','tb','B')");
  exec_ok(&db, "INSERT INTO users(id,email) VALUES ('uA1','a1@x.test'),('uA2','a2@x.test'),('uB1','b1@x.test')");
  tenant_ctx a1 = who("tA", "uA1", "analyst"), a2 = who("tA", "uA2", "analyst"),
             b1 = who("tB", "uB1", "owner");

  /* ── saved searches: 7 by uA1 (2 pinned, 3 osint), 4 by uA2, 2 in tB ── */
  int st = 0;
  for (int i = 0; i < 13; i++) {
    const tenant_ctx *t = i < 7 ? &a1 : i < 11 ? &a2 : &b1;
    char body[200];
    snprintf(body, sizeof body,
             "{\"name\":\"s%02d\",\"kind\":\"%s\",\"params\":{\"q\":\"x%d\"},\"pinned\":%s}",
             i, (i < 3) ? "osint" : "intel", i, (i == 1 || i == 5) ? "true" : "false");
    char *r = savedsearchapi(&db, t, "POST", "", "", "", body, &st);
    assert(r && (st == 201 || st == 200)); free(r);
  }
  assert(walk(ss_list, &a2, "", 3, 11) == 11);
  assert(walk(ss_list, &a2, "", 200, 11) == 11);
  assert(walk(ss_list, &a1, "mine=1", 2, 7) == 7);
  assert(walk(ss_list, &a2, "mine=1", 3, 4) == 4);
  assert(walk(ss_list, &a2, "kind=osint", 2, 3) == 3);
  assert(walk(ss_list, &a2, "pinned=1", 1, 2) == 2);
  assert(walk(ss_list, &b1, "", 5, 2) == 2);
  { cJSON *j = parse(ss_list(&db, &a2, "mine=1&limit=2", &st));
    cJSON *sc = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "scope");
    assert(cJSON_IsString(sc) && !strcmp(sc->valuestring, "user"));
    pg p = page_of(cJSON_GetObjectItem(j, "page"));
    assert(p.count == 2 && p.total == 4 && p.has_more == 1);
    cJSON_Delete(j); }
  { cJSON *j = parse(ss_list(&db, &a2, "offset=50", &st));
    pg p = page_of(cJSON_GetObjectItem(j, "page"));
    assert(p.count == 0 && p.total == 11 && p.has_more == 0 &&
           "an offset past the end is an empty page with the real total");
    cJSON_Delete(j); }
  { cJSON *j = parse(ss_list(&db, &a2, "offset=-4&limit=0", &st));
    pg p = page_of(cJSON_GetObjectItem(j, "page"));
    assert(p.offset == 0 && p.limit == 50 && p.count == 11);
    cJSON_Delete(j); }
  printf("  saved searches: offset walk, totals under mine/kind/pinned, other workspace excluded: ok\n");

  /* ── search history: 9 by uA1 (4 osint), 6 by uA2, 3 in tB ── */
  for (int i = 0; i < 18; i++) {
    const char *tid = i < 15 ? "tA" : "tB";
    const char *uid = i < 9 ? "uA1" : i < 15 ? "uA2" : "uB1";
    char params[64]; snprintf(params, sizeof params, "{\"q\":\"h%d\"}", i);
    search_history_record(&db, tid, uid, (i < 4) ? "osint" : "intel", params, i);
  }
  assert(walk(hist_list, &a1, "", 4, 15) == 15);
  assert(walk(hist_list, &a1, "mine=1", 4, 9) == 9);
  assert(walk(hist_list, &a2, "mine=1", 5, 6) == 6);
  assert(walk(hist_list, &a2, "kind=osint", 3, 4) == 4);
  assert(walk(hist_list, &a1, "mine=1&kind=intel", 2, 5) == 5);
  assert(walk(hist_list, &b1, "", 2, 3) == 3);
  { cJSON *j = parse(hist_list(&db, &a1, "limit=1", &st));
    cJSON *rm = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "retained_max");
    assert(cJSON_IsNumber(rm) && rm->valueint == SS_HISTORY_KEEP);
    cJSON_Delete(j); }
  printf("  search history: offset walk, totals under mine/kind, other workspace excluded: ok\n");

  /* ── evidence: 7 custody rows for one item, 2 for another ── */
  for (int i = 0; i < 9; i++) {
    char sql[512];
    snprintf(sql, sizeof sql,
      "INSERT INTO evidence(id,item_uid,source_id,captured_at,content_sha256,"
      "blob_path,chain_seq) VALUES ('ev%d','%s','src','2026-10-0%d 00:00:00',"
      "'%064d','p',%d)", i, i < 7 ? "item:1" : "item:2", 1 + i % 5, i, i);
    exec_ok(&db, sql);
  }
  { char seen[16][16]; int n = 0;
    for (int off = 0; ; off += 3) {
      cJSON *j = parse(evidence_list_for_item(&db, "item:1", 3, off));
      pg p = page_of(cJSON_GetObjectItem(j, "page"));
      assert(p.total == 7 && p.offset == off && p.limit == 3);
      cJSON *d = cJSON_GetObjectItem(j, "data"), *r;
      cJSON_ArrayForEach(r, d) {
        const char *id = cJSON_GetObjectItem(r, "id")->valuestring;
        for (int i = 0; i < n; i++) assert(strcmp(seen[i], id) != 0);
        snprintf(seen[n++], sizeof seen[0], "%s", id);
      }
      cJSON *sc = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "present_scope");
      assert(cJSON_IsString(sc) && !strcmp(sc->valuestring, "page"));
      int more = p.has_more;
      assert(more == (off + p.count < 7));
      cJSON_Delete(j);
      if (!more) break;
    }
    assert(n == 7 && "every custody row is reachable"); }
  { cJSON *j = parse(evidence_list_for_item(&db, "item:2", 100, 0));
    pg p = page_of(cJSON_GetObjectItem(j, "page"));
    assert(p.count == 2 && p.total == 2 && p.has_more == 0);
    cJSON_Delete(j); }
  printf("  evidence: offset walk reaches every custody row, total and has_more measured: ok\n");

  /* ── maintenance ── */
  exec_ok(&db, "INSERT INTO sources(id,name,type,category) VALUES "
               "('S1','one','api','x'),('S2','two','api','x')");
  exec_ok(&db, "INSERT INTO collector_anomaly(source_id,verdict) VALUES ('S1','manual')");
  /* 5 staged url_swaps, then 55 NEWER auto-dismissals: the old digest took
   * the newest 50 verified rows and split them, so the 5 never appeared. */
  for (int i = 0; i < 60; i++) {
    char sql[400];
    snprintf(sql, sizeof sql,
      "INSERT INTO collector_repair(anomaly_id,source_id,status,action,patch,"
      "created_at) VALUES (1,'S1','verified','%s','{}',"
      "datetime('now','-%d minutes'))",
      i < 5 ? "url_swap" : "auto_dismiss", 120 - i);
    exec_ok(&db, sql);
  }
  for (int i = 0; i < 53; i++)
    exec_ok(&db, "INSERT INTO collector_repair(anomaly_id,source_id,status,created_at) "
                 "VALUES (1,'S2','needs_human',datetime('now','-1 minutes'))");
  for (int i = 0; i < 35; i++)
    exec_ok(&db, "INSERT INTO fetch_log(source_id,status) VALUES ('S1','ok')");

  cJSON *d = parse(maintenance_digest(&db, 24));
  cJSON *pages = cJSON_GetObjectItem(d, "pages");
  assert(pages);
  cJSON *aw = cJSON_GetObjectItem(d, "awaiting_review");
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(aw, "awaiting_apply")) == 5 &&
         "staged fixes older than the newest 50 verified rows are listed");
  pg p = page_of(cJSON_GetObjectItem(pages, "awaiting_apply"));
  assert(p.total == 5 && p.count == 5 && p.has_more == 0);
  p = page_of(cJSON_GetObjectItem(pages, "auto_dismissed"));
  assert(p.total == 55 && p.count == 50 && p.has_more == 1 && p.limit == 50);
  assert(arr_len(d, "auto_dismissed") == 50 && "the digest keeps its old size");
  p = page_of(cJSON_GetObjectItem(pages, "needs_human"));
  assert(p.total == 53 && p.count == 50 && p.has_more == 1);
  p = page_of(cJSON_GetObjectItem(pages, "quarantined"));
  assert(p.total == 0 && p.count == 0 && p.limit == -1 && "quarantined is served whole");
  p = page_of(cJSON_GetObjectItem(pages, "worst_sources"));
  assert(p.total == 1 && "worst_sources counts sources, not repairs");
  cJSON_Delete(d);

  { cJSON *j = parse(maintenance_list(&db, "auto_dismissed", NULL, 24, 50, 50, &st));
    assert(st == 200);
    pg q = page_of(cJSON_GetObjectItem(j, "page"));
    assert(q.count == 5 && q.total == 55 && q.has_more == 0 && q.offset == 50);
    assert(arr_len(j, "data") == 5);
    cJSON *wh = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "window_hours");
    assert(cJSON_IsNumber(wh) && wh->valueint == 24);
    cJSON_Delete(j); }
  { cJSON *j = parse(maintenance_list(&db, "needs_human", NULL, 24, 500, 0, &st));
    pg q = page_of(cJSON_GetObjectItem(j, "page"));
    assert(q.limit == 200 && q.count == 53 && q.has_more == 0 && "limit clamps to 200");
    cJSON_Delete(j); }

  cJSON *sd = parse(maintenance_source_detail(&db, "S1"));
  p = page_of(cJSON_GetObjectItem(cJSON_GetObjectItem(sd, "pages"), "fetch_log"));
  assert(p.total == 35 && p.count == 30 && p.has_more == 1);
  assert(arr_len(sd, "fetch_log") == 30);
  p = page_of(cJSON_GetObjectItem(cJSON_GetObjectItem(sd, "pages"), "repairs"));
  assert(p.total == 60 && p.count == 30 && p.has_more == 1);
  cJSON_Delete(sd);
  { cJSON *j = parse(maintenance_list(&db, "fetch_log", "S1", 0, 0, 30, &st));
    assert(st == 200);
    pg q = page_of(cJSON_GetObjectItem(j, "page"));
    assert(q.count == 5 && q.total == 35 && q.has_more == 0 && q.limit == 30);
    cJSON_Delete(j); }

  char *r = maintenance_list(&db, "no_such_list", NULL, 24, 0, 0, &st);
  assert(r && st == 404); free(r);
  r = maintenance_list(&db, "fetch_log", "NOPE", 0, 0, 0, &st);
  assert(r && st == 404); free(r);
  r = maintenance_list(&db, "needs_human", "S1", 0, 0, 0, &st);
  assert(r && st == 404 && "a digest list is not a per-source list"); free(r);
  printf("  maintenance: digest pages, verified buckets in full, next pages, 404s: ok\n");

  db_close(&db);
  printf("\nall passed\n");
  return 0;
}
