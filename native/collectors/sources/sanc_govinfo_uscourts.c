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
 */
#include "sanc_common.inc"

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
           "https://api.govinfo.gov/collections/USCOURTS/%s?offset=0&pageSize=100&api_key=%s",
           since, key);

  cJSON *doc = sanc_http_json(ctx, url, NULL, 45000, "govinfo-uscourts");
  if (!doc) return -1;

  cJSON *pkgs = cJSON_GetObjectItem(doc, "packages");
  if (!cJSON_IsArray(pkgs)) pkgs = cJSON_GetObjectItem(doc, "results");

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
  cJSON_Delete(doc);
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
