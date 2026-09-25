/* Cook County (Chicago) Medical Examiner case archive (Socrata row tier).
 * Endpoint: https://datacatalog.cookcountyil.gov/resource/cjeq-bs86.json
 *           ?$limit=100&$order=incident_date%20DESC
 * Bare JSON array, updated within hours of the incident. Emits, per case,
 * every scalar the API returned: casenumber, incident_date, death_date, age,
 * gender, race, latino, cold_related, heat_related, gunrelated, primarycause,
 * incident_address.
 * R2: latitude/longitude are STRINGS and are parsed as such; rows without
 * them get no location - a county centroid is never substituted. Keyless.
 * Licence: Cook County open data, public record. Rows are already
 * de-identified by the publisher (no names). */
#include "od_shared.inc"

#define SID "socrata-cookcounty-medical-examiner"
/* `$offset=0` seeds lib/pagewalk.c's offset walk — pw_walk only ever advances a
 * parameter the URL already carries. `,:id` makes the sort TOTAL so an offset
 * window cannot drift across a tie in incident_date. Measured 2026-09-19:
 * 98,732 rows upstream, of which the single-page collector kept 100. */
static const char *URL =
  "https://datacatalog.cookcountyil.gov/resource/cjeq-bs86.json"
  "?$limit=100&$offset=0&$order=incident_date%20DESC,:id";
static const char *const TITLE_KEYS[] = { "primarycause", "casenumber", NULL };
static const char *const SUM_KEYS[] = { "incident_address", "gender", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "medical-examiner-case";
  sp.tags_json = "[\"opendata\",\"mortality\",\"us\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "casenumber";
  sp.date_key = "incident_date";
  sp.title_prefix = "Cook County ME:";
  return od_wrc(SID, od_walk_rows(ctx, sink, SID, URL, NULL, &sp));
}

static const source_def od_socrata_cookcounty_me_def = {
  .id = SID, .collector = "government",
  .name = "Cook County Medical Examiner case archive (Socrata)",
  .update_interval_sec = 21600, .run = run,
  .category = "government", .type = "api",
  .url = "https://datacatalog.cookcountyil.gov/resource/cjeq-bs86.json?$limit=100&$order=incident_date%20DESC",
  .description = "Cook County (Chicago) death investigations with cause, demographics, heat/cold/gun flags and geocoded incident location",
  .license = "Cook County open data, public record (publisher de-identified)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_cookcounty_me_def)
