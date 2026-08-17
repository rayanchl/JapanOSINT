/* European Court of Human Rights — HUDOC judgment query API.
 *
 * Endpoint : https://hudoc.echr.coe.int/app/query/results
 *              ?query=contentsitename=ECHR AND (documentcollectionid2="JUDGMENTS")
 *              &select=itemid,docname,appno,article,conclusion,kpdate
 *              &sort=kpdate Descending&start=<offset>&length=50   (%-encoded as verified)
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
 * on appno+kpdate — across the whole walk, not per page, since the pair can straddle a page
 * boundary once more than one page is read.
 */
#include "sanc_common.inc"
#include "lib/truncnotice.h"
#include "lib/seenset.h"

/* start= is the offset, length= the page size, and the walk below moves the
 * first by the second. It used to be pinned at start=0: 50 judgments of the
 * 89,604 the same response reports, twice a day, with the other 89,554 neither
 * read nor mentioned. Sorted newest-first, so page 1 is the useful one — but
 * "the useful one" is not the same claim as "all of them", and a court's
 * case-law corpus is exactly the kind of thing an investigator walks backwards
 * through. */
#define HUDOC_URL_FMT \
  "https://hudoc.echr.coe.int/app/query/results?query=contentsitename%%3DECHR%%20AND%%20" \
  "(documentcollectionid2%%3D%%22JUDGMENTS%%22)&select=itemid,docname,appno,article," \
  "conclusion,kpdate&sort=kpdate%%20Descending&start=%d&length=%d"

#define HUDOC_PAGE   50
#define HUDOC_PAGES  10   /* per run; override with HUDOC_PAGE_MAX */

/* The EN/FR pair of one judgment must be deduped, and this set used to be a
 * fixed char *seen[128]. At one page of 50 it could not overflow; at ten pages
 * it silently would, and an overflowing dedupe set does not merely miss
 * duplicates — entries past the end are mis-deduped or dropped. lib/seenset.h
 * exists for exactly this and grows, so dedupe never costs records. */
static int emit_page(intel_sink *sink, const cJSON *results, const cJSON *rc,
                     seen_set *seen) {
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
    if (!seen_add(seen, key)) continue;

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
  int pages_max = HUDOC_PAGES;
  const char *pm = jo_env("HUDOC_PAGE_MAX");
  if (pm && atoi(pm) > 0) pages_max = atoi(pm);

  seen_set seen = {0};
  int n = 0, pages_read = 0, truncated = 0;
  long available = -1;
  char url[640];

  for (int page = 0; page < pages_max; page++) {
    snprintf(url, sizeof url, HUDOC_URL_FMT, page * HUDOC_PAGE, HUDOC_PAGE);
    cJSON *doc = sanc_http_json(ctx, url, NULL, 45000, "echr-hudoc");
    if (!doc) {
      if (page == 0) { seen_free(&seen); return -1; }    /* dead endpoint */
      truncated = 1;                                     /* died mid-walk */
      break;
    }
    const cJSON *results = cJSON_GetObjectItem(doc, "results");
    const cJSON *rc = cJSON_GetObjectItem(doc, "resultcount");
    if (available < 0 && cJSON_IsNumber(rc)) available = (long)rc->valuedouble;

    int got = cJSON_IsArray(results) ? cJSON_GetArraySize(results) : 0;
    n += emit_page(sink, results, rc, &seen);
    cJSON_Delete(doc);
    pages_read++;

    /* A short page is the upstream saying it is finished. */
    if (got < HUDOC_PAGE) break;
    if (page + 1 >= pages_max) truncated = 1;
  }

  fprintf(stderr, "[echr-hudoc] emitted %d across %d page(s)%s\n",
          n, pages_read, truncated ? " (TRUNCATED)" : "");

  if (truncated)
    trunc_notice(sink, "echr-hudoc-judgments", url, NULL, n, available,
                 "the per-run page budget stopped the walk through a corpus of "
                 "tens of thousands of judgments",
                 "raise HUDOC_PAGE_MAX — the walk is newest-first, so each run "
                 "already carries the most recent judgments");
  seen_free(&seen);
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
