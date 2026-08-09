/* CourtListener REST API v4 — opinion search. ENTITY PIVOT (on-demand).
 *
 * Endpoint : https://www.courtlistener.com/api/rest/v4/search/?q=<entity>&type=o
 * Format   : JSON, {"count","next","previous","results":[…]}.
 * Verified : HTTP 200 with results and NO Authorization header (probe term "sanctions" ->
 *            358,159 matches, 20 results per page). Re-probed with a corporate name:
 *            "In Re Application of Gorsoan Ltd. v. Bullock", 2d Cir., dateFiled 2016-06-08,
 *            docket 15-57-cv.
 * Keyless  : yes, anonymous access is permitted and rate-limited. A free API token raises the
 *            ceiling: COURTLISTENER_TOKEN is read with getenv (R6) and sent as
 *            "Authorization: Token …" when present; its absence is NOT an error.
 * Emits    : case name, court and court_id, date filed, docket number, citations, citation
 *            count and the opinion URL — one row per matched opinion.
 * Geometry : NONE (R2).
 * Licence  : Free Law Project; the underlying data is public-domain court output. Be polite
 *            and cache — this runs only on an explicit pivot, never on a schedule.
 *
 * Notes    : the pre-existing COURTLISTENER id only builds a courtlistener.com/?q= web URL and
 *            fetches nothing; this endpoint is what makes that pivot real. type=r would search
 *            RECAP dockets instead of opinions. Paging is by cursor via the `next` URL; one
 *            page per pivot is enough and keeps us inside the anonymous rate limit.
 *
 * NAME MATCHING: this collector does NO fuzzy matching. It hands the query to CourtListener's
 * own search and emits what CourtListener returned. Substring matching in-process is exactly
 * how a previous audit ended up matching "PUTIN" inside "COMPUTING".
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;                    /* no pivot value -> honest empty */

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[600];
  snprintf(url, sizeof url,
           "https://www.courtlistener.com/api/rest/v4/search/?q=%s&type=o", enc);
  free(enc);

  char authbuf[160];
  const char *auth = NULL;
  const char *tok = jo_env("COURTLISTENER_TOKEN");
  if (tok) {
    snprintf(authbuf, sizeof authbuf, "Authorization: Token %s", tok);
    auth = authbuf;
  }

  cJSON *doc = sanc_http_json(ctx, url, auth, 30000, "courtlistener");
  if (!doc) return -1;

  const cJSON *count = cJSON_GetObjectItem(doc, "count");
  cJSON *results = cJSON_GetObjectItem(doc, "results");
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, results) {
    const char *cname = jo_sv(r, "caseName");
    if (!cname) cname = jo_sv(r, "caseNameFull");
    if (!cname) continue;

    const char *court = jo_sv(r, "court");
    const char *court_id = jo_sv(r, "court_id");
    const char *filed = jo_sv(r, "dateFiled");
    const char *docket = jo_sv(r, "docketNumber");
    const char *abs = jo_sv(r, "absolute_url");
    const char *status = jo_sv(r, "status");
    const char *judge = jo_sv(r, "judge");
    const cJSON *cites = cJSON_GetObjectItem(r, "citeCount");
    const cJSON *cluster = cJSON_GetObjectItem(r, "cluster_id");

    char citation[256];
    sanc_join(cJSON_GetObjectItem(r, "citation"), "; ", 4, citation, sizeof citation);

    char link[512] = {0};
    if (abs) snprintf(link, sizeof link, "https://www.courtlistener.com%s", abs);

    char summary[400];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             court ? court : (court_id ? court_id : "US court"),
             filed ? " · " : "", filed ? filed : "",
             docket ? " · " : "", docket ? docket : "");

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "case", cname);
    sanc_add(body, "court", court);
    sanc_add(body, "court_id", court_id);
    sanc_add(body, "date_filed", filed);
    sanc_add(body, "docket_number", docket);
    if (citation[0]) sanc_add(body, "citation", citation);
    sanc_add(body, "precedential_status", status);
    sanc_add(body, "judge", judge);
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "service", "COURTLISTENER_OPINIONS");
    sanc_add(props, "query", q);
    sanc_add(props, "court", court);
    sanc_add(props, "court_id", court_id);
    sanc_add(props, "docket_number", docket);
    sanc_add(props, "date_filed", filed);
    if (citation[0]) sanc_add(props, "citation", citation);
    if (cJSON_IsNumber(cites))
      cJSON_AddNumberToObject(props, "cite_count", cites->valuedouble);
    if (cJSON_IsNumber(cluster))
      cJSON_AddNumberToObject(props, "cluster_id", cluster->valuedouble);
    if (cJSON_IsNumber(count))
      cJSON_AddNumberToObject(props, "total_matches", count->valuedouble);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = link[0] ? link : cname;
    it.title = cname;
    it.summary = summary;
    it.body = bj;
    it.link = link[0] ? link : NULL;
    it.lang = "en";
    it.published_at = filed;
    it.record_type = "courtlistener-opinion";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"osint-search\",\"court\",\"opinion\",\"us\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj);
    free(pj);
  }
  cJSON_Delete(doc);
  /* A pivot that matches NOTHING is a clean result, not a failure: it means the
   * name does not appear in ~10 million US opinions. R3 — return 0. */
  fprintf(stderr, "[courtlistener] emitted %d for \"%s\"\n", n, q);
  return 0;
}

static const source_def sanc_courtlistener_opinions_def = {
  .id = "COURTLISTENER_OPINIONS", .collector = "osint",
  .name = "CourtListener opinion search",
  .update_interval_sec = 0, .run = run,      /* on-demand entity pivot only */
  .category = "government", .type = "api",
  .url = "https://www.courtlistener.com/api/rest/v4/search/",
  .description = "Name pivot into ~10 million US federal and state opinions with structured court, docket and filing-date fields — has this person or company been litigated against, where, and when.",
  .license = "Free Law Project; underlying data is public-domain court output. Anonymous access permitted but rate-limited.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_courtlistener_opinions_def)
