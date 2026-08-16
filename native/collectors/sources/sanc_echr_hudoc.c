/* European Court of Human Rights — HUDOC judgment query API.
 *
 * Endpoint : https://hudoc.echr.coe.int/app/query/results
 *              ?query=contentsitename=ECHR AND (documentcollectionid2="JUDGMENTS")
 *              &select=itemid,docname,appno,article,conclusion,kpdate
 *              &sort=kpdate Descending&start=0&length=50      (%-encoded as verified)
 * Format   : JSON, {"resultcount", "results":[{"columns":{…}}]}.
 * Verified : HTTP 200, resultcount 89,604 judgments; newest
 *            001-251251 "CASE OF KOLESNYK AND SMELNYTSKYY v. UKRAINE", appno 24465/23;25217/23,
 *            articles 35;35-3-a;5;5-3, "No violation of Article 5 …", 2026-07-23.
 * Keyless  : yes.
 * Emits    : case name, application number(s), Convention articles engaged, the Court's own
 *            conclusion string, judgment date, respondent state (parsed from the tail of the
 *            case name) and the HUDOC permalink.
 * Geometry : NONE (R2). The respondent state is a party to a case, not a location.
 * Licence  : the Council of Europe permits reuse of HUDOC content with attribution; the
 *            Court's case-law database is free of charge.
 *
 * TWO TRAPS this endpoint sets, both handled here:
 *   1. every result nests its fields under a `columns` object — a parser that looks for
 *      top-level keys finds a well-formed 200 response and zero usable records;
 *   2. WITHOUT the documentcollectionid2="JUDGMENTS" clause the same query also returns press
 *      releases (itemid prefix 003-) with null appno/article/conclusion. The clause is part of
 *      the verified URL and must not be trimmed.
 * Judgments are published in EN and FR as SEPARATE items sharing an appno, so rows are deduped
 * on appno+kpdate within the page.
 */
#include "sanc_common.inc"
#include "../../lib/pagewalk.h"

/* start=0 is the FIRST page, not the only one: run() hands this url to
 * pw_walk(), which advances start=50,100,… to the JO_PAGE_MAX ceiling and
 * discloses the remainder against HUDOC's own resultcount.
 * exhaustive-ok: first page of a pw_walk, not a single fetch */
#define HUDOC_URL \
  "https://hudoc.echr.coe.int/app/query/results?query=contentsitename%3DECHR%20AND%20" \
  "(documentcollectionid2%3D%22JUDGMENTS%22)&select=itemid,docname,appno,article," \
  "conclusion,kpdate&sort=kpdate%20Descending&start=0&length=50"  /* exhaustive-ok: first page of a pw_walk, not a single fetch */

/* Dedupe state for the WHOLE walk, not one page.
 *
 * HUDOC publishes each judgment twice, EN and FR, sharing an appno — so rows
 * are deduped on appno+kpdate. That used to be a fixed 128-entry array scoped
 * to a single 50-row page, which was sufficient only because exactly one page
 * was ever fetched. Now that pagewalk continues over start=50,100,…, two things
 * break unless the state spans the walk: 128 slots cannot hold ~1,000 rows, and
 * an EN/FR pair split across a page boundary would slip through as two rows.
 * Growable, and freed once at the end of run(). */
typedef struct { char **k; int n, cap; } hudoc_seen;

static int hudoc_seen_add(hudoc_seen *s, const char *key) {
  for (int i = 0; i < s->n; i++)
    if (strcmp(s->k[i], key) == 0) return 0;          /* already emitted */
  if (s->n >= s->cap) {
    int nc = s->cap ? s->cap * 2 : 128;
    char **p = realloc(s->k, (size_t)nc * sizeof *p);
    if (!p) return 1;            /* OOM: emit rather than silently drop a row */
    s->k = p; s->cap = nc;
  }
  char *d = strdup(key);
  if (d) s->k[s->n++] = d;
  return 1;
}

/* pagewalk fetch shim: keeps the collector's 45 s timeout and header set. */
static cJSON *hudoc_fetch(const source_ctx *c, const char *url, void *ud) {
  (void)ud;
  return sanc_http_json(c, url, NULL, 45000, "echr-hudoc");
}

static int hudoc_emit_page(const source_ctx *ctx, intel_sink *sink,
                           const char *sid, cJSON *doc, void *ud, int *seen_out) {
  (void)ctx; (void)sid;
  hudoc_seen *seen_st = (hudoc_seen *)ud;

  cJSON *results = cJSON_GetObjectItem(doc, "results");
  const cJSON *rc = cJSON_GetObjectItem(doc, "resultcount");

  /* Records the page CONTAINED — pagewalk decides "did this come back full"
   * from this, not from the emitted count. A page of 50 holding 25 EN/FR pairs
   * emits 25 and is still full; reporting 25 would stop the walk early and
   * suppress the truncation notice. See lib/pagewalk.h. */
  if (seen_out) *seen_out = cJSON_IsArray(results) ? cJSON_GetArraySize(results) : 0;

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, results) {
    const cJSON *col = cJSON_GetObjectItem(r, "columns");
    if (!col) continue;                    /* trap 1: fields live under columns */
    const char *itemid = jo_sv(col, "itemid");
    const char *docname = jo_sv(col, "docname");
    if (!itemid || !docname) continue;

    const char *appno = jo_sv(col, "appno");
    const char *article = jo_sv(col, "article");
    const char *conclusion = jo_sv(col, "conclusion");
    const char *kpdate = jo_sv(col, "kpdate");

    char date[16] = {0};
    if (kpdate && strlen(kpdate) >= 10 && kpdate[4] == '-' && kpdate[7] == '-') {
      memcpy(date, kpdate, 10);
      date[10] = 0;
    }

    /* dedupe the EN/FR pair of the same judgment */
    char key[128];
    snprintf(key, sizeof key, "%s|%s", appno ? appno : itemid, date);
    if (!hudoc_seen_add(seen_st, key)) continue;

    /* respondent state is the tail of the Court's own case name */
    const char *state = NULL;
    const char *vs = strstr(docname, " v. ");
    if (vs) {
      const char *last = vs;
      const char *p = vs;
      while ((p = strstr(p + 4, " v. ")) != NULL) last = p;
      state = last + 4;
    }

    char link[160];
    snprintf(link, sizeof link, "https://hudoc.echr.coe.int/eng?i=%s", itemid);

    char summary[512];
    sanc_trunc(summary, sizeof summary, conclusion ? conclusion : "");
    sanc_collapse_ws(summary);

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "case", docname);
    sanc_add(body, "application_numbers", appno);
    sanc_add(body, "articles", article);
    sanc_add(body, "conclusion", conclusion);
    sanc_add(body, "judgment_date", date[0] ? date : NULL);
    sanc_add(body, "respondent_state", state);
    sanc_add(body, "itemid", itemid);
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "itemid", itemid);
    sanc_add(props, "application_numbers", appno);
    sanc_add(props, "articles", article);
    sanc_add(props, "respondent_state", state);
    sanc_add(props, "judgment_date", date[0] ? date : NULL);
    if (cJSON_IsNumber(rc))
      cJSON_AddNumberToObject(props, "upstream_judgment_count", rc->valuedouble);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = itemid;
    it.title = docname;
    it.summary = summary[0] ? summary : NULL;
    it.body = bj;
    it.link = link;
    it.lang = "en";
    it.published_at = date[0] ? date : NULL;
    it.record_type = "echr-judgment";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"court\",\"judgment\",\"human-rights\",\"echr\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj);
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* pw_walk advances the `start=` offset the query already carries, bounded by
   * JO_PAGE_MAX, and reports what it could not reach as a truncation notice —
   * including HUDOC's own `resultcount` (~90k) as records_available, which
   * pw_total_available now reads. This replaces a hand-written notice that
   * disclosed the same gap but never tried to close it: the reporting half of
   * house rule 2 was satisfied, the collection half was not. */
  hudoc_seen seen = {0};
  int n = pw_walk(ctx, sink, "echr-hudoc", HUDOC_URL,
                  hudoc_fetch, hudoc_emit_page, &seen);
  for (int i = 0; i < seen.n; i++) free(seen.k[i]);
  free(seen.k);
  if (n < 0) return -1;                    /* dead endpoint is an error */
  return 0;
}

static const source_def sanc_echr_hudoc_def = {
  .id = "echr-hudoc-judgments", .collector = "sanctions",
  .name = "ECHR HUDOC judgments",
  .update_interval_sec = 43200, .run = run,
  .category = "government", .type = "api",
  .url = "https://hudoc.echr.coe.int/app/query/results",
  .description = "European Court of Human Rights case law with the respondent state, application number, articles engaged and the holding — the largest human-rights jurisprudence set, machine-readable.",
  .license = "Council of Europe permits reuse of HUDOC content with attribution; the case-law database is free of charge.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_echr_hudoc_def)
