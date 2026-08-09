/* US Consolidated Screening List (International Trade Administration) — 11 agency lists merged.
 *
 * Endpoint : https://data.trade.gov/downloadable_consolidated_screening_list/v1/consolidated.json
 * Format   : one JSON object, {"results":[…], "total":…, "sources_used":[…]}.
 * Verified : HTTP 200, 33 MB, 25,921 records; first record "BANK OF KUNLUN CO LTD" (Capta List
 *            (CAP) - Treasury Department) with four addresses and two alt_names.
 * Keyless  : yes.
 * Emits    : per record — name, the agency list it came from (`source`, also used as the row's
 *            sub_source_id), alt_names, full postal addresses as returned, sanctions programs,
 *            identifier documents (passport / SWIFT-BIC / tax id), remarks, Federal Register
 *            notice, start date and the publisher's own list URL.
 * Geometry : NONE (R2). The records carry postal addresses but no coordinates, and an address
 *            is NOT geocoded here — a screening hit must never be drawn on a map as a fact.
 * Licence  : US Government work, public domain; ITA publishes the file explicitly for
 *            denied-party screening use.
 *
 * Notes    : the old api.trade.gov host now serves an EXPIRED TLS certificate — data.trade.gov
 *            is the working host. This is the canonical export-control screening source: BIS
 *            Entity List, Denied Persons, Unverified List, Military End User, State Dept ITAR
 *            debarred parties and AECA debarments, plus OFAC SDN/consolidated.
 *            The file is large: the download takes a long timeout and rows per run are capped
 *            by JO_SANC_MAX_ROWS (default 5000) so one daily run cannot flood the store.
 */
#include "sanc_common.inc"

/* Flatten one CSL address object exactly as returned (no geocoding, R2). */
static void csl_address(const cJSON *a, char *out, size_t n) {
  const char *parts[5];
  parts[0] = jo_sv(a, "address");
  parts[1] = jo_sv(a, "city");
  parts[2] = jo_sv(a, "state");
  parts[3] = jo_sv(a, "postal_code");
  parts[4] = jo_sv(a, "country");
  size_t j = 0;
  out[0] = 0;
  for (int i = 0; i < 5; i++) {
    if (!parts[i]) continue;
    size_t need = strlen(parts[i]) + (j ? 2 : 0);
    if (j + need >= n - 1) break;
    if (j) { strcpy(out + j, ", "); j += 2; }
    strcpy(out + j, parts[i]);
    j += strlen(parts[i]);
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = sanc_http_json(
      ctx, "https://data.trade.gov/downloadable_consolidated_screening_list/v1/consolidated.json",
      NULL, 180000, "us-trade-csl");
  if (!doc) return -1;

  cJSON *results = cJSON_GetObjectItem(doc, "results");
  int max_rows = sanc_env_int("JO_SANC_MAX_ROWS", 5000);
  int n = 0;
  const cJSON *rec;
  cJSON_ArrayForEach(rec, results) {
    if (n >= max_rows) break;
    const char *name = jo_sv(rec, "name");
    const char *id = jo_sv(rec, "id");
    if (!name) continue;                       /* no fetched name -> no row (R1) */

    const char *list = jo_sv(rec, "source");
    const char *type = jo_sv(rec, "type");
    const char *remarks = jo_sv(rec, "remarks");
    const char *start = jo_sv(rec, "start_date");
    const char *fr = jo_sv(rec, "federal_register_notice");
    const char *list_url = jo_sv(rec, "source_list_url");
    const char *info_url = jo_sv(rec, "source_information_url");

    char programs[256];
    sanc_join(cJSON_GetObjectItem(rec, "programs"), ", ", 12, programs, sizeof programs);
    char alts[512];
    sanc_join(cJSON_GetObjectItem(rec, "alt_names"), " / ", 10, alts, sizeof alts);

    cJSON *addrs = cJSON_CreateArray();
    const cJSON *a;
    cJSON_ArrayForEach(a, cJSON_GetObjectItem(rec, "addresses")) {
      char line[400];
      csl_address(a, line, sizeof line);
      if (line[0]) cJSON_AddItemToArray(addrs, cJSON_CreateString(line));
      if (cJSON_GetArraySize(addrs) >= 8) break;
    }
    cJSON *ids = cJSON_CreateArray();
    const cJSON *d;
    cJSON_ArrayForEach(d, cJSON_GetObjectItem(rec, "ids")) {
      const char *t = jo_sv(d, "type"), *num = jo_sv(d, "number");
      if (!t && !num) continue;
      char line[256];
      snprintf(line, sizeof line, "%s%s%s", t ? t : "", (t && num) ? ": " : "",
               num ? num : "");
      cJSON_AddItemToArray(ids, cJSON_CreateString(line));
      if (cJSON_GetArraySize(ids) >= 8) break;
    }

    char summary[512];
    snprintf(summary, sizeof summary, "%s%s%s%s%s", list ? list : "US screening list",
             type ? " · " : "", type ? type : "",
             programs[0] ? " · " : "", programs);

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "name", name);
    sanc_add(body, "list", list);
    sanc_add(body, "type", type);
    if (alts[0]) sanc_add(body, "alt_names", alts);
    if (programs[0]) sanc_add(body, "programs", programs);
    cJSON_AddItemToObject(body, "addresses", cJSON_Duplicate(addrs, 1));
    cJSON_AddItemToObject(body, "ids", cJSON_Duplicate(ids, 1));
    sanc_add(body, "federal_register_notice", fr);
    sanc_add(body, "start_date", start);
    sanc_add(body, "remarks", remarks);
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "list", list);
    sanc_add(props, "entity_id", id);
    sanc_add(props, "entity_type", type);
    if (programs[0]) sanc_add(props, "programs", programs);
    if (alts[0]) sanc_add(props, "alt_names", alts);
    cJSON_AddItemToObject(props, "addresses", addrs);   /* ownership transferred */
    cJSON_AddItemToObject(props, "ids", ids);           /* ownership transferred */
    sanc_add(props, "federal_register_notice", fr);
    sanc_add(props, "source_list_url", list_url);
    sanc_add(props, "source_information_url", info_url);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = id ? id : name;
    it.title = name;
    it.summary = summary;
    it.body = bj;
    it.link = list_url ? list_url : info_url;
    it.lang = "en";
    it.published_at = start;
    it.record_type = "us-csl-entry";
    it.sub_source_id = list;      /* which of the 11 agency lists this came from */
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"sanctions\",\"screening\",\"export-control\",\"us\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj);
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[us-trade-csl] emitted %d\n", n);
  return 0;                       /* fetched fine (R3) */
}

static const source_def sanc_us_trade_csl_def = {
  .id = "us-trade-csl", .collector = "sanctions",
  .name = "US Consolidated Screening List (ITA)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.trade.gov/downloadable_consolidated_screening_list/v1/consolidated.json",
  .description = "One download covering BIS Entity List, Denied Persons List, Unverified List, Military End User List, State Dept ITAR debarred parties and AECA debarments, plus OFAC SDN/consolidated — the canonical export-control denied-party screening source.",
  .license = "US Government work, public domain (ITA publishes it explicitly for screening use).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_us_trade_csl_def)
