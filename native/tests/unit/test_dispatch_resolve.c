/* test_dispatch_resolve.c — what core/osint_dispatch.c does AFTER a name-miss
 * has been resolved, and what it counts as a record.
 *
 * Three defects this holds closed:
 *
 *  1. The resolved source ran under the MISSPELLED id. osint_dispatch() set
 *     ctx.source_id = canon after resolving canon onto a registered source, and
 *     hp_run() looks its table row up by an exact match on ctx->source_id — so
 *     the resolved run failed every time, wrote fetch_log status=error against
 *     the healthy source it resolved to, and opened an anomaly on it.
 *
 *  2. The resolver considered every registered id, most of them scheduled bulk
 *     rows. A typo one edit from one of those would answer an entity question
 *     with a whole bulk collection attributed to the entity (rule 4d). Only
 *     entity pivots may be resolution candidates.
 *
 *  3. A `collector-*-notice` emit counted as data: a keyless pivot that emitted
 *     only its needs-credential notice came back success=1, records=1,
 *     confidence 70 and fetch_log ok with records_fetched=1.
 *
 * Includes osint_dispatch.c so it can reach the static is_entity_pivot(); the
 * harness links every object except that one (tests/unit/run.sh). No network:
 * the pivot used here is credential-gated and the credential is unset. */
#include "../../core/osint_dispatch.c"

#include <assert.h>
#include <ctype.h>

static int upper_id(const char *s) {
  if (!s || !*s) return 0;
  for (; *s; s++)
    if (!(isupper((unsigned char)*s) || isdigit((unsigned char)*s) || *s == '_'))
      return 0;
  return 1;
}

static long scalar(db_handle *db, const char *sql, const char *arg) {
  sqlite3_stmt *s; long v = -1;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return -2;
  if (arg) sqlite3_bind_text(s, 1, arg, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

int main(void) {
  assert(registry_count() > 0 && "no sources registered — link line is wrong");

  /* ── 2. a resolution only ever lands on an entity pivot ────────────────── */
  {
    const source_def **all = registry_all();
    int n = registry_count(), checked = 0, resolved = 0;
    for (int i = 0; i < n && checked < 150; i++) {
      const source_def *d = all[i];
      if (!d || is_entity_pivot(d) || !upper_id(d->id) || strlen(d->id) < 8)
        continue;
      checked++;
      char asked[160], out[128];
      /* the structural form ("<ID>_LOOKUP") and a one-character typo */
      snprintf(asked, sizeof asked, "%s_LOOKUP", d->id);
      if (osint_resolve_near(asked, out, sizeof out)) {
        resolved++;
        const source_def *hit = registry_get(out);
        assert(hit && is_entity_pivot(hit) &&
               "a near-miss resolved onto a scheduled bulk row");
      }
      snprintf(asked, sizeof asked, "%s", d->id);
      asked[strlen(asked) - 1] = asked[strlen(asked) - 1] == 'X' ? 'Y' : 'X';
      if (!registry_get(asked) && osint_resolve_near(asked, out, sizeof out)) {
        resolved++;
        const source_def *hit = registry_get(out);
        assert(hit && is_entity_pivot(hit) &&
               "a typo resolved onto a scheduled bulk row");
      }
    }
    assert(checked > 0 && "no scheduled upper-case ids to probe");
    printf("  %d scheduled ids probed, %d near-misses resolved, all onto pivots: ok\n",
           checked, resolved);
  }

  /* ── 4. an EXACT id runs on an entity only if it is a pivot ──────────────
   * osint_lookup() returned any registered source for an exact id, so a model
   * or a hand-written POST /api/search naming a scheduled bulk feed got the
   * whole feed attributed to the entity, and PORT_SCANNER (collector _probe)
   * was one request away from any JWT holder. */
  {
    const char *dbp4 = getenv("JO_DB");
    assert(dbp4 && *dbp4);
    db_handle db4 = {0};
    assert(db_open(&db4, NULL, NULL) == 0);
    intel_sink ps = intel_sink_make(&db4, "osint-search", "legacy");
    const source_def **all = registry_all();
    int n = registry_count(), refused = 0;
    for (int i = 0; i < n && refused < 5; i++) {
      const source_def *d = all[i];
      if (!d || !upper_id(d->id) || is_entity_pivot(d) || d->update_interval_sec <= 0)
        continue;
      if (strlen(d->id) >= 64) continue;          /* osint_canon's buffer */
      osint_result r4;
      osint_dispatch(&db4, NULL, d->id, "john smith", "person", &ps, &r4);
      assert(r4.error && !strcmp(r4.error, "not_a_pivot"));
      assert(r4.success == 0 && r4.records == 0 && !r4.resolved_from);
      osint_result_free(&r4);
      /* refused before it ran: nothing logged against the feed */
      assert(scalar(&db4, "SELECT count(*) FROM fetch_log WHERE source_id=?1",
                    d->id) == 0);
      refused++;
    }
    assert(refused == 5);
    if (registry_get("PORT_SCANNER")) {
      osint_result r4;
      osint_dispatch(&db4, NULL, "PORT_SCANNER", "127.0.0.1", "ip", &ps, &r4);
      assert(r4.error && !strcmp(r4.error, "not_a_pivot"));
      osint_result_free(&r4);
      /* the operator CLI path still reaches it (host_allowed refuses
       * loopback before any connect, so this touches no network) */
      osint_dispatch_operator(&db4, NULL, "PORT_SCANNER", "127.0.0.1", "ip",
                              &ps, &r4);
      assert(!r4.error || strcmp(r4.error, "not_a_pivot"));
      osint_result_free(&r4);
    }
    /* the pipeline's own fallback pivot is a pivot */
    const source_def *jc = registry_get("JP_CORPUS_LOOKUP");
    assert(jc && is_entity_pivot(jc));
    intel_sink_free(&ps);
    db_close(&db4);

    /* the follow-up round's schema names exactly the menu's pivots */
    const char *ids[3] = { "JP_CORPUS_LOOKUP", NULL, "NO_SUCH_SERVICE_X" };
    const char *sched = NULL;
    for (int i = 0; i < n && !sched; i++)
      if (all[i] && all[i]->id && !is_entity_pivot(all[i])) sched = all[i]->id;
    ids[1] = sched;
    char *sch = osint_phase2_schema_dynamic_ids(ids, 3);
    assert(sch);
    cJSON *sj = cJSON_Parse(sch);
    free(sch);
    cJSON *en = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(
        cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(sj,
        "properties"), "chain_services"), "items"), "properties"), "service"),
        "enum");
    assert(cJSON_GetArraySize(en) == 1 &&
           !strcmp(cJSON_GetArrayItem(en, 0)->valuestring, "JP_CORPUS_LOOKUP"));
    assert(cJSON_GetObjectItem(cJSON_GetObjectItem(sj, "properties"),
                               "needs_newphase"));
    cJSON_Delete(sj);
    const char *none[1] = { sched };
    assert(osint_phase2_schema_dynamic_ids(none, 1) == NULL);
    sch = osint_phase2_schema_dynamic_limited(7);
    sj = cJSON_Parse(sch);
    free(sch);
    en = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(
        cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(sj,
        "properties"), "chain_services"), "items"), "properties"), "service"),
        "enum");
    assert(cJSON_GetArraySize(en) == 7);
    cJSON_Delete(sj);
    printf("  exact non-pivot ids refused (not_a_pivot, nothing run), operator "
           "path kept, phase-2 enum = menu pivots: ok\n");
  }

  /* ── 1 + 3. a resolved, credential-gated pivot ─────────────────────────── */
  const char *GATED = "UK_CH_OFFICER_SEARCH";
  const source_def *g = registry_get(GATED);
  if (!g || !is_entity_pivot(g)) {
    printf("  (skipped the dispatch half: %s is not a registered pivot)\n", GATED);
    printf("test_dispatch_resolve: ok\n");
    return 0;
  }
  unsetenv("COMPANIES_HOUSE_API_KEY");

  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  intel_sink persist = intel_sink_make(&db, "osint-search", "legacy");

  char misspelled[96];
  snprintf(misspelled, sizeof misspelled, "%s_LOOKUP", GATED);
  osint_result r;
  osint_dispatch(&db, NULL, misspelled, "john smith", "person", &persist, &r);

  printf("  dispatch %s -> service=%s success=%d records=%d notices=%d error=%s\n",
         misspelled, r.service, r.success, r.records, r.notices,
         r.error ? r.error : "(null)");
  assert(r.resolved_from && !strcmp(r.resolved_from, misspelled));
  assert(!strcmp(r.service, GATED));
  /* 3: the notice is reported, never counted */
  assert(r.success == 0 && r.records == 0 && r.notices == 1 && r.confidence == 0);
  assert(r.error && !strcmp(r.error, "needs_credential"));
  assert(r.data && strstr(r.data, "\"notice_count\":1") &&
         strstr(r.data, "\"record_count\":0"));
  /* 1: the run happened under the REAL id — fetch_log says ok, not error, and
   * no anomaly was opened against the healthy source. */
  assert(scalar(&db, "SELECT count(*) FROM fetch_log WHERE source_id=?1 AND status='ok'"
                     " AND records_fetched=0", GATED) == 1);
  assert(scalar(&db, "SELECT count(*) FROM fetch_log WHERE source_id=?1 AND status<>'ok'",
                GATED) == 0);
  assert(scalar(&db, "SELECT count(*) FROM fetch_log WHERE source_id=?1",
                misspelled) == 0);
  assert(scalar(&db, "SELECT count(*) FROM collector_anomaly WHERE source_id=?1",
                GATED) == 0);
  /* the notice row itself is stored, under the real id */
  assert(scalar(&db, "SELECT count(*) FROM intel_items WHERE source_id=?1 AND"
                     " record_type='collector-status-notice'", GATED) == 1);
  printf("  resolved run stored under %s, fetch_log ok, no anomaly: ok\n", GATED);

  osint_result_free(&r);
  intel_sink_free(&persist);
  db_close(&db);
  printf("test_dispatch_resolve: ok\n");
  return 0;
}
