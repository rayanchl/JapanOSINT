/* City of Calgary 311 service requests (Socrata SoDA 2.x row tier).
 * Endpoint: https://data.calgary.ca/resource/iahh-g8bj.json
 *           ?$limit=100&$order=requested_date%20DESC
 * Bare JSON array. Emits, per request, every scalar the API returned:
 * service_request_id, requested_date, updated_date, closed_date,
 * status_description, source, agency_responsible, comm_name.
 * R2: latitude/longitude are checked per row and older rows omit them - when
 * they are absent no location is set, and none is substituted. Keyless.
 * Licence: Open Government Licence - City of Calgary. */
#include "od_shared.inc"

#define SID "socrata-calgary-311"
/* `$offset=0` seeds lib/pagewalk.c's offset walk — pw_walk only ever advances a
 * parameter the URL already carries. `,:id` makes the sort TOTAL so an offset
 * window cannot drift across a tie in requested_date. Measured 2026-09-19:
 * 7,488,797 rows upstream, of which the single-page collector kept 100. */
static const char *URL =
  "https://data.calgary.ca/resource/iahh-g8bj.json"
  "?$limit=100&$offset=0&$order=requested_date%20DESC,:id";
static const char *const TITLE_KEYS[] = { "service_name", "agency_responsible",
                                          "service_request_id", NULL };
static const char *const SUM_KEYS[] = { "status_description", "comm_name",
                                        NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "civic-311-request";
  sp.tags_json = "[\"opendata\",\"311\",\"ca\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "service_request_id";
  sp.date_key = "requested_date";
  sp.title_prefix = "Calgary 311:";
  return od_wrc(SID, od_walk_rows(ctx, sink, SID, URL, NULL, &sp));
}

static const source_def od_socrata_calgary_311_def = {
  .id = SID, .collector = "government",
  .name = "City of Calgary 311 service requests (Socrata)",
  .update_interval_sec = 21600, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.calgary.ca/resource/iahh-g8bj.json?$limit=100&$order=requested_date%20DESC",
  .description = "Calgary 311 service requests: responsible agency, status, community and per-row coordinates when published",
  .license = "Open Government Licence - City of Calgary",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_calgary_311_def)
