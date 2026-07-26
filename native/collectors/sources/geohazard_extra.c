/* Additional real-time geohazard feeds (seismic, cyclone, volcano, weather, disaster) — real RSS, via rss_collect. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

#define RSSX(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, LANG, TAGS, IVAL, DESC)  \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    int n = rss_collect(c, s, URL, LANG, TAGS); return n < 0 ? -1 : 0; }     \
  static const source_def SYM = {                                           \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,            \
    .update_interval_sec = IVAL, .run = run_##SYM,                           \
    .category = CAT, .type = "web_request", .url = URL,                      \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

RSSX(geo_emsc_quakes, "emsc-quakes", "EMSC Latest Earthquakes", "EMSC Latest Earthquakes", "environment", "environment",
  "https://www.emsc-csem.org/service/rss/rss.php?typ=emsc", "en", "[\"hazard\",\"seismic\",\"alert\"]", 900,
  "EMSC Latest Earthquakes — real-time seismic hazard feed");

RSSX(geo_nhc_cpac, "nhc-cpac", "NOAA NHC Central Pacific", "NOAA NHC Central Pacific", "environment", "environment",
  "https://www.nhc.noaa.gov/index-cp.xml", "en", "[\"hazard\",\"cyclone\",\"alert\"]", 900,
  "NOAA NHC Central Pacific — real-time cyclone hazard feed");

RSSX(geo_volcanodiscovery, "volcanodiscovery", "VolcanoDiscovery", "VolcanoDiscovery", "environment", "environment",
  "https://www.volcanodiscovery.com/rss.xml", "en", "[\"hazard\",\"volcano\",\"alert\"]", 900,
  "VolcanoDiscovery — real-time volcano hazard feed");

RSSX(geo_bom_au_warnings, "bom-au-warnings", "Australia BoM Warnings", "Australia BoM Warnings", "environment", "environment",
  "http://www.bom.gov.au/rss/1.xml", "en", "[\"hazard\",\"weather\",\"alert\"]", 900,
  "Australia BoM Warnings — real-time weather hazard feed");

RSSX(geo_metoffice_warnings, "metoffice-warnings", "UK Met Office Warnings", "UK Met Office Warnings", "environment", "environment",
  "https://www.metoffice.gov.uk/public/data/PWSCache/WarningsRSS/Region/UK", "en", "[\"hazard\",\"weather\",\"alert\"]", 900,
  "UK Met Office Warnings — real-time weather hazard feed");

RSSX(geo_iceland_quakes, "iceland-quakes", "Iceland Met Office Quakes", "Iceland Met Office Quakes", "environment", "environment",
  "https://en.vedur.is/earthquakes-and-volcanism/earthquakes/rss/", "en", "[\"hazard\",\"seismic\",\"alert\"]", 900,
  "Iceland Met Office Quakes — real-time seismic hazard feed");

RSSX(geo_copernicus_ems, "copernicus-ems", "Copernicus Emergency Mgmt", "Copernicus Emergency Mgmt", "environment", "environment",
  "https://emergency.copernicus.eu/mapping/activations-rapid/rss.xml", "en", "[\"hazard\",\"disaster\",\"alert\"]", 900,
  "Copernicus Emergency Mgmt — real-time disaster hazard feed");

RSSX(geo_pdc_disasters, "pdc-disasters", "Pacific Disaster Center", "Pacific Disaster Center", "environment", "environment",
  "https://www.pdc.org/feed/", "en", "[\"hazard\",\"disaster\",\"alert\"]", 900,
  "Pacific Disaster Center — real-time disaster hazard feed");
