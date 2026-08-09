/* Baton Rouge police crime incidents (Socrata row tier) - HISTORICAL archive.
 * Endpoint: https://data.brla.gov/resource/fabb-cnnu.json
 *           ?$limit=100&$order=offense_date%20DESC
 * This is the LEGACY dataset: its most recent rows are from 2021, so it is
 * presented as a historical archive, not a live incident feed. Bare JSON
 * array. Emits, per incident: file_number, offense_date, offense_time, crime,
 * offense, offense_desc, address, district, zip.
 * R2: geolocation is present on some rows only and is the sole coordinate
 * source; rows without it get no location. Keyless.
 * Licence: City of Baton Rouge / East Baton Rouge Parish open data, public
 * record. */
#include "od_shared.c"

#define SID "socrata-brla-crime"
static const char *URL =
  "https://data.brla.gov/resource/fabb-cnnu.json"
  "?$limit=100&$order=offense_date%20DESC";
static const char *const TITLE_KEYS[] = { "crime", "offense_desc", "offense",
                                          NULL };
static const char *const SUM_KEYS[] = { "address", "district", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "crime-incident";
  sp.tags_json = "[\"opendata\",\"crime\",\"us\",\"historical\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "file_number";
  sp.date_key = "offense_date";
  sp.title_prefix = "Baton Rouge PD:";
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, NULL, &sp));
}

static const source_def od_socrata_brla_crime_def = {
  .id = SID, .collector = "government",
  .name = "Baton Rouge police crime incidents (Socrata, historical)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.brla.gov/resource/fabb-cnnu.json?$limit=100&$order=offense_date%20DESC",
  .description = "Baton Rouge PD incident-level crime reports with statute codes and addresses - legacy archive, latest rows are 2021",
  .license = "City of Baton Rouge / EBR Parish open data, public record",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_brla_crime_def)
