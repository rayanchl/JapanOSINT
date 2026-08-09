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

#define HUDOC_URL \
  "https://hudoc.echr.coe.int/app/query/results?query=contentsitename%3DECHR%20AND%20" \
  "(documentcollectionid2%3D%22JUDGMENTS%22)&select=itemid,docname,appno,article," \
  "conclusion,kpdate&sort=kpdate%20Descending&start=0&length=50"

#define HUDOC_MAX_SEEN 128

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = sanc_http_json(ctx, HUDOC_URL, NULL, 45000, "echr-hudoc");
  if (!doc) return -1;

  cJSON *results = cJSON_GetObjectItem(doc, "results");
  const cJSON *rc = cJSON_GetObjectItem(doc, "resultcount");

  char *seen[HUDOC_MAX_SEEN];
  int nseen = 0, n = 0;
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
    int dup = 0;
    for (int i = 0; i < nseen; i++)
      if (strcmp(seen[i], key) == 0) { dup = 1; break; }
    if (dup) continue;
    if (nseen < HUDOC_MAX_SEEN) {
      char *k = strdup(key);
      if (k) seen[nseen++] = k;
    }

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
  for (int i = 0; i < nseen; i++) free(seen[i]);
  cJSON_Delete(doc);
  fprintf(stderr, "[echr-hudoc] emitted %d\n", n);
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
