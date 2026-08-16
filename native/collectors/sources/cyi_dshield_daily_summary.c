/* collectors/sources/cyi_dshield_daily_summary.c
 * DShield / SANS ISC daily attack-volume summary — total firewall-log records,
 * unique attacking sources and unique targets per day. The internet-wide
 * background-noise baseline that makes a spike interpretable.
 * Endpoint: https://isc.sans.edu/api/dailysummary/<yyyy-mm-dd>?json   (keyless)
 * parse_notes: "Passing one date returns that date plus the following days up
 * to today (3 rows for a date 2 days back)" — so the request is anchored two
 * days back and every returned day is emitted. The Content-Type is text/json,
 * which the parser deliberately ignores.
 * Numbers may arrive as strings, so every numeric field is read through a
 * guarded parse. No coordinates -> has_geo 0 (R2).
 * Licence: DShield/SANS ISC data is CC BY-NC-SA 2.5 — NON-COMMERCIAL use only;
 * confirm terms before any commercial deployment.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const cJSON *find_rows(const cJSON *doc) {
  if (cJSON_IsArray(doc)) return doc;
  if (!cJSON_IsObject(doc)) return NULL;
  const cJSON *c;
  cJSON_ArrayForEach(c, doc)
    if (cJSON_IsArray(c) && cJSON_GetArraySize(c) > 0) return c;
  return NULL;                       /* single-object form handled by caller */
}

static int emit_row(intel_sink *sink, const cJSON *r) {
  const char *date = jo_sv(r, "date");
  if (!date) return 0;
  double records = 0, sources = 0, targets = 0;
  int hr = jo_numf(r, "records", &records);
  int hs = jo_numf(r, "sources", &sources);
  int ht = jo_numf(r, "targets", &targets);
  if (!hr && !hs && !ht) return 0;         /* nothing measured -> no row */

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "date", date);
  if (hr) cJSON_AddNumberToObject(p, "records", records);
  if (hs) cJSON_AddNumberToObject(p, "unique_sources", sources);
  if (ht) cJSON_AddNumberToObject(p, "unique_targets", targets);
  cJSON_AddStringToObject(p, "sensor_network", "DShield / SANS ISC");
  cJSON_AddStringToObject(p, "licence", "CC BY-NC-SA 2.5 (non-commercial)");
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char title[128];
  snprintf(title, sizeof title, "DShield daily attack volume %s", date);
  char summary[224];
  snprintf(summary, sizeof summary,
           "%.0f firewall-log records from %.0f sources against %.0f targets",
           records, sources, targets);

  intel_item it = {0};
  it.remote_key      = date;
  it.title           = title;
  it.summary         = summary;
  it.link            = "https://isc.sans.edu/";
  it.lang            = "en";
  it.published_at    = date;
  it.record_type     = "attack-volume-daily";
  it.properties_json = pj;
  it.tags_json       = "[\"cyber\",\"telemetry\",\"dshield\"]";
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t t = time(NULL) - 2 * 24 * 3600;
  struct tm tmv;
  char day[16];
#if defined(_WIN32)
  if (gmtime_s(&tmv, &t) != 0) return -1;
#else
  if (!gmtime_r(&t, &tmv)) return -1;
#endif
  strftime(day, sizeof day, "%Y-%m-%d", &tmv);

  char url[128];
  snprintf(url, sizeof url, "https://isc.sans.edu/api/dailysummary/%s?json", day);

  cJSON *doc = feed_get_json(ctx->http, url, 20000);
  if (!doc) { fprintf(stderr, "[dshield-daily-summary] fetch/parse failed\n"); return -1; }

  int n = 0;
  const cJSON *rows = find_rows(doc);
  if (rows) {
    const cJSON *r;
    cJSON_ArrayForEach(r, rows) if (cJSON_IsObject(r)) n += emit_row(sink, r);
  } else if (cJSON_IsObject(doc)) {
    n += emit_row(sink, doc);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[dshield-daily-summary] emitted %d (from %s)\n", n, day);
  return 0;
}

static const source_def cyi_dshield_daily_summary_def = {
  .id = "dshield-daily-summary", .collector = "cyber",
  .name = "DShield daily attack volume summary",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://isc.sans.edu/api/dailysummary/",
  .description = "Daily global attack-volume time series: firewall-log records, unique attacking sources and unique targets — the baseline that makes a spike interpretable.",
  .license = "DShield/SANS ISC — CC BY-NC-SA 2.5, NON-COMMERCIAL; confirm terms before commercial deployment.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_dshield_daily_summary_def)
