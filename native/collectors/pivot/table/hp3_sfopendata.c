/* collectors/pivot/table/hp3_sfopendata.c -- San Francisco's three event logs,
 * read whole across runs.
 *
 * These were generated VJSON rows (collectors/feed/generated/vsrc13_us_local_2.c)
 * asking for `$limit=500&$order=:id` under the 20-page VJSON ceiling: the
 * first 10,000 rows by internal id -- not the newest, just the first -- of
 * datasets that hold, measured 2026-10-13:
 *
 *   311 cases                vw6y-z8j6   8,986,452 rows
 *   fire / EMS calls          nuek-vuh3   7,463,230 rows
 *   building permits          i98e-djp9   1,296,756 rows
 *
 * No single run can read that, and offset paging cannot reach the end at all:
 * Socrata answered the 311 dataset at $offset=7,000,000 in 175 s. A keyset on
 * the system row id is flat at any depth (`:id > cursor`, 7 s), and it agrees
 * with `$order=:id` row for row (checked on 311 and on fire calls). So each row
 * walks the whole dataset in :id order, 20 pages of 10,000 per run, and
 * page_resume=1 (lib/hpengine.h) keeps the next page's URL — the keyset cursor
 * is in it — across runs (core/scheduler.c's resume store): the next run
 * continues there, and after the last page the position is cleared and the
 * cycle starts again, so every row is read once a cycle (311: ~45 hourly
 * runs; fire calls: ~38; permits: ~7 six-hourly runs) and re-read in the next
 * one -- which is when a case's later updates arrive. `$select=:*,*` returns
 * the system fields too (:id, :created_at, :updated_at), which is both the
 * cursor and the record's own provenance.
 *
 * Identity: each dataset's own per-row key, measured unique over a 10,000-row
 * page -- service_request_id (10,000/10,000), rowid (10,000/10,000; one fire
 * call has a row per responding unit, so call_number is 4,314/10,000), and
 * record_id (10,000/10,000; permit_number is 9,075). */
#include "lib/hpengine.h"

static const hp_source HP3_SFOPENDATA[] = {
  { .id = "nam-sf-311-cases", .name = "City and County of San Francisco — 311 cases",
    .name_ja = "サンフランシスコ市 311 市民要望",
    .collector = "us_local", .category = "opendata", .portal = "https://data.sf.gov/d/vw6y-z8j6",
    .record_type = "service-request",
    .tags = "\"usa\",\"ca\",\"311\",\"opendata\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "https://data.sf.gov/resource/vw6y-z8j6.json?$select=:*,*&$order=:id&$limit=10000", /* exhaustive-ok: keyset-paged by next_path and resumed across runs (page_resume=1) */
    .next_path = "$last.:id",
    .next_tmpl = "https://data.sf.gov/resource/vw6y-z8j6.json?$select=:*,*&$order=:id&$limit=10000&$where=:id%20%3E%20%27{v}%27",
    .array_path = ".",
    .id_keys = "service_request_id",
    .title_keys = "service_name+address,service_name",
    .lat_key = "lat", .lon_key = "long",
    .page_max = 20, .page_resume = 1, .timeout_ms = 180000,
    .interval = 3600,
    .description = "San Francisco 311 service cases — request type and subtype, "
      "responsible agency, status and status notes, opened, updated and "
      "closed times, address, neighbourhood, supervisor district and "
      "coordinates. The whole dataset (8.99 M cases) is read across runs "
      "by keyset, 200,000 rows a run." },

  { .id = "nam-sf-fire-calls", .name = "City and County of San Francisco — fire department calls for service",
    .name_ja = "サンフランシスコ市 消防・救急出動記録",
    .collector = "us_local", .category = "hazard", .portal = "https://data.sf.gov/d/nuek-vuh3",
    .record_type = "emergency-call",
    .tags = "\"usa\",\"ca\",\"fire\",\"hazard\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "https://data.sf.gov/resource/nuek-vuh3.json?$select=:*,*&$order=:id&$limit=10000", /* exhaustive-ok: keyset-paged by next_path and resumed across runs (page_resume=1) */
    .next_path = "$last.:id",
    .next_tmpl = "https://data.sf.gov/resource/nuek-vuh3.json?$select=:*,*&$order=:id&$limit=10000&$where=:id%20%3E%20%27{v}%27",
    .array_path = ".",
    .id_keys = "rowid",
    .title_keys = "call_type+address,call_type",
    .lat_key = "case_location.coordinates.1", .lon_key = "case_location.coordinates.0",
    .page_max = 20, .page_resume = 1, .timeout_ms = 180000,
    .interval = 3600,
    .description = "SFFD calls for service, one row per responding unit — call "
      "type and final priority, received, dispatch, on-scene and "
      "transport times, disposition, battalion, station area, address "
      "and coordinates. The whole dataset (7.46 M rows) is read across "
      "runs by keyset, 200,000 rows a run." },

  { .id = "nam-sf-building-permits", .name = "City and County of San Francisco — building permits",
    .name_ja = "サンフランシスコ市 建築許可",
    .collector = "us_local", .category = "economy", .portal = "https://data.sf.gov/d/i98e-djp9",
    .record_type = "building-permit",
    .tags = "\"usa\",\"ca\",\"permits\",\"economy\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "https://data.sf.gov/resource/i98e-djp9.json?$select=:*,*&$order=:id&$limit=10000", /* exhaustive-ok: keyset-paged by next_path and resumed across runs (page_resume=1) */
    .next_path = "$last.:id",
    .next_tmpl = "https://data.sf.gov/resource/i98e-djp9.json?$select=:*,*&$order=:id&$limit=10000&$where=:id%20%3E%20%27{v}%27",
    .array_path = ".",
    .id_keys = "record_id",
    .title_keys = "description,permit_type_definition",
    .lat_key = "location.coordinates.1", .lon_key = "location.coordinates.0",
    .page_max = 20, .page_resume = 1, .timeout_ms = 180000,
    .interval = 21600,
    .description = "San Francisco DBI permits — permit type, status and status "
      "date, filed, issued and completed dates, proposed and existing "
      "use, estimated and revised cost, units, address and coordinates. "
      "The whole dataset (1.30 M permits) is read across runs by keyset, "
      "200,000 rows a run." },
};
HP_REGISTER_TABLE(HP3_SFOPENDATA)
