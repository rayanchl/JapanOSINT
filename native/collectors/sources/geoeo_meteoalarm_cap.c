/* MeteoAlarm — the pan-European severe weather warning aggregator (EUMETNET),
 * every member national met service's warnings normalised into CAP.
 *
 * Endpoint (keyless), the WORKING per-country Atom pattern:
 *   https://feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-<country>
 * The Italy feed is the one probed live (45 warnings, including Extreme heat);
 * the sibling country feeds use the identical schema and are polled
 * best-effort — a country feed that fails does NOT fail the run, only the
 * verified feed does.
 * Emits per <entry>: cap:event, cap:severity, cap:certainty, cap:urgency,
 *   cap:areaDesc, the EMMA_ID region code from cap:geocode, cap:onset,
 *   cap:expires, cap:identifier and the entry title.
 * Licence: the feed states "Copyright 2026 MeteoAlarm.Org. Licensed under
 *   terms equivalent to CC BY 4.0, with additional requirements for
 *   redistributing outlined in our Terms and Conditions." Those extra
 *   conditions are: attribution to MeteoAlarm, NO ALTERATION of the warning
 *   text, and a link back to meteoalarm.org — the warning text is therefore
 *   carried verbatim and meteoalarm.org is carried as the link on every row.
 *
 * REJECTED PATHS (probed, from parse_notes): /feeds/meteoalarm-legacy-atom-europe
 * and /feeds/meteoalarm-legacy-rss-europe 404 or return a deprecation stub with
 * zero items; /api/v1/warnings/feeds-europe 404s. Only the per-country path
 * works.
 *
 * GEOMETRY WARNING: despite being a geo hazard feed there are NO coordinates
 * and NO polygon — only <cap:geocode> valueName EMMA_ID with a region code
 * ("IT010") plus <cap:areaDesc> ("Umbria"). Every row is emitted with
 * has_geo=0. Guessing a region centroid is exactly the invented geometry R2
 * forbids.
 *
 * One <entry> exists per region per warning, so uid is identifier|EMMA_ID:
 * that keeps the per-region granularity while making repeated polls idempotent.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "geoeo_common.inc"
#include <strings.h>

#define MA_BASE "https://feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-"

/* The verified feed first; the rest are best-effort siblings. */
static const char *MA_COUNTRIES[] = {
  "italy", "france", "germany", "spain", "greece", "poland", "norway",
  "sweden", "austria", "portugal", "netherlands", "ireland", NULL
};

/* EMMA_ID lives in <cap:geocode><valueName>EMMA_ID</valueName><value>IT010</value>. */
static int entry_emma(const char *blk, char *out, size_t n) {
  out[0] = 0;
  const char *c = blk, *inner = NULL;
  int len = 0;
  while ((c = html_block(c, "geocode", &inner, &len)) != NULL) {
    if (len <= 0) continue;
    char *gb = geoeo_dupn(inner, (size_t)len);
    if (!gb) return 0;
    char vn[64], vv[96];
    if (geoeo_xml_text(gb, "valueName", vn, sizeof vn) &&
        strcasecmp(vn, "EMMA_ID") == 0 &&
        geoeo_xml_text(gb, "value", vv, sizeof vv)) {
      snprintf(out, n, "%s", vv);
      free(gb);
      return 1;
    }
    free(gb);
  }
  return 0;
}

static int ma_country(const source_ctx *ctx, intel_sink *sink,
                      const char *country, int *emitted) {
  char url[256];
  snprintf(url, sizeof url, "%s%s", MA_BASE, country);
  char *body = feed_get_text(ctx->http, url, 30000);
  if (!body) return -1;

  int entries = 0;
  const char *cur = body, *inner = NULL;
  int ilen = 0;
  while ((cur = html_block(cur, "entry", &inner, &ilen)) != NULL) {
    if (ilen <= 0) continue;
    char *blk = geoeo_dupn(inner, (size_t)ilen);
    if (!blk) break;
    entries++;

    char title[512], ident[256], event[128], sev[64], cert[64], urg[64];
    char area[256], onset[64], expires[64], emma[64];
    int has_title = geoeo_xml_text(blk, "title", title, sizeof title);
    geoeo_xml_text(blk, "identifier", ident, sizeof ident);
    geoeo_xml_text(blk, "event", event, sizeof event);
    geoeo_xml_text(blk, "severity", sev, sizeof sev);
    geoeo_xml_text(blk, "certainty", cert, sizeof cert);
    geoeo_xml_text(blk, "urgency", urg, sizeof urg);
    geoeo_xml_text(blk, "areaDesc", area, sizeof area);
    geoeo_xml_text(blk, "onset", onset, sizeof onset);
    geoeo_xml_text(blk, "expires", expires, sizeof expires);
    entry_emma(blk, emma, sizeof emma);
    if (!has_title && !ident[0]) { free(blk); continue; }

    cJSON *props = cJSON_CreateObject();
    geoeo_add_str(props, "title", title);
    geoeo_add_str(props, "cap_identifier", ident);
    geoeo_add_str(props, "cap_event", event);
    geoeo_add_str(props, "cap_severity", sev);
    geoeo_add_str(props, "cap_certainty", cert);
    geoeo_add_str(props, "cap_urgency", urg);
    geoeo_add_str(props, "cap_areaDesc", area);
    geoeo_add_str(props, "cap_onset", onset);
    geoeo_add_str(props, "cap_expires", expires);
    geoeo_add_str(props, "emma_id", emma);
    cJSON_AddStringToObject(props, "country_feed", country);
    cJSON_AddStringToObject(props, "attribution",
                            "MeteoAlarm.org / EUMETNET member national met "
                            "service — warning text unaltered");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char key[384];
    snprintf(key, sizeof key, "%s|%s",
             ident[0] ? ident : title, emma[0] ? emma : area);

    char summary[384];
    snprintf(summary, sizeof summary, "%s%s%s%s%s", sev[0] ? sev : "",
             (sev[0] && area[0]) ? " · " : "", area[0] ? area : "",
             onset[0] ? " · from " : "", onset[0] ? onset : "");

    intel_item it = {0};
    it.remote_key = key;
    it.title = has_title ? title : ident;
    it.summary = summary[0] ? summary : NULL;
    it.link = "https://meteoalarm.org/";     /* licence: link back required */
    it.lang = "en";
    it.published_at = onset[0] ? onset : NULL;
    it.record_type = "weather-warning";
    it.has_geo = 0;                          /* EMMA_ID only — no coordinate */
    it.geometry_geojson = NULL;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"weather\",\"warning\",\"cap\",\"meteoalarm\",\"europe\"]";
    if (sink->emit(sink, &it) >= 0) (*emitted)++;

    free(pj);
    free(blk);
  }
  free(body);
  return entries;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0;
  int primary = ma_country(ctx, sink, MA_COUNTRIES[0], &n);
  if (primary < 0) {
    fprintf(stderr, "[meteoalarm-cap] %s feed fetch failed\n", MA_COUNTRIES[0]);
    return -1;
  }
  for (int i = 1; MA_COUNTRIES[i]; i++)
    ma_country(ctx, sink, MA_COUNTRIES[i], &n);   /* best-effort siblings */

  fprintf(stderr, "[meteoalarm-cap] emitted %d warning/region rows\n", n);
  return 0;                                  /* calm weather → 0 is honest */
}

static const source_def geoeo_meteoalarm_def = {
  .id = "meteoalarm-cap", .collector = "environment",
  .name = "MeteoAlarm European Weather Warnings (CAP Atom)",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-italy",
  .description = "The pan-European severe weather warning aggregator — every EUMETNET member's national met service warnings normalised into CAP, per country feed.",
  .license = "MeteoAlarm.Org, terms equivalent to CC BY 4.0 with extra redistribution conditions: attribution, no alteration of warning text, link back to meteoalarm.org.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_meteoalarm_def)
