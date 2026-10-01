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
 * on appno+kpdate — across the whole walk, using the tree's growable seen_set rather than a
 * fixed ring (a ring that fills stops deduping and starts losing rows, silently).
 *
 * PAGED. This read `start=0&length=50` once, against a `resultcount` of 89,944: fifty newest
 * judgments, roughly 25 after the EN/FR dedupe, and page two of 1,798 was never asked for.
 * Both dials are the upstream's own and both were verified 2026-08-24: `length=500` and
 * `length=1000` answer in full, and `start=50` returns the next window with no overlap.
 * The walk now takes 500 at a time and advances `start`. The page ceiling below is a
 * deliberate, DISCLOSED bound — pulling all 89,944 judgments twice a day is load the Council
 * of Europe never agreed to — so a run that stops early emits a collector-truncation-notice
 * carrying the upstream's own resultcount, and $JO_HUDOC_PAGE_MAX lifts it for a backfill.
 */
#include "sanc_common.inc"
#include "lib/seenset.h"

#define HUDOC_QUERY \
  "https://hudoc.echr.coe.int/app/query/results?query=contentsitename%3DECHR%20AND%20" \
  "(documentcollectionid2%3D%22JUDGMENTS%22)&select=itemid,docname,appno,article," \
  "conclusion,kpdate&sort=kpdate%20Descending"
/* The walk's FIRST page, and the endpoint quoted in a truncation notice. run()
 * builds each page's own URL from HUDOC_QUERY and advances `start`. */
#define HUDOC_URL HUDOC_QUERY "&start=0&length=500"   /* exhaustive-ok: first page of a walk; run() advances start to the ceiling and discloses an early stop */

#define HUDOC_PAGE_SIZE 500
#define HUDOC_PAGE_MAX  20   /* exhaustive-ok: disclosed page ceiling; an early stop emits a collector-truncation-notice against resultcount, and $JO_HUDOC_PAGE_MAX raises it */

static int hudoc_page_max(void) {
  const char *e = getenv("JO_HUDOC_PAGE_MAX");
  if (e && *e) { int v = atoi(e); if (v > 0) return v; }
  return HUDOC_PAGE_MAX;
}

/* Emit every judgment on one page. `seen` spans the whole walk so the EN/FR
 * pair of a judgment is collapsed even when the two land on different pages. */
static int hudoc_emit_page(intel_sink *sink, const cJSON *doc, seen_set *seen) {
  const cJSON *results = cJSON_GetObjectItem(doc, "results");
  const cJSON *rc = cJSON_GetObjectItem(doc, "resultcount");
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
    if (!seen_add(seen, key)) continue;      /* growable — never stops deduping */

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
  const int page_max = hudoc_page_max();
  seen_set seen = {0};
  int n = 0, pages = 0, truncated = 0;
  long available = -1;

  for (; pages < page_max; pages++) {
    char url[640];
    snprintf(url, sizeof url, "%s&start=%d&length=%d",
             HUDOC_QUERY, pages * HUDOC_PAGE_SIZE, HUDOC_PAGE_SIZE);

    cJSON *doc = sanc_http_json(ctx, url, NULL, 45000, "echr-hudoc");
    if (!doc) {
      /* A dead FIRST page is a dead endpoint and stays an error (R3); a
       * failure mid-walk keeps the records already held and is disclosed. */
      if (pages == 0) { seen_free(&seen); return -1; }
      truncated = 1;
      break;
    }
    const cJSON *rc = cJSON_GetObjectItem(doc, "resultcount");
    if (available < 0 && cJSON_IsNumber(rc)) available = (long) rc->valuedouble;

    const cJSON *results = cJSON_GetObjectItem(doc, "results");
    int got = cJSON_IsArray(results) ? cJSON_GetArraySize(results) : 0;
    n += hudoc_emit_page(sink, doc, &seen);
    cJSON_Delete(doc);

    /* A short page is HUDOC saying it has run out; asking for another would be
     * us inventing a page it never offered. */
    if (got < HUDOC_PAGE_SIZE) { pages++; break; }
    if (pages + 1 >= page_max) truncated = 1;
  }
  seen_free(&seen);

  if (truncated)
    jo_trunc_notice(sink, "echr-hudoc-judgments", HUDOC_URL,
                    (long) pages * HUDOC_PAGE_SIZE, available,
                    "page ceiling reached: HUDOC holds far more judgments than "
                    "one scheduled run pulls (records_used counts result rows "
                    "read; emitted rows are fewer because the EN/FR pair of a "
                    "judgment collapses to one)",
                    "raise $JO_HUDOC_PAGE_MAX to backfill the archive");

  fprintf(stderr, "[echr-hudoc] emitted %d judgments across %d page(s)%s\n",
          n, pages, truncated ? " (TRUNCATED — notice emitted)" : "");
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
