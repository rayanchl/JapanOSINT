/* collectors/sources/cyi_dshield_topips.c
 * DShield / SANS ISC top attacking source IPs — the loudest scanners on the
 * internet right now, ranked by firewall-log reports across the DShield sensor
 * network, with the number of distinct targets each hit (which is what tells a
 * mass scanner from a single-target brute-forcer).
 * Endpoint: https://isc.sans.edu/api/topips/records/100?json          (keyless)
 * TRAP (parse_notes): "Content-Type is text/json (not application/json) — do
 * not gate the parser on the MIME type." feed_get_json never inspects the MIME,
 * so the body is parsed regardless. This is a different endpoint from the
 * existing sans-isc source, which only reads /api/infocon.
 * DShield returns its numbers as strings in some builds, so every numeric field
 * is read through a guarded parse. No coordinates -> has_geo 0 (R2).
 * Licence: DShield/SANS ISC data is CC BY-NC-SA 2.5 — NON-COMMERCIAL use only;
 * confirm terms before any commercial deployment.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://isc.sans.edu/api/topips/records/100?json"

/* The ISC JSON APIs wrap their rows differently per endpoint; find the array of
 * objects wherever it sits rather than hardcoding a key. */
static const cJSON *find_rows(const cJSON *doc) {
  if (cJSON_IsArray(doc)) return doc;
  if (!cJSON_IsObject(doc)) return NULL;
  const cJSON *c;
  cJSON_ArrayForEach(c, doc)
    if (cJSON_IsArray(c) && cJSON_GetArraySize(c) > 0) return c;
  /* object-of-objects form: {"0":{...},"1":{...}} */
  cJSON_ArrayForEach(c, doc)
    if (cJSON_IsObject(c) && cJSON_GetObjectItem(c, "source")) return doc;
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 25000);
  if (!doc) { fprintf(stderr, "[dshield-topips] fetch/parse failed\n"); return -1; }

  const cJSON *rows = find_rows(doc);
  if (!rows) {
    cJSON_Delete(doc);
    fprintf(stderr, "[dshield-topips] no row array in payload\n");
    return 0;
  }

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    if (!cJSON_IsObject(r)) continue;
    const char *src = jo_sv(r, "source");
    if (!src) continue;
    double rank = 0, reports = 0, targets = 0;
    int hr = jo_numf(r, "rank", &rank);
    int hp = jo_numf(r, "reports", &reports);
    int ht = jo_numf(r, "targets", &targets);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "ip", src);
    if (hr) cJSON_AddNumberToObject(p, "rank", rank);
    if (hp) cJSON_AddNumberToObject(p, "reports", reports);
    if (ht) cJSON_AddNumberToObject(p, "distinct_targets", targets);
    cJSON_AddStringToObject(p, "sensor_network", "DShield / SANS ISC");
    cJSON_AddStringToObject(p, "licence", "CC BY-NC-SA 2.5 (non-commercial)");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[192];
    snprintf(summary, sizeof summary, "rank %.0f · %.0f reports · %.0f distinct targets",
             rank, reports, targets);
    char link[160];
    snprintf(link, sizeof link, "https://isc.sans.edu/ipinfo.html?ip=%s", src);

    intel_item it = {0};
    it.remote_key      = src;
    it.title           = src;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "top-attacking-ip";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"scanning\",\"dshield\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[dshield-topips] emitted %d\n", n);
  return 0;
}

static const source_def cyi_dshield_topips_def = {
  .id = "dshield-topips", .collector = "cyber",
  .name = "DShield/ISC top attacking source IPs",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "The loudest scanners on the internet right now, ranked by firewall-log reports across the DShield sensor network, with distinct target counts.",
  .license = "DShield/SANS ISC — CC BY-NC-SA 2.5, NON-COMMERCIAL; confirm terms before commercial deployment.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_dshield_topips_def)
