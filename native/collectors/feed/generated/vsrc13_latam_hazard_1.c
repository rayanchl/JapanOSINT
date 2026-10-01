/* Verified-live latam_hazard sources (13), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VRSS(lat_cemaden_rss, "lat-cemaden-rss", "CEMADEN - natural disaster monitoring bulletin", "ブラジル：防災 - CEMADEN - natural disaster monitoring bulletin",
  "latam_hazard", "hazard",
  "http://www2.cemaden.gov.br/feed/",
  "pt", "[\"bra\",\"hazard\",\"hazard\"]", 1800,
  "Brazil's national disaster monitoring and early warning centre publishing situation notes and alerts.");

VJSON(lat_cl_gael_clima, "lat-cl-gael-clima", "Chile - current weather by station", "チリ：防災 - Chile - current weather by station",
  "latam_hazard", "hazard",
  "https://api.gael.cloud/general/public/clima",
  "",
  "es", "[\"chl\",\"hazard\",\"hazard\"]", 1800,
  "Current observed conditions at Chilean weather stations: temperature, humidity, wind and pressure.");

VJSON(lat_cl_xor_sismo, "lat-cl-xor-sismo", "Chile - recent earthquakes (sismologia mirror)", "チリ：防災 - Chile - recent earthquakes (sismologia mirror)",
  "latam_hazard", "hazard",
  "https://api.xor.cl/sismo/recent",
  "events",
  "es", "[\"chl\",\"hazard\",\"hazard\"]", 900,
  "Recent Chilean earthquakes with origin time, magnitude, depth and epicentre coordinates.");

VJSON(lat_co_ideam_precip, "lat-co-ideam-precip", "Datos Abiertos Colombia - IDEAM precipitation observations", "コロンビア：防災 - Datos Abiertos Colombia - IDEAM precipitation observations",
  "latam_hazard", "hazard",
  "https://www.datos.gov.co/resource/s54a-sgyg.json?$limit=500&$order=:id",
  "",
  "es", "[\"col\",\"hazard\",\"hazard\"]", 86400,
  "Station-level rainfall observations from the IDEAM network, with station id, date and measured value.");

VJSON(lat_co_ideam_presion, "lat-co-ideam-presion", "Datos Abiertos Colombia - IDEAM atmospheric pressure observations", "コロンビア：防災 - Datos Abiertos Colombia - IDEAM atmospheric pressure observations",
  "latam_hazard", "hazard",
  "https://www.datos.gov.co/resource/62tk-nxj5.json?$limit=500&$order=:id",
  "",
  "es", "[\"col\",\"hazard\",\"hazard\"]", 86400,
  "Station-level barometric pressure readings from the IDEAM network.");

VJSON(lat_co_ideam_temp, "lat-co-ideam-temp", "Datos Abiertos Colombia - IDEAM air temperature observations", "コロンビア：防災 - Datos Abiertos Colombia - IDEAM air temperature observations",
  "latam_hazard", "hazard",
  "https://www.datos.gov.co/resource/sbwg-7ju4.json?$limit=500&$order=:id",
  "",
  "es", "[\"col\",\"hazard\",\"hazard\"]", 86400,
  "Station-level ambient air temperature readings from the IDEAM network.");

#include "lib/arcgis_dir.h"

/* lat-geo-arcgis-sgc / -snirh: ArcGIS REST services directories. The old rows
 * pointed the JSON emitter at "folders" (bare folder-name strings: SGC 53, SNIRH
 * 26 on 2026-09-14) and emitted nothing on every run. lib/arcgis_dir.c walks
 * each folder listing and emits the services themselves. */
static int run_lat_geo_arcgis_sgc(const source_ctx *c, intel_sink *s) {
  return arcgis_dir_walk(c, s, "lat-geo-arcgis-sgc",
                         "https://srvags.sgc.gov.co/arcgis/rest/services",
                         "geo", "es", "[\"col\",\"geo\",\"hazard\"]");
}
static const source_def lat_geo_arcgis_sgc = {
  .id = "lat-geo-arcgis-sgc", .collector = "latam_hazard",
  .name = "Servicio Geologico Colombiano - ArcGIS service directory",
  .name_ja = "コロンビア：地理空間 - Servicio Geologico Colombiano - ArcGIS service directory",
  .update_interval_sec = 86400, .run = run_lat_geo_arcgis_sgc,
  .category = "geo", .type = "api",
  .url = "https://srvags.sgc.gov.co/arcgis/rest/services?f=json",
  .description = "Directory of SGC ArcGIS map and feature services covering Colombian seismicity, volcanoes and geohazards.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(lat_geo_arcgis_sgc);

static int run_lat_geo_arcgis_snirh(const source_ctx *c, intel_sink *s) {
  return arcgis_dir_walk(c, s, "lat-geo-arcgis-snirh",
                         "https://www.snirh.gov.br/arcgis/rest/services",
                         "geo", "pt", "[\"bra\",\"geo\",\"hazard\"]");
}
static const source_def lat_geo_arcgis_snirh = {
  .id = "lat-geo-arcgis-snirh", .collector = "latam_hazard",
  .name = "ANA Brazil - SNIRH ArcGIS service directory",
  .name_ja = "ブラジル：地理空間 - ANA Brazil - SNIRH ArcGIS service directory",
  .update_interval_sec = 86400, .run = run_lat_geo_arcgis_snirh,
  .category = "geo", .type = "api",
  .url = "https://www.snirh.gov.br/arcgis/rest/services?f=json",
  .description = "Directory of Brazilian National Water Agency ArcGIS services for hydrology, reservoirs and drought.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(lat_geo_arcgis_snirh);

VRSS(lat_insivumeh_gt, "lat-insivumeh-gt", "INSIVUMEH Guatemala - volcano and weather bulletins", "グアテマラ：防災 - INSIVUMEH Guatemala - volcano and weather bulletins",
  "latam_hazard", "hazard",
  "https://insivumeh.gob.gt/feed/",
  "es", "[\"gtm\",\"hazard\",\"hazard\"]", 1800,
  "Guatemala's volcanology, meteorology and hydrology institute: volcanic activity and weather bulletins.");

VRSS(lat_pa_sinaproc_feed, "lat-pa-sinaproc-feed", "SINAPROC Panama - civil protection bulletin", "パナマ：防災 - SINAPROC Panama - civil protection bulletin",
  "latam_hazard", "hazard",
  "https://www.sinaproc.gob.pa/feed/",
  "es", "[\"pan\",\"hazard\",\"hazard\"]", 1800,
  "RSS bulletin from Panama's national civil protection system.");

VJSON(lat_wp_conred_gt, "lat-wp-conred-gt", "CONRED Guatemala - disaster agency posts", "グアテマラ：防災 - CONRED Guatemala - disaster agency posts",
  "latam_hazard", "hazard",
  "https://conred.gob.gt/wp-json/wp/v2/posts?per_page=50",
  "",
  "es", "[\"gtm\",\"hazard\",\"hazard\"]", 1800,
  "Guatemala's national disaster coordination agency: alerts, evacuations and incident reports.");

VJSON(lat_wp_odpm_tt, "lat-wp-odpm-tt", "ODPM Trinidad and Tobago - disaster management posts", "トリニダード・トバゴ：防災 - ODPM Trinidad and Tobago - disaster management posts",
  "latam_hazard", "hazard",
  "https://odpm.gov.tt/wp-json/wp/v2/posts?per_page=50",
  "",
  "en", "[\"tto\",\"hazard\",\"hazard\"]", 1800,
  "Trinidad and Tobago's Office of Disaster Preparedness and Management: alerts and advisories.");

VJSON(lat_wp_sinaproc_pa, "lat-wp-sinaproc-pa", "SINAPROC Panama - civil protection posts", "パナマ：防災 - SINAPROC Panama - civil protection posts",
  "latam_hazard", "hazard",
  "https://www.sinaproc.gob.pa/wp-json/wp/v2/posts?per_page=50",
  "",
  "es", "[\"pan\",\"hazard\",\"hazard\"]", 1800,
  "Panama's civil protection system: weather warnings, incidents and response operations.");
