/* Verified-live environment sources (12), part 3.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

/* geo-tidesandcurrents-currents: NOAA's station registry keys current-
 * prediction points by "id" (the physical station), but a single station
 * commonly reports current predictions at several depths/bins under one
 * "currbin" per record — e.g. "ACT0311" appears twice, currbin 1 (depth
 * 4.57m) and currbin 2 (depth 12.19m), two genuinely distinct prediction
 * points sharing one "id". jsonlist's default id precedence picks up "id"
 * alone and collapses them; live-verified 2026-09-04: 4430 records, only
 * 2785 distinct "id" values, matching the measured 1645-record loss exactly.
 * jsonlist_emit_paged_keyed only supports a single id field, not a composite
 * one, so this hand-rolls the same paged-emit-with-relabelled-id pattern
 * (see lib/jsonlist.c jl_emit_page_keyed) but relabels "id" to "id_currbin"
 * (both values already present on the record — nothing invented) before
 * handing off to the shared jsonlist_emit_ex, keeping paging, disclosure,
 * title/geo derivation and the collision guard all unchanged. */
typedef struct { const char *path, *record_type, *lang, *tags_json; } tc_composite_opts;

static int tc_emit_page_composite(const source_ctx *c, intel_sink *s,
                                  const char *id, cJSON *doc, void *ud,
                                  int *seen) {
  (void)c;
  tc_composite_opts *o = (tc_composite_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  if (arr) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (!cJSON_IsObject(rec)) continue;
      cJSON *sid = cJSON_GetObjectItemCaseSensitive(rec, "id");
      cJSON *bin = cJSON_GetObjectItemCaseSensitive(rec, "currbin");
      if (!cJSON_IsString(sid) || !sid->valuestring) continue;
      char buf[160];
      if (bin && cJSON_IsNumber(bin))
        snprintf(buf, sizeof buf, "%s_%d", sid->valuestring, (int)bin->valuedouble);
      else
        snprintf(buf, sizeof buf, "%s", sid->valuestring);
      cJSON_DeleteItemFromObjectCaseSensitive(rec, "id");
      cJSON_AddStringToObject(rec, "id", buf);
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->record_type, o->lang,
                          o->tags_json, seen);
}

static int run_geo_tidesandcurrents_currents(const source_ctx *c, intel_sink *s) {
  tc_composite_opts o = { "stations", "ocean", "en",
                          "[\"usa\",\"currents\",\"noaa\",\"stations\"]" };
  int n = pw_walk(c, s, "geo-tidesandcurrents-currents",
                  "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations.json?type=currentpredictions&units=metric",
                  pw_fetch_json, tc_emit_page_composite, &o);
  if (n < 0) {
    fprintf(stderr, "[geo-tidesandcurrents-currents] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def geo_tidesandcurrents_currents = {
  .id = "geo-tidesandcurrents-currents", .collector = "environment",
  .name = "NOAA CO-OPS — current prediction stations",
  .name_ja = "NOAA CO-OPS — 潮流予測地点",
  .update_interval_sec = 86400, .run = run_geo_tidesandcurrents_currents,
  .category = "ocean", .type = "api",
  .url = "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations.json?type=currentpredictions&units=metric",
  .description = "US tidal current prediction station registry with coordinates.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(geo_tidesandcurrents_currents);

VJSON(geo_smhi_ocobs_sealevel, "geo-smhi-ocobs-sealevel", "SMHI Sweden — sea level stations", "スウェーデンSMHI — 潮位観測所",
  "environment", "ocean",
  "https://opendata-download-ocobs.smhi.se/api/version/1.0/parameter/2.json",
  "station",
  "en", "[\"sweden\",\"sealevel\",\"tidegauge\"]", 21600,
  "Swedish sea level gauge network.");

VJSON(geo_tidesandcurrents_met, "geo-tidesandcurrents-met", "NOAA CO-OPS — meteorological stations", "NOAA CO-OPS — 気象観測地点",
  "environment", "ocean",
  "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations.json?type=met&units=metric",
  "stations",
  "en", "[\"usa\",\"coastal\",\"meteorology\"]", 86400,
  "US coastal met stations co-located with tide gauges.");

VJSON(geo_tidesandcurrents_watertemp, "geo-tidesandcurrents-watertemp", "NOAA CO-OPS — water temperature stations", "NOAA CO-OPS — 水温観測地点",
  "environment", "ocean",
  "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations.json?type=watertemp&units=metric",
  "stations",
  "en", "[\"usa\",\"watertemperature\",\"noaa\"]", 86400,
  "US stations reporting water temperature.");

VGEO(geo_usda_drought_arcgis, "geo-usda-drought-arcgis", "US Drought Monitor intensity polygons", "米国干ばつモニター 強度域",
  "environment", "weather",
  "https://services9.arcgis.com/RHVPKKiFTONKtxq3/arcgis/rest/services/US_Drought_Intensity_v1/FeatureServer/3/query?where=1%3D1&outFields=*&f=geojson&resultRecordCount=500",
  "en", "[\"drought\",\"usa\",\"monitor\"]", 86400,
  "Weekly US Drought Monitor intensity class polygons.");

VGEO(geo_usgs_ogc_monitoring, "geo-usgs-ogc-monitoring", "USGS Water — monitoring location registry", "USGS水文 — 観測地点登録",
  "environment", "flood",
  "https://api.waterdata.usgs.gov/ogcapi/v0/collections/monitoring-locations/items?f=json&limit=500",
  "en", "[\"usa\",\"usgs\",\"stations\"]", 86400,
  "Full USGS monitoring location registry; this endpoint returns attributes without geometry.");

VJSON(geo_usgs_water_collections, "geo-usgs-water-collections", "USGS Water OGC API — collection catalogue", "USGS水文 OGC API — コレクション目録",
  "environment", "flood",
  "https://api.waterdata.usgs.gov/ogcapi/v0/collections?f=json",
  "collections",
  "en", "[\"usa\",\"usgs\",\"ogcapi\",\"catalog\"]", 86400,
  "Catalogue of USGS water data OGC API collections.");

VGEO(geo_usgs_water_daily, "geo-usgs-water-daily", "USGS Water — daily values", "USGS水文 — 日値",
  "environment", "flood",
  "https://api.waterdata.usgs.gov/ogcapi/v0/collections/daily/items?f=json&limit=300",
  "en", "[\"usa\",\"usgs\",\"hydrology\"]", 21600,
  "Daily discharge and stage values from USGS gauges.");

VGEO(geo_usgs_water_field_meas, "geo-usgs-water-field-meas", "USGS Water — latest field measurements", "USGS水文 — 最新現地観測",
  "environment", "flood",
  "https://api.waterdata.usgs.gov/ogcapi/v0/collections/latest-field-measurements/items?f=json&limit=300",
  "en", "[\"usa\",\"usgs\",\"hydrology\",\"field\"]", 86400,
  "Manual field discharge measurements used to rate gauges.");

VGEO(geo_usgs_water_latest_continuous, "geo-usgs-water-latest-continuous", "USGS Water — latest continuous readings", "USGS水文 — 最新連続観測",
  "environment", "flood",
  "https://api.waterdata.usgs.gov/ogcapi/v0/collections/latest-continuous/items?f=json&limit=500",
  "en", "[\"usa\",\"usgs\",\"hydrology\",\"realtime\"]", 900,
  "Latest continuous gauge values across the US national water network.");

VGEO(geo_usgs_water_latest_daily, "geo-usgs-water-latest-daily", "USGS Water — latest daily values", "USGS水文 — 最新日値",
  "environment", "flood",
  "https://api.waterdata.usgs.gov/ogcapi/v0/collections/latest-daily/items?f=json&limit=500",
  "en", "[\"usa\",\"usgs\",\"hydrology\",\"daily\"]", 21600,
  "Most recent daily statistics per USGS monitoring location.");

VGEO(geo_usgs_water_timeseries_meta, "geo-usgs-water-timeseries-meta", "USGS Water — time series metadata", "USGS水文 — 時系列メタデータ",
  "environment", "flood",
  "https://api.waterdata.usgs.gov/ogcapi/v0/collections/time-series-metadata/items?f=json&limit=300",
  "en", "[\"usa\",\"usgs\",\"metadata\"]", 86400,
  "Which parameters each USGS site currently reports.");
