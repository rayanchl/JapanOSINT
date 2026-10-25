/* tests/unit/test_sourceapi.c — the source pages' API (core/sourceapi.c,
 * core/sourceapi_summary.c) against a scratch DB and the real registry.
 *
 * What is pinned:
 *   - detail: counts, record-type histogram and field inventory are what the
 *     rows in the DB say; the engine block names the engine; an hpengine row
 *     carries its declared request/parsing/paging; an unknown id is NULL.
 *   - search: a substring on the id finds the row; page/total are measured;
 *     facets name the collector; an impossible filter is an honest empty page.
 *   - collectors: every registered source is in exactly one collector, the
 *     per-collector tallies sum to the registry, and a collector's detail
 *     pages its own sources only.
 *   - summary: with no model listening, the route returns the llm status code
 *     AND the facts — never a fabricated summary (house rule 1 for prose).
 *
 * JO_DB is set by tests/unit/run.sh to a scratch file. */
#include "../../core/sourceapi.h"
#include "../../core/db.h"
#include "../../source.h"
#include "../../lib/hpengine.h"
#include "../../third_party/cJSON.h"
#include "../../third_party/sqlite3.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void insert_item(db_handle *db, const char *src, const char *uid, const char *title,
                        const char *rt, const char *props, const char *fetched) {
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(db->h,
    "INSERT OR REPLACE INTO intel_items(uid,source_id,title,fetched_at,record_type,properties,lat,lon) "
    "VALUES(?1,?2,?3,?4,?5,?6,?7,?8)", -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, src, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 3, title, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 4, fetched, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 5, rt, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 6, props, -1, SQLITE_STATIC);
  sqlite3_bind_double(s, 7, 35.0); sqlite3_bind_double(s, 8, 139.0);
  assert(sqlite3_step(s) == SQLITE_DONE);
  sqlite3_finalize(s);
}
static const cJSON *get(const cJSON *o, const char *path) {
  char buf[256]; snprintf(buf, sizeof buf, "%s", path);
  const cJSON *cur = o;
  for (char *tok = strtok(buf, "."); tok && cur; tok = strtok(NULL, "."))
    cur = cJSON_GetObjectItem(cur, tok);
  return cur;
}
static double num(const cJSON *o, const char *path) {
  const cJSON *v = get(o, path);
  assert(v && cJSON_IsNumber(v));
  return v->valuedouble;
}
static const char *str(const cJSON *o, const char *path) {
  const cJSON *v = get(o, path);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

/* A registered hpengine row to hang rows on — any one; the first with a {q}. */
static const source_def *pick_hp_pivot(void) {
  const source_def **a = registry_all();
  for (int i = 0; i < registry_count(); i++)
    if (a[i]->engine && !strcmp(a[i]->engine, "hpengine") && hp_source_uses_entity(a[i]->id))
      return a[i];
  return NULL;
}

int main(void) {
  db_handle db;
  assert(db_open(&db, NULL, NULL) == 0);
  const source_def *d = pick_hp_pivot();
  assert(d && "no hpengine entity pivot registered?");
  printf("using %s (collector=%s)\n", d->id, d->collector);

  /* 1. detail reflects the rows the DB holds */
  insert_item(&db, d->id, "t|1", "Alpha", "t-company", "{\"name\":\"Alpha\",\"orgno\":\"1\",\"city\":\"Oslo\"}", "2026-10-01T00:00:00Z");
  insert_item(&db, d->id, "t|2", "Beta",  "t-company", "{\"name\":\"Beta\",\"orgno\":\"2\"}", "2026-10-02T00:00:00Z");
  insert_item(&db, d->id, "t|3", "Gamma", "t-person",  "{\"name\":\"Gamma\",\"role\":\"ceo\"}", "2026-10-03T00:00:00Z");
  insert_item(&db, d->id, "t|n", "stored nothing: upstream answered 503", "collector-fetch-failure",
              "{\"http_status\":503}", "2026-10-04T00:00:00Z");
  setenv("JO_SOURCEAPI_SNAP_SEC", "0", 1);        /* no stale snapshot inside the test */

  char *js = sourceapi_detail(&db, d->id);
  assert(js);
  cJSON *o = cJSON_Parse(js); free(js);
  assert(o);
  assert(!strcmp(str(o, "registry.id"), d->id));
  assert(!strcmp(str(o, "registry.engine"), "hpengine"));
  assert(!strcmp(str(o, "registry.collector"), d->collector));
  assert(!strcmp(str(o, "engine.engine"), "hpengine"));
  assert(str(o, "engine.mode") && "engine mode named");
  assert(str(o, "engine.request.url") && "request url template present");
  assert(str(o, "engine.paging.strategy") && "paging strategy named");
  assert(!strcmp(str(o, "engine.kind"), "pivot") && !strcmp(str(o, "registry.kind"), "pivot"));
  assert(num(o, "data.item_count") == 4 && "4 rows incl. the notice");
  assert(num(o, "data.notices") == 1);
  assert(num(o, "data.geocoded") == 4);
  assert(num(o, "data.record_types.total") == 3);
  {
    const cJSON *vals = get(o, "data.record_types.values");
    assert(vals && cJSON_GetArraySize(vals) == 3);
    const cJSON *first = cJSON_GetArrayItem(vals, 0);
    assert(!strcmp(str(first, "record_type"), "t-company") && num(first, "count") == 2);
  }
  assert(num(o, "data.field_inventory.sample_size") == 4);
  {
    const cJSON *vals = get(o, "data.field_inventory.fields.values");
    const cJSON *first = cJSON_GetArrayItem(vals, 0);
    assert(!strcmp(str(first, "key"), "name") && num(first, "count") == 3 && "name is in 3 of 4 rows");
  }
  assert(cJSON_IsArray(get(o, "notices.counts")));
  {
    const cJSON *latest = get(o, "notices.latest");
    assert(latest && cJSON_GetArraySize(latest) == 1);
    assert(!strcmp(str(cJSON_GetArrayItem(latest, 0), "kind"), "collector-fetch-failure"));
  }
  assert(get(o, "schedule.kind") && get(o, "credentials") && get(o, "provenance") && get(o, "runs"));
  assert(get(o, "status.status") && "status key present (null before any run)");
  assert(cJSON_IsNull(get(o, "schedule.next_due")) && "never ran: next_due is null, not a guess");
  assert(get(o, "data.db_text_bytes") && cJSON_IsNumber(get(o, "data.db_text_bytes")));
  cJSON_Delete(o);
  assert(sourceapi_detail(&db, "NO_SUCH_SOURCE_ID") == NULL);
  printf("  detail ok\n");

  /* 2. search */
  sourceapi_query q = { .q = d->id, .limit = 10 };
  js = sourceapi_search(&db, &q);
  o = cJSON_Parse(js); free(js);
  assert(o);
  assert(num(o, "page.total") >= 1 && num(o, "page.count") >= 1 && num(o, "page.limit") == 10);
  {
    const cJSON *row0 = cJSON_GetArrayItem(get(o, "data"), 0);
    /* the exact id sorts first by name only by luck; find it */
    const cJSON *row; int found = 0;
    cJSON_ArrayForEach(row, get(o, "data")) if (!strcmp(str(row, "id"), d->id)) { found = 1; row0 = row; }
    assert(found);
    assert(num(row0, "item_count") == 4);
    assert(!strcmp(str(row0, "engine"), "hpengine") && !strcmp(str(row0, "kind"), "pivot"));
    assert(get(row0, "sched") && get(row0, "trust"));
  }
  assert(get(o, "meta.facets.collectors.values") && get(o, "meta.facets.statuses.values"));
  assert(num(o, "meta.registry_total") == registry_count());
  cJSON_Delete(o);
  sourceapi_query qz = { .q = "zz-no-such-source-zz", .limit = 5 };
  js = sourceapi_search(&db, &qz); o = cJSON_Parse(js); free(js);
  assert(num(o, "page.total") == 0 && num(o, "page.count") == 0 && cJSON_GetArraySize(get(o, "data")) == 0);
  cJSON_Delete(o);
  /* paging never repeats: page 1 and page 2 of a stable sort are disjoint */
  sourceapi_query p1 = { .sort = "id", .limit = 3, .offset = 0 }, p2 = { .sort = "id", .limit = 3, .offset = 3 };
  char *a1 = sourceapi_search(&db, &p1), *a2 = sourceapi_search(&db, &p2);
  cJSON *o1 = cJSON_Parse(a1), *o2 = cJSON_Parse(a2); free(a1); free(a2);
  assert(num(o1, "page.total") == registry_count() && cJSON_IsTrue(get(o1, "page.has_more")));
  {
    const char *last1 = str(cJSON_GetArrayItem(get(o1, "data"), 2), "id");
    const char *first2 = str(cJSON_GetArrayItem(get(o2, "data"), 0), "id");
    assert(strcmp(last1, first2) < 0);
  }
  cJSON_Delete(o1); cJSON_Delete(o2);
  printf("  search ok\n");

  /* 3. collectors partition the registry */
  js = sourceapi_collectors(&db); o = cJSON_Parse(js); free(js);
  assert(o);
  long sum = 0; const cJSON *c; int mine = 0;
  cJSON_ArrayForEach(c, get(o, "data")) {
    sum += (long)num(c, "sources");
    assert(get(c, "status.online") && get(c, "categories.values") && get(c, "engines.values"));
    if (!strcmp(str(c, "key"), d->collector)) { mine = 1; assert(num(c, "items") >= 4); }
  }
  assert(sum == registry_count() && "every source is in exactly one collector");
  assert(mine);
  assert(num(o, "meta.total") == cJSON_GetArraySize(get(o, "data")));
  /* 3b. groups: every collector has a parent group in collectors.def (zero
   * unassigned), and the groups' source counts sum to the registry too */
  assert(num(o, "meta.unassigned_collectors") == 0 && "a registered collector is missing from core/collectors.def");
  {
    long gsum = 0; const cJSON *g; int ng = 0;
    cJSON_ArrayForEach(g, get(o, "groups")) {
      ng++; gsum += (long)num(g, "sources");
      assert(cJSON_IsTrue(get(g, "is_group")) && get(g, "collectors") && cJSON_GetArraySize(get(g, "collectors")) > 0);
      assert(str(g, "label") && "a group has a label");
    }
    assert(gsum == registry_count() && "groups partition the registry");
    assert(ng >= 10 && "the grouping is in several natural-language headings");
  }
  cJSON_Delete(o);
  sourceapi_query gq = { .limit = 5 };
  js = sourceapi_collector(&db, "grp_government", &gq); assert(js && "a group key answers like a collector");
  o = cJSON_Parse(js); free(js);
  assert(cJSON_IsTrue(get(o, "is_group")) && cJSON_GetArraySize(get(o, "children")) > 5);
  assert(num(o, "sources.page.total") == num(o, "collector.sources") && "a group page lists its members' sources");
  {
    const cJSON *row; cJSON_ArrayForEach(row, get(o, "sources.data"))
      assert(!strcmp(str(row, "group"), "grp_government"));
  }
  cJSON_Delete(o);
  sourceapi_query cq = { .limit = 5 };
  js = sourceapi_collector(&db, d->collector, &cq); assert(js);
  o = cJSON_Parse(js); free(js);
  assert(!strcmp(str(o, "collector.key"), d->collector));
  {
    const cJSON *row; cJSON_ArrayForEach(row, get(o, "sources.data"))
      assert(!strcmp(str(row, "collector"), d->collector) && "a collector page holds its own sources only");
    assert(num(o, "sources.page.total") == num(o, "collector.sources"));
  }
  assert(get(o, "groups.values"));
  cJSON_Delete(o);
  assert(sourceapi_collector(&db, "no-such-collector-key", &cq) == NULL);
  printf("  collectors ok\n");

  /* 4. summary without a model: the status code and the facts, no prose */
  setenv("LLM_SUGGEST_BASE_URL", "http://127.0.0.1:9", 1);   /* discard port: refused */
  setenv("LLM_BASE_URL", "http://127.0.0.1:9", 1);
  js = sourceapi_summary(&db, d->id, 1); assert(js);
  o = cJSON_Parse(js); free(js);
  assert(str(o, "error") && !strcmp(str(o, "error"), "llm_unreachable"));
  assert(get(o, "facts.registry.id") && !strcmp(str(o, "facts.registry.id"), d->id));
  assert(num(o, "facts.data.item_count") == 4);
  assert(cJSON_GetArraySize(get(o, "facts.data.sample_titles")) == 3 && "notices are not sample titles");
  assert(!get(o, "summary") && "no summary is fabricated when the model is unreachable");
  cJSON_Delete(o);
  {
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(db.h, "SELECT COUNT(*) FROM source_summary", -1, &s, NULL) == SQLITE_OK);
    assert(sqlite3_step(s) == SQLITE_ROW && sqlite3_column_int(s, 0) == 0 && "a failure is never cached as a summary");
    sqlite3_finalize(s);
  }
  assert(sourceapi_summary(&db, "NO_SUCH_SOURCE_ID", 0) == NULL);
  printf("  summary degradation ok\n");

  /* ── health: the reason a source is not working, in the row ────────────
   *
   * The engine already records WHY a fetch was refused and what the remedy is;
   * before this, a reader had to open one source's detail page and read a
   * notice's properties blob to find it, which for 58,000 sources is the same
   * as not disclosing it. These assertions pin the projection, the precedence
   * between the two record kinds, and the one filter a reader actually wants.
   *
   * Nothing here removes a source: a failing row keeps its place and gains a
   * legible reason, which is the operator's decision this implements. */
  {
    const char *sid = d->id;
    /* a refusal, then a credential notice fetched LATER: the newer one wins,
     * because that is the state the source is in now */
    insert_item(&db, sid, "hf1", "refused", "collector-fetch-failure",
      "{\"http_status\":403,\"body_parsed\":false,\"outcome\":\"bot_wall\","
      "\"note\":\"the host refused this client\","
      "\"remedy\":\"bisect the agent, then core/httpclient.c UA_OVERRIDE\"}",
      "2026-10-22T09:00:00Z");
    char *j = sourceapi_detail(&db, sid);
    cJSON *o = cJSON_Parse(j); free(j);
    assert(get(o, "status.health") && "the detail carries health");
    assert(!strcmp(str(o, "status.health.state"), "failing"));
    assert(num(o, "status.health.http_status") == 403);
    assert(str(o, "status.health.remedy") &&
           strstr(str(o, "status.health.remedy"), "UA_OVERRIDE") &&
           "the engine's own remedy reaches the reader");
    assert(cJSON_IsFalse(get(o, "status.health.working")));
    cJSON_Delete(o);

    insert_item(&db, sid, "hf2", "gated", "collector-status-notice",
      "{\"status\":\"needs_credential\",\"env_vars_required\":[\"JO_A_KEY\",\"JO_B_KEY\"]}",
      "2026-10-22T10:00:00Z");
    j = sourceapi_detail(&db, sid);
    o = cJSON_Parse(j); free(j);
    assert(!strcmp(str(o, "status.health.state"), "gated") &&
           "the NEWER record decides the state");
    assert(cJSON_GetArraySize(get(o, "status.health.credentials_required")) == 2 &&
           "a gated source names the vars it needs, as a list");
    cJSON_Delete(o);

    /* the search row carries the SAME object, from the same builder */
    sourceapi_query hq = { .q = sid, .limit = 5 };
    j = sourceapi_search(&db, &hq);
    o = cJSON_Parse(j); free(j);
    const cJSON *row = cJSON_GetArrayItem(get(o, "data"), 0);
    assert(row && !strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem((cJSON *)row, "health"),
                                              "state")->valuestring, "gated") &&
           "the list shows the same reason the detail confirms");
    assert(get(o, "meta.facets.healths") && "the states are a facet");
    cJSON_Delete(o);

    /* "broken" is every state that is not ok — asked as one query, so a state
     * added later cannot be missed by a reader adding four counts up */
    sourceapi_query bq = { .health = "broken", .limit = 500 };
    j = sourceapi_search(&db, &bq);
    o = cJSON_Parse(j); free(j);
    int broken = (int)num(o, "page.total");
    assert(broken >= 1 && "the gated source is in the broken set");
    cJSON_Delete(o);

    sourceapi_query okq = { .health = "ok", .limit = 500 };
    j = sourceapi_search(&db, &okq);
    o = cJSON_Parse(j); free(j);
    const cJSON *okrows = get(o, "data");
    for (int i = 0; i < cJSON_GetArraySize(okrows); i++) {
      const cJSON *h = cJSON_GetObjectItem(cJSON_GetArrayItem((cJSON *)okrows, i), "health");
      assert(h && !strcmp(cJSON_GetObjectItem((cJSON *)h, "state")->valuestring, "ok") &&
             "health=ok returns only working sources");
    }
    cJSON_Delete(o);

    /* a truncation notice is a WORKING source telling us something: folding it
     * in here would report healthy rows as failing */
    insert_item(&db, sid, "ht1", "truncated", "collector-truncation-notice",
                "{\"records_available\":9000}", "2026-10-22T11:00:00Z");
    j = sourceapi_detail(&db, sid);
    o = cJSON_Parse(j); free(j);
    assert(!strcmp(str(o, "status.health.state"), "gated") &&
           "a truncation notice does not change the health state");
    cJSON_Delete(o);
    printf("  health projection ok\n");
  }

  db_close(&db);
  printf("test_sourceapi: all passed\n");
  return 0;
}
