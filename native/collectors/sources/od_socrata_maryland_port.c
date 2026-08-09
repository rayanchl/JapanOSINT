/* Maryland Port Administration general cargo (Socrata row tier).
 * Endpoint: https://opendata.maryland.gov/resource/2ir4-626w.json
 *           ?$limit=100&$order=month%20DESC
 * Bare JSON array; the tonnage fields are STRINGS upstream and are emitted
 * verbatim as returned rather than being re-typed. Emits, per month:
 * total_container_tons, total_automobile_tons, total_ro_ro_tons,
 * forest_products_break_bulk_, steel_amp_other_metals_tons and the other
 * commodity columns the API returned. Keyless.
 * Licence: State of Maryland open data, public record. */
#include "od_shared.c"

#define SID "socrata-maryland-port-cargo"
static const char *URL =
  "https://opendata.maryland.gov/resource/2ir4-626w.json"
  "?$limit=100&$order=month%20DESC";
static const char *const TITLE_KEYS[] = { "month", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "port-cargo-throughput";
  sp.tags_json = "[\"opendata\",\"trade\",\"maritime\",\"us\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.id_key = "month";
  sp.date_key = "month";
  sp.title_prefix = "Port of Baltimore cargo";
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, NULL, &sp));
}

static const source_def od_socrata_maryland_port_def = {
  .id = SID, .collector = "statistics",
  .name = "Maryland Port Administration general cargo (Socrata)",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://opendata.maryland.gov/resource/2ir4-626w.json?$limit=100&$order=month%20DESC",
  .description = "Port of Baltimore monthly cargo throughput by commodity: container, automobile, ro-ro, break-bulk and metals tonnage",
  .license = "State of Maryland open data, public record",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_maryland_port_def)
