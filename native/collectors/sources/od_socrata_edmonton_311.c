/* City of Edmonton 311 service requests (Socrata SoDA 2.x row tier).
 * Endpoint: https://data.edmonton.ca/resource/q7ua-agfg.json
 *           ?$limit=100&$order=date_created%20DESC
 * The response is a bare JSON ARRAY of rows (no envelope); the $ prefixes are
 * literal in the query string. Emits, per request, every scalar the API
 * returned: row_id, date_created, date_closed, request_status,
 * service_category, year, month. No coordinates are published on this
 * dataset, so has_geo is never set (R2). Keyless.
 * Licence: Open Government Licence - City of Edmonton. */
#include "od_shared.c"

#define SID "socrata-edmonton-311"
static const char *URL =
  "https://data.edmonton.ca/resource/q7ua-agfg.json"
  "?$limit=100&$order=date_created%20DESC";
static const char *const TITLE_KEYS[] = { "service_category", "description",
                                          "row_id", NULL };
static const char *const SUM_KEYS[] = { "request_status", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "civic-311-request";
  sp.tags_json = "[\"opendata\",\"311\",\"ca\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "row_id";
  sp.date_key = "date_created";
  sp.title_prefix = "Edmonton 311:";
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, NULL, &sp));
}

static const source_def od_socrata_edmonton_311_def = {
  .id = SID, .collector = "government",
  .name = "City of Edmonton 311 service requests (Socrata)",
  .update_interval_sec = 21600, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.edmonton.ca/resource/q7ua-agfg.json?$limit=100&$order=date_created%20DESC",
  .description = "Edmonton 311 civic service requests: category, status, created and closed timestamps",
  .license = "Open Government Licence - City of Edmonton",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_edmonton_311_def)
