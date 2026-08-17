/* GovInfo USCOURTS collection — US federal court opinions.
 *
 * Endpoint : https://api.govinfo.gov/collections/USCOURTS/<startDate>?offset=0&pageSize=100
 *              &api_key=<key>
 * Format   : JSON, {"count","nextPage","packages":[{packageId,title,dateIssued,lastModified,
 *            packageLink,docClass}]}.
 * Verified : HTTP 200; 45,105 packages in the probed window, e.g.
 *            USCOURTS-mtd-2_26-cv-00051 "Alliance for the Wild Rockies et al v. Jedra et al",
 *            dateIssued 2026-07-31.
 * Keyless  : effectively yes. GPO asks for an api.data.gov key; DEMO_KEY is documented as
 *            usable and is what the endpoint was verified with. Per R6 we read GOVINFO_API_KEY
 *            with getenv and fall back to DEMO_KEY rather than hard-failing — DEMO_KEY is
 *            throttled to roughly 30 requests/hour/IP, which is why the interval is 12 h and
 *            each run makes exactly one request.
 * Emits    : case caption, GovInfo packageId, the court code and docket number decoded from
 *            that id, date issued, last modified and the package summary link.
 * Geometry : NONE (R2).
 * Licence  : US Government work, public domain.
 *
 * Notes    : the collection path carries a start date. Pinning it to a literal would freeze
 *            the window and re-emit the same packages forever, so the date is computed as
 *            (now - 3 days) in the SAME format the verified URL used; every query parameter is
 *            unchanged. Official, authenticated opinions from district, bankruptcy and
 *            appellate courts without scraping PACER.
 *
 * Paging   : the response carries `count` and `nextPage`, and this collector used to read one
 *            page of 100 and stop — 100 of the 45,105 its own header documents, with nothing
 *            said about the rest. The window is three days, so the real page count is small,
 *            but "small" is not "one".
 *
 *            It now follows `nextPage` — the server's own link, not a guessed cursor — under a
 *            request budget, because the throttle here is real: DEMO_KEY allows roughly 30
 *            requests/hour/IP, so an unbounded walk on the shared key would spend the whole
 *            allowance and fail the next collector to ask. The budget is therefore keyed to
 *            what the caller actually has: GOVINFO_PAGE_MAX pages with a real key, a
 *            deliberately small number without one. When the budget stops the walk that is
 *            disclosed as a collector-truncation-notice carrying `count` — a bounded read that
 *            says how much it bounded (docs/SOURCE_EXHAUSTIVENESS.md), not a silent slice.
 */
#include "sanc_common.inc"
#include "lib/truncnotice.h"

/* Pages per run. Small on DEMO_KEY (shared, ~30 req/hour/IP), generous with a
 * real key; either can be overridden by GOVINFO_PAGE_MAX. */
#define GOVINFO_PAGES_DEMO   4
#define GOVINFO_PAGES_KEYED 40

/* The server's own next-page link, made usable: govinfo does not always carry
 * the api_key through, and a next link without it answers 403. NULL when this
 * was the last page. Caller frees. */
static char *next_page_url(const cJSON *doc, const char *key) {
  const char *np = jo_sv(doc, "nextPage");
  if (!np || !*np || strncmp(np, "http", 4) != 0) return NULL;
  size_t n = strlen(np) + strlen(key) + 16;
  char *out = malloc(n);
  if (!out) return NULL;
  if (strstr(np, "api_key="))
    snprintf(out, n, "%s", np);
  else
    snprintf(out, n, "%s%capi_key=%s", np, strchr(np, '?') ? '&' : '?', key);
  return out;
}

static int emit_packages(intel_sink *sink, const cJSON *pkgs, const char *since) {
  int n = 0;
  const cJSON *p;
  cJSON_ArrayForEach(p, pkgs) {
    const char *pid = jo_sv(p, "packageId");
    const char *title = jo_sv(p, "title");
    if (!pid) continue;

    /* packageId is "USCOURTS-<court>-<docket with _ for :>" — decode both. */
    char court[24] = {0}, docket[64] = {0};
    if (strncmp(pid, "USCOURTS-", 9) == 0) {
      const char *c = pid + 9;
      const char *dash = strchr(c, '-');
      if (dash && (size_t)(dash - c) < sizeof court) {
        memcpy(court, c, (size_t)(dash - c));
        court[dash - c] = 0;
        snprintf(docket, sizeof docket, "%s", dash + 1);
        for (char *q = docket; *q; q++) if (*q == '_') *q = ':';
      }
    }

    const char *issued = jo_sv(p, "dateIssued");
    const char *modified = jo_sv(p, "lastModified");
    const char *plink = jo_sv(p, "packageLink");
    const char *docclass = jo_sv(p, "docClass");

    char link[256];
    snprintf(link, sizeof link, "https://www.govinfo.gov/app/details/%s", pid);

    char summary[320];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             court[0] ? court : "USCOURTS", docket[0] ? " " : "", docket,
             issued ? " · issued " : "", issued ? issued : "");

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "case", title);
    sanc_add(body, "package_id", pid);
    sanc_add(body, "court_code", court[0] ? court : NULL);
    sanc_add(body, "docket_number", docket[0] ? docket : NULL);
    sanc_add(body, "date_issued", issued);
    sanc_add(body, "last_modified", modified);
    sanc_add(body, "package_link", plink);
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "package_id", pid);
    sanc_add(props, "court_code", court[0] ? court : NULL);
    sanc_add(props, "docket_number", docket[0] ? docket : NULL);
    sanc_add(props, "doc_class", docclass);
    sanc_add(props, "package_link", plink);
    sanc_add(props, "last_modified", modified);
    sanc_add(props, "window_start", since);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = pid;
    it.title = title ? title : pid;
    it.summary = summary;
    it.body = bj;
    it.link = link;
    it.lang = "en";
    it.published_at = issued;
    it.record_type = "uscourts-opinion";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"court\",\"opinion\",\"us\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj);
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* R6: a real key raises the ceiling; its absence is not an error. */
  const char *key = jo_env("GOVINFO_API_KEY");
  int keyed = key != NULL;
  if (!keyed) {
    key = "DEMO_KEY";
    fprintf(stderr, "[govinfo-uscourts] GOVINFO_API_KEY unset — using the documented "
                    "shared DEMO_KEY (throttled ~30 req/hour/IP)\n");
  }
  int budget = keyed ? GOVINFO_PAGES_KEYED : GOVINFO_PAGES_DEMO;
  const char *pm = jo_env("GOVINFO_PAGE_MAX");
  if (pm && atoi(pm) > 0) budget = atoi(pm);

  char since[32];
  sanc_utc_days_ago(3, since, sizeof since);

  char url[512];
  snprintf(url, sizeof url,
           "https://api.govinfo.gov/collections/USCOURTS/%s?offset=0&pageSize=100&api_key=%s",
           since, key);

  char *page_url = strdup(url);
  if (!page_url) return -1;

  int n = 0, pages = 0, truncated = 0;
  long available = -1;
  while (page_url && pages < budget) {
    cJSON *doc = sanc_http_json(ctx, page_url, NULL, 45000, "govinfo-uscourts");
    if (!doc) {
      /* A dead FIRST page is a dead endpoint and belongs to the caller as an
       * error. A failure mid-walk keeps the real records already emitted and
       * discloses the shortfall below. */
      if (pages == 0) { free(page_url); return -1; }
      truncated = 1;
      break;
    }
    if (available < 0) {
      const cJSON *c = cJSON_GetObjectItem(doc, "count");
      if (cJSON_IsNumber(c)) available = (long)c->valuedouble;
    }

    const cJSON *pkgs = cJSON_GetObjectItem(doc, "packages");
    if (!cJSON_IsArray(pkgs)) pkgs = cJSON_GetObjectItem(doc, "results");
    n += emit_packages(sink, pkgs, since);

    char *next = next_page_url(doc, key);
    cJSON_Delete(doc);
    free(page_url);
    page_url = next;
    pages++;
    if (page_url && pages >= budget) truncated = 1;   /* budget stop */
  }
  free(page_url);

  fprintf(stderr, "[govinfo-uscourts] emitted %d across %d page(s) (since %s)%s\n",
          n, pages, since, truncated ? " (TRUNCATED)" : "");

  if (truncated)
    trunc_notice(sink, "govinfo-uscourts-opinions", url, NULL, n, available,
                 keyed ? "the per-run page budget stopped the walk"
                       : "the per-run page budget stopped the walk — the shared "
                         "DEMO_KEY allows roughly 30 requests/hour/IP",
                 "set GOVINFO_API_KEY for a real key, or raise GOVINFO_PAGE_MAX");
  return 0;
}

static const source_def sanc_govinfo_uscourts_def = {
  .id = "govinfo-uscourts-opinions", .collector = "sanctions",
  .name = "GovInfo USCOURTS — US federal court opinions",
  .update_interval_sec = 43200, .run = run,
  .category = "government", .type = "api",
  .url = "https://api.govinfo.gov/collections/USCOURTS/",
  .description = "Official, authenticated opinions from US district, bankruptcy and appellate courts as a dated package feed — case caption, court code and issue date without scraping PACER.",
  .license = "US Government work, public domain. GPO asks for an api.data.gov key; the shared DEMO_KEY is documented as usable but rate-limited.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_govinfo_uscourts_def)
