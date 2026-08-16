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
 *            a run makes at most JO_PAGE_MAX (20) requests: lib/pagewalk.c advances the
 *            offset= the URL already carries, and a page the throttle refuses ends the walk
 *            with a collector-truncation-notice instead of a silent stop.
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
 */
#include "sanc_common.inc"
#include "../../lib/pagewalk.h"   /* pw_walk() — house rule 2 paging */

/* sanc_http_json's Accept header and 45 s timeout, kept for every page. */
static cJSON *gi_fetch(const source_ctx *ctx, const char *url, void *ud) {
  (void)ud;
  return sanc_http_json(ctx, url, NULL, 45000, "govinfo-uscourts");
}

/* One page of packages[]. Returns #emitted and reports #records the page
 * CONTAINED through `seen`; see lib/pagewalk.h for why the two differ. */
static int gi_emit_page(const source_ctx *ctx, intel_sink *sink, const char *id,
                        cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)id;
  const char *since = (const char *)ud;
  cJSON *pkgs = cJSON_GetObjectItem(doc, "packages");
  if (!cJSON_IsArray(pkgs)) pkgs = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(pkgs)) return 0;
  *seen = cJSON_GetArraySize(pkgs);

  /* govinfo states the size of the whole collection window in `count`, a name
   * lib/pagewalk.c deliberately does not read (half the APIs here use `count`
   * for "records in THIS page"). It is unambiguous here, so republish the
   * upstream's OWN number under a name pagewalk does read, and let the
   * truncation notice carry a real records_available. Copied, never
   * computed (house rule 1). */
  {
    const cJSON *cv = cJSON_GetObjectItem(doc, "count");
    if (cJSON_IsNumber(cv) && !cJSON_GetObjectItem(doc, "totalCount"))
      cJSON_AddNumberToObject(doc, "totalCount", cv->valuedouble);
  }

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

/* House rule 2: the URL already carries the author's own offset=0&pageSize=100,
 * so pw_walk advances offset=100,200,… for as long as a page comes back full
 * and discloses whatever is left at the JO_PAGE_MAX ceiling. The page size is
 * unchanged, and no parameter is invented.
 *
 * Rate limit: with the shared DEMO_KEY this is up to JO_PAGE_MAX (20) requests
 * per run at a 12-hour cadence — inside api.data.gov's documented 30/hour and
 * 50/day allowance. If a page is nonetheless throttled, the fetch returns NULL,
 * the walk stops there and says so as a collector-truncation-notice rather than
 * pretending the collection ended. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  /* R6: a real key raises the ceiling; its absence is not an error. */
  const char *key = jo_env("GOVINFO_API_KEY");
  if (!key) {
    key = "DEMO_KEY";
    fprintf(stderr, "[govinfo-uscourts] GOVINFO_API_KEY unset — using the documented "
                    "shared DEMO_KEY (throttled ~30 req/hour/IP)\n");
  }
  char since[32];
  sanc_utc_days_ago(3, since, sizeof since);

  char url[512];
  snprintf(url, sizeof url,
           "https://api.govinfo.gov/collections/USCOURTS/%s?offset=0&pageSize=100&api_key=%s",  /* exhaustive-ok: offset=0 is where the pw_walk() below STARTS — it advances offset=100,200,… and discloses the remainder */
           since, key);

  int n = pw_walk(ctx, sink, "govinfo-uscourts", url, gi_fetch, gi_emit_page,
                  since);
  if (n < 0) return -1;
  fprintf(stderr, "[govinfo-uscourts] emitted %d (since %s)\n", n, since);
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
