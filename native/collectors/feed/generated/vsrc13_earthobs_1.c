/* Verified-live earthobs sources (60), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(sci_bio_checklistbank, "sci-bio-checklistbank", "ChecklistBank — Catalogue of Life name search", "ChecklistBank — Catalogue of Life name search",
  "earthobs", "environment",
  "https://api.checklistbank.org/dataset/3LR/nameusage/search?limit=100",
  "result",
  "en", "[\"int\",\"biodiversity\",\"taxonomy\"]", 86400,
  "Name usages from the Catalogue of Life with accepted name, classification and source dataset.");

VJSON(sci_bio_gbif_dataset_search, "sci-bio-gbif-dataset-search", "GBIF — dataset search", "GBIF — dataset search",
  "earthobs", "environment",
  "https://api.gbif.org/v1/dataset/search?limit=100",
  "results",
  "en", "[\"int\",\"biodiversity\",\"dataset\",\"catalog\"]", 86400,
  "GBIF-registered datasets with publisher, licence, record counts and endpoints.");

VJSON(sci_bio_gbif_installation, "sci-bio-gbif-installation", "GBIF — data installation registry", "GBIF — data installation registry",
  "earthobs", "environment",
  "https://api.gbif.org/v1/installation?limit=100",
  "results",
  "en", "[\"int\",\"biodiversity\",\"registry\"]", 93600,
  "IPT and other installations serving data to GBIF, with hosting organisation and endpoints.");

VJSON(sci_bio_gbif_network, "sci-bio-gbif-network", "GBIF — thematic network registry", "GBIF — thematic network registry",
  "earthobs", "environment",
  "https://api.gbif.org/v1/network?limit=100",
  "results",
  "en", "[\"int\",\"biodiversity\",\"registry\"]", 91800,
  "Thematic GBIF networks and the datasets each aggregates.");

VJSON(sci_bio_gbif_node, "sci-bio-gbif-node", "GBIF — participant node registry", "GBIF — participant node registry",
  "earthobs", "environment",
  "https://api.gbif.org/v1/node?limit=100",
  "results",
  "en", "[\"int\",\"biodiversity\",\"registry\"]", 90000,
  "GBIF participant nodes by country with membership type and governance role.");

VJSON(sci_bio_gbif_occ_europe, "sci-bio-gbif-occ-europe", "GBIF — georeferenced occurrences in Europe", "GBIF — georeferenced occurrences in Europe",
  "earthobs", "environment",
  "https://api.gbif.org/v1/occurrence/search?hasCoordinate=true&limit=300&continent=EUROPE",
  "results",
  "en", "[\"eur\",\"biodiversity\",\"coordinates\"]", 52200,
  "Recent European species occurrence records that carry decimal coordinates, with taxon, date and dataset.");

VJSON(sci_bio_gbif_occ_northamerica, "sci-bio-gbif-occ-northamerica", "GBIF — georeferenced occurrences in North America", "GBIF — georeferenced occurrences in North America",
  "earthobs", "environment",
  "https://api.gbif.org/v1/occurrence/search?hasCoordinate=true&limit=300&continent=NORTH_AMERICA",
  "results",
  "en", "[\"usa\",\"biodiversity\",\"coordinates\"]", 54000,
  "Recent North American species occurrence records with decimal coordinates, taxon and collection metadata.");

VJSON(sci_bio_gbif_organization, "sci-bio-gbif-organization", "GBIF — publishing organisation registry", "GBIF — publishing organisation registry",
  "earthobs", "environment",
  "https://api.gbif.org/v1/organization?limit=100",
  "results",
  "en", "[\"int\",\"biodiversity\",\"registry\"]", 88200,
  "Organisations publishing to GBIF with country, contacts and hosted dataset counts.");

VJSON(sci_bio_obis_checklist, "sci-bio-obis-checklist", "OBIS — marine species checklist", "OBIS — marine species checklist",
  "earthobs", "environment",
  "https://api.obis.org/v3/checklist?size=100",
  "results",
  "en", "[\"int\",\"biodiversity\",\"marine\",\"species\"]", 86400,
  "Marine taxa recorded in OBIS with record counts and taxonomic ranks.");

VJSON(sci_bio_obis_institutes, "sci-bio-obis-institutes", "OBIS — contributing institute registry", "OBIS — contributing institute registry",
  "earthobs", "environment",
  "https://api.obis.org/v3/institute",
  "results",
  "en", "[\"int\",\"biodiversity\",\"marine\",\"registry\"]", 88200,
  "Institutions supplying marine occurrence data to OBIS, with country and record volumes.");

VJSON(sci_bio_obis_nodes, "sci-bio-obis-nodes", "OBIS — regional and thematic node registry", "OBIS — regional and thematic node registry",
  "earthobs", "environment",
  "https://api.obis.org/v3/node",
  "results",
  "en", "[\"int\",\"biodiversity\",\"marine\",\"registry\"]", 90000,
  "OBIS nodes worldwide with the datasets and geographic scope each one curates.");

VJSON(sci_bio_obis_occurrence_recent, "sci-bio-obis-occurrence-recent", "OBIS — marine occurrence records", "OBIS — marine occurrence records",
  "earthobs", "environment",
  "https://api.obis.org/v3/occurrence?size=200&areaid=1",
  "results",
  "en", "[\"int\",\"biodiversity\",\"marine\",\"coordinates\"]", 48600,
  "Georeferenced marine species occurrence records with decimal coordinates, depth, date and dataset provenance.");

VJSON(sci_bio_worms_aphia, "sci-bio-worms-aphia", "WoRMS — marine taxon name resolution", "WoRMS — marine taxon name resolution",
  "earthobs", "environment",
  "https://www.marinespecies.org/rest/AphiaRecordsByName/Gadus?like=true&marine_only=true&offset=1",
  "",
  "en", "[\"int\",\"biodiversity\",\"marine\",\"taxonomy\"]", 86400,
  "World Register of Marine Species records with accepted names, authority, rank and status.");

VJSON(sci_fdsn_datacenters, "sci-fdsn-datacenters", "FDSN — registry of seismic data centres", "FDSN — registry of seismic data centres",
  "earthobs", "seismic",
  "https://www.fdsn.org/ws/datacenters/1/query",
  "datacenters",
  "en", "[\"int\",\"seismic\",\"registry\",\"fdsn\"]", 86400,
  "Authoritative list of every FDSN-registered seismological data centre and the web services each one exposes.");

VJSON(sci_fdsn_networks, "sci-fdsn-networks", "FDSN — registry of seismic networks", "FDSN — registry of seismic networks",
  "earthobs", "seismic",
  "https://www.fdsn.org/ws/networks/1/query",
  "networks",
  "en", "[\"int\",\"seismic\",\"registry\",\"fdsn\"]", 88200,
  "Every registered seismic network code worldwide with operator, DOI and operating period — the lookup table behind station metadata.");

VGEO(sci_fdsn_resif_event, "sci-fdsn-resif-event", "RESIF-EPOS France — FDSN earthquake catalogue (GeoJSON)", "RESIF-EPOS France — FDSN earthquake catalogue (GeoJSON)",
  "earthobs", "hazard",
  "https://ws.resif.fr/fdsnws/event/1/query?format=json&limit=200&orderby=time",
  "en", "[\"fra\",\"earthquake\",\"seismic\",\"fdsn\",\"geojson\"]", 1800,
  "French national seismic network FDSN event service returning the most recent located events as GeoJSON points.");

VRSS(sci_feed_bgs_news, "sci-feed-bgs-news", "British Geological Survey — news", "British Geological Survey — news",
  "earthobs", "science",
  "https://www.bgs.ac.uk/feed/",
  "en", "[\"gbr\",\"geology\",\"science\"]", 3600,
  "Headline feed from British Geological Survey; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_carbonbrief, "sci-feed-carbonbrief", "Carbon Brief — climate science and policy", "Carbon Brief — climate science and policy",
  "earthobs", "environment",
  "https://www.carbonbrief.org/feed/",
  "en", "[\"gbr\",\"climate\",\"policy\"]", 3600,
  "Headline feed from Carbon Brief; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_eo_hazards, "sci-feed-eo-hazards", "NASA Earth Observatory — Natural Hazards", "NASA Earth Observatory — Natural Hazards",
  "earthobs", "hazard",
  "https://earthobservatory.nasa.gov/feeds/natural-hazards.rss",
  "en", "[\"usa\",\"hazard\",\"satellite\"]", 3600,
  "Headline feed from NASA Earth Observatory; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_eo_iotd, "sci-feed-eo-iotd", "NASA Earth Observatory — Image of the Day", "NASA Earth Observatory — Image of the Day",
  "earthobs", "environment",
  "https://earthobservatory.nasa.gov/feeds/image-of-the-day.rss",
  "en", "[\"usa\",\"satellite\",\"imagery\"]", 5400,
  "Headline feed from NASA Earth Observatory; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_eumetsat, "sci-feed-eumetsat", "EUMETSAT — news", "EUMETSAT — news",
  "earthobs", "environment",
  "https://www.eumetsat.int/rss.xml",
  "en", "[\"eur\",\"satellite\",\"weather\"]", 3600,
  "Headline feed from EUMETSAT; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_ipcc, "sci-feed-ipcc", "IPCC — news", "IPCC — news",
  "earthobs", "environment",
  "https://www.ipcc.ch/feed/",
  "en", "[\"int\",\"climate\",\"policy\"]", 3600,
  "Headline feed from IPCC; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_nature_climate, "sci-feed-nature-climate", "Nature Climate Change — table of contents", "Nature Climate Change — table of contents",
  "earthobs", "science",
  "https://www.nature.com/nclimate.rss",
  "en", "[\"int\",\"science\",\"climate\"]", 3600,
  "Headline feed from Nature Climate Change; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_nature_geoscience, "sci-feed-nature-geoscience", "Nature Geoscience — table of contents", "Nature Geoscience — table of contents",
  "earthobs", "science",
  "https://www.nature.com/ngeo.rss",
  "en", "[\"int\",\"science\",\"geology\"]", 5400,
  "Headline feed from Nature Geoscience; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_nsf_news, "sci-feed-nsf-news", "US National Science Foundation — news", "US National Science Foundation — news",
  "earthobs", "science",
  "https://www.nsf.gov/rss/rss_www_news.xml",
  "en", "[\"usa\",\"science\"]", 3600,
  "Headline feed from US National Science Foundation; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_phys_earth, "sci-feed-phys-earth", "Phys.org — Earth science news", "Phys.org — Earth science news",
  "earthobs", "science",
  "https://phys.org/rss-feed/earth-news/",
  "en", "[\"int\",\"science\",\"earth\"]", 3600,
  "Headline feed from Phys.org; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_sciencedaily_earth, "sci-feed-sciencedaily-earth", "ScienceDaily — Earth and Climate", "ScienceDaily — Earth and Climate",
  "earthobs", "science",
  "https://www.sciencedaily.com/rss/earth_climate.xml",
  "en", "[\"int\",\"science\",\"climate\"]", 3600,
  "Headline feed from ScienceDaily; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_sciencedaily_geology, "sci-feed-sciencedaily-geology", "ScienceDaily — Geology", "ScienceDaily — Geology",
  "earthobs", "science",
  "https://www.sciencedaily.com/rss/earth_climate/geology.xml",
  "en", "[\"int\",\"science\",\"geology\"]", 7200,
  "Headline feed from ScienceDaily; used to catch new findings, missions and hazard reporting as they are published.");

VRSS(sci_feed_volcanocafe, "sci-feed-volcanocafe", "VolcanoCafe — volcanology commentary", "VolcanoCafe — volcanology commentary",
  "earthobs", "hazard",
  "https://www.volcanocafe.org/feed/",
  "en", "[\"int\",\"volcano\",\"hazard\"]", 3600,
  "Headline feed from VolcanoCafe; used to catch new findings, missions and hazard reporting as they are published.");

/* The three sci-hub-arcgis-* rows below (and sci-hub-arcgis-airquality in
 * vsrc13_climate_air_1.c) key correctly — the JSON:API record carries a unique
 * top-level "id". What collided was the PAGE WALK: with no sort given the Hub
 * returns relevance order, whose ties are broken differently on each request,
 * so page N+1 re-serves rows of page N and the rows they displaced are never
 * fetched. Live-verified 2026-09-07 on q=volcano, page[size]=50: pages 8 and 9
 * share 3 dataset ids unsorted and 0 with `sort=created`, which the API accepts
 * and which is a total order over the catalogue. */
VJSON(sci_hub_arcgis_earthquake, "sci-hub-arcgis-earthquake", "ArcGIS Hub — earthquake dataset search", "ArcGIS Hub — earthquake dataset search",
  "earthobs", "geodata",
  "https://hub.arcgis.com/api/v3/datasets?q=earthquake&page%5Bsize%5D=50&sort=created",
  "data",
  "en", "[\"int\",\"earthquake\",\"arcgis\",\"catalog\"]", 43200,
  "Open ArcGIS Hub datasets tagged earthquake across thousands of publishing organisations, each with a live service URL.");

VJSON(sci_hub_arcgis_landslide, "sci-hub-arcgis-landslide", "ArcGIS Hub — landslide dataset search", "ArcGIS Hub — landslide dataset search",
  "earthobs", "geodata",
  "https://hub.arcgis.com/api/v3/datasets?q=landslide&page%5Bsize%5D=50&sort=created",
  "data",
  "en", "[\"int\",\"landslide\",\"arcgis\",\"catalog\"]", 52200,
  "Open ArcGIS Hub landslide inventory and susceptibility datasets with service URLs.");

VJSON(sci_hub_arcgis_volcano, "sci-hub-arcgis-volcano", "ArcGIS Hub — volcano dataset search", "ArcGIS Hub — volcano dataset search",
  "earthobs", "geodata",
  "https://hub.arcgis.com/api/v3/datasets?q=volcano&page%5Bsize%5D=50&sort=created",
  "data",
  "en", "[\"int\",\"volcano\",\"arcgis\",\"catalog\"]", 45000,
  "Open ArcGIS Hub datasets about volcanoes, with publisher, extent and queryable FeatureServer endpoints.");

VGEO(sci_macrostrat_columns_bare, "sci-macrostrat-columns-bare", "Macrostrat — geologic column polygons (GeoJSON)", "Macrostrat — geologic column polygons (GeoJSON)",
  "earthobs", "geology",
  "https://macrostrat.org/api/v2/columns?all&format=geojson_bare",
  "en", "[\"int\",\"geology\",\"stratigraphy\",\"geojson\"]", 88200,
  "Every Macrostrat stratigraphic column as a polygon feature with column name, group and area — a real geology map layer.");

VJSON(sci_macrostrat_econs, "sci-macrostrat-econs", "Macrostrat — economic resource classes", "Macrostrat — economic resource classes",
  "earthobs", "geology",
  "https://macrostrat.org/api/v2/defs/econs?all",
  "success.data",
  "en", "[\"int\",\"geology\",\"mining\",\"reference\"]", 91800,
  "Economic-resource classification (hydrocarbon, coal, metal, aggregate) attached to Macrostrat units.");

VJSON(sci_macrostrat_environments, "sci-macrostrat-environments", "Macrostrat — depositional environment dictionary", "Macrostrat — depositional environment dictionary",
  "earthobs", "geology",
  "https://macrostrat.org/api/v2/defs/environments?all",
  "success.data",
  "en", "[\"int\",\"geology\",\"reference\"]", 90000,
  "Depositional environment classes and hierarchy used to tag Macrostrat stratigraphic units.");

VJSON(sci_macrostrat_groups, "sci-macrostrat-groups", "Macrostrat — column group registry", "Macrostrat — column group registry",
  "earthobs", "geology",
  "https://macrostrat.org/api/v2/defs/groups?all",
  "success.data",
  "en", "[\"int\",\"geology\",\"reference\"]", 86400,
  "Regional groupings of Macrostrat columns, used to slice the column set by basin or province.");

VJSON(sci_macrostrat_intervals, "sci-macrostrat-intervals", "Macrostrat — geologic time intervals", "Macrostrat — geologic time intervals",
  "earthobs", "geology",
  "https://macrostrat.org/api/v2/defs/intervals?all",
  "success.data",
  "en", "[\"int\",\"geology\",\"timescale\",\"reference\"]", 88200,
  "All named geologic intervals with age bounds and timescale membership.");

VJSON(sci_macrostrat_lithologies, "sci-macrostrat-lithologies", "Macrostrat — lithology dictionary", "Macrostrat — lithology dictionary",
  "earthobs", "geology",
  "https://macrostrat.org/api/v2/defs/lithologies?all",
  "success.data",
  "en", "[\"int\",\"geology\",\"reference\"]", 86400,
  "Controlled lithology vocabulary with class, group and colour used to interpret Macrostrat units.");

VJSON(sci_macrostrat_measurements, "sci-macrostrat-measurements", "Macrostrat — measurement type dictionary", "Macrostrat — measurement type dictionary",
  "earthobs", "geology",
  "https://macrostrat.org/api/v2/defs/measurements?all",
  "success.data",
  "en", "[\"int\",\"geology\",\"geochemistry\",\"reference\"]", 97200,
  "Geochemical and petrophysical measurement classes recorded against Macrostrat units.");

VJSON(sci_macrostrat_minerals, "sci-macrostrat-minerals", "Macrostrat — mineral dictionary", "Macrostrat — mineral dictionary",
  "earthobs", "geology",
  "https://macrostrat.org/api/v2/defs/minerals?all",
  "success.data",
  "en", "[\"int\",\"geology\",\"mineral\",\"reference\"]", 93600,
  "Mineral records with formula, hardness, crystal system and type locality.");

VJSON(sci_macrostrat_timescales, "sci-macrostrat-timescales", "Macrostrat — timescale registry", "Macrostrat — timescale registry",
  "earthobs", "geology",
  "https://macrostrat.org/api/v2/defs/timescales?all",
  "success.data",
  "en", "[\"int\",\"geology\",\"timescale\",\"reference\"]", 95400,
  "The timescales Macrostrat supports and their source references.");

VJSON(sci_ngdc_earthquakes, "sci-ngdc-earthquakes", "NOAA NCEI — significant historical earthquakes", "NOAA NCEI — significant historical earthquakes",
  "earthobs", "hazard",
  "https://www.ngdc.noaa.gov/hazel/hazard-service/api/v1/earthquakes?minYear=1900&maxYear=2000",
  "items",
  "en", "[\"int\",\"earthquake\",\"hazard\",\"history\"]", 88200,
  "Significant earthquake database entries with epicentre, magnitude, intensity and recorded deaths and damage.");

#include "lib/arcgis_dir.h"

/* sci-noaa-mapsvc-*: ArcGIS REST services DIRECTORIES. The old rows pointed the
 * JSON emitter at "folders" — bare folder-name strings — and emitted nothing.
 * lib/arcgis_dir.c walks each folder's listing and emits the services.
 * Live-verified 2026-09-14: eventdriven 7 services, raster 31, static 224,
 * vector 34, every one stored. */
typedef struct { const char *id, *base, *tags; } noaa_mapsvc_opts;

static int noaa_mapsvc_walk(const source_ctx *c, intel_sink *s,
                            const noaa_mapsvc_opts *o) {
  return arcgis_dir_walk(c, s, o->id, o->base, "geodata", "en", o->tags);
}

static int run_sci_noaa_mapsvc_eventdriven(const source_ctx *c, intel_sink *s) {
  noaa_mapsvc_opts o = { "sci-noaa-mapsvc-eventdriven",
                         "https://mapservices.weather.noaa.gov/eventdriven/rest/services",
                         "[\"usa\",\"weather\",\"hazard\",\"arcgis\"]" };
  return noaa_mapsvc_walk(c, s, &o);
}
static const source_def sci_noaa_mapsvc_eventdriven = {
  .id = "sci-noaa-mapsvc-eventdriven", .collector = "earthobs",
  .name = "NOAA NWS — event-driven map service directory",
  .name_ja = "NOAA NWS — event-driven map service directory",
  .update_interval_sec = 86400, .run = run_sci_noaa_mapsvc_eventdriven,
  .category = "geodata", .type = "api",
  .url = "https://mapservices.weather.noaa.gov/eventdriven/rest/services?f=json",
  .description = "Directory of NWS event-driven ArcGIS services (warnings, river gauges, tropical) and their layer endpoints.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(sci_noaa_mapsvc_eventdriven);

static int run_sci_noaa_mapsvc_raster(const source_ctx *c, intel_sink *s) {
  noaa_mapsvc_opts o = { "sci-noaa-mapsvc-raster",
                         "https://mapservices.weather.noaa.gov/raster/rest/services",
                         "[\"usa\",\"weather\",\"arcgis\",\"geodata\"]" };
  return noaa_mapsvc_walk(c, s, &o);
}
static const source_def sci_noaa_mapsvc_raster = {
  .id = "sci-noaa-mapsvc-raster", .collector = "earthobs",
  .name = "NOAA NWS — raster map service directory",
  .name_ja = "NOAA NWS — raster map service directory",
  .update_interval_sec = 90000, .run = run_sci_noaa_mapsvc_raster,
  .category = "geodata", .type = "api",
  .url = "https://mapservices.weather.noaa.gov/raster/rest/services?f=json",
  .description = "Directory of NWS raster services including radar mosaics and gridded forecast imagery.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(sci_noaa_mapsvc_raster);

static int run_sci_noaa_mapsvc_static(const source_ctx *c, intel_sink *s) {
  noaa_mapsvc_opts o = { "sci-noaa-mapsvc-static",
                         "https://mapservices.weather.noaa.gov/static/rest/services",
                         "[\"usa\",\"weather\",\"arcgis\",\"geodata\"]" };
  return noaa_mapsvc_walk(c, s, &o);
}
static const source_def sci_noaa_mapsvc_static = {
  .id = "sci-noaa-mapsvc-static", .collector = "earthobs",
  .name = "NOAA NWS — static reference map service directory",
  .name_ja = "NOAA NWS — static reference map service directory",
  .update_interval_sec = 91800, .run = run_sci_noaa_mapsvc_static,
  .category = "geodata", .type = "api",
  .url = "https://mapservices.weather.noaa.gov/static/rest/services?f=json",
  .description = "Directory of static NWS boundary and reference services (zones, counties, basins).",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(sci_noaa_mapsvc_static);

static int run_sci_noaa_mapsvc_vector(const source_ctx *c, intel_sink *s) {
  noaa_mapsvc_opts o = { "sci-noaa-mapsvc-vector",
                         "https://mapservices.weather.noaa.gov/vector/rest/services",
                         "[\"usa\",\"weather\",\"arcgis\",\"geodata\"]" };
  return noaa_mapsvc_walk(c, s, &o);
}
static const source_def sci_noaa_mapsvc_vector = {
  .id = "sci-noaa-mapsvc-vector", .collector = "earthobs",
  .name = "NOAA NWS — vector map service directory",
  .name_ja = "NOAA NWS — vector map service directory",
  .update_interval_sec = 88200, .run = run_sci_noaa_mapsvc_vector,
  .category = "geodata", .type = "api",
  .url = "https://mapservices.weather.noaa.gov/vector/rest/services?f=json",
  .description = "Directory of NWS vector map services covering observations, forecasts and marine zones.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(sci_noaa_mapsvc_vector);

VJSON(sci_ogc_idee_spain, "sci-ogc-idee-spain", "Spain IGN — OGC API Features collection catalogue", "Spain IGN — OGC API Features collection catalogue",
  "earthobs", "geodata",
  "https://api-features.idee.es/collections?f=json",
  "collections",
  "es", "[\"esp\",\"geodata\",\"inspire\",\"catalog\"]", 86400,
  "Lists every INSPIRE feature collection the Spanish national mapping agency serves, with extents and CRS.");

VGEO(sci_ogcf_idee_existinglanduseobject, "sci-ogcf-idee-existinglanduseobject", "Spain IGN OGC API Features — existinglanduseobject", "Spain IGN OGC API Features — existinglanduseobject",
  "earthobs", "geodata",
  "https://api-features.idee.es/collections/existinglanduseobject/items?f=json&limit=200",
  "es", "[\"esp\",\"geodata\",\"inspire\"]", 45000,
  "INSPIRE-harmonised GeoJSON features from the Spanish national mapping agency 'existinglanduseobject' collection.");

VGEO(sci_ogcf_idee_hydronode, "sci-ogcf-idee-hydronode", "Spain IGN OGC API Features — hydronode", "Spain IGN OGC API Features — hydronode",
  "earthobs", "geodata",
  "https://api-features.idee.es/collections/hydronode/items?f=json&limit=200",
  "es", "[\"esp\",\"geodata\",\"inspire\"]", 48600,
  "INSPIRE-harmonised GeoJSON features from the Spanish national mapping agency 'hydronode' collection.");

VGEO(sci_ogcf_idee_landcoverunit, "sci-ogcf-idee-landcoverunit", "Spain IGN OGC API Features — landcoverunit", "Spain IGN OGC API Features — landcoverunit",
  "earthobs", "geodata",
  "https://api-features.idee.es/collections/landcoverunit/items?f=json&limit=200",
  "es", "[\"esp\",\"geodata\",\"inspire\"]", 46800,
  "INSPIRE-harmonised GeoJSON features from the Spanish national mapping agency 'landcoverunit' collection.");

VGEO(sci_ogcf_idee_watercourselink, "sci-ogcf-idee-watercourselink", "Spain IGN OGC API Features — watercourselink", "Spain IGN OGC API Features — watercourselink",
  "earthobs", "geodata",
  "https://api-features.idee.es/collections/watercourselink/items?f=json&limit=200",
  "es", "[\"esp\",\"geodata\",\"inspire\"]", 50400,
  "INSPIRE-harmonised GeoJSON features from the Spanish national mapping agency 'watercourselink' collection.");

VJSON(sci_pbdb_colls_holocene, "sci-pbdb-colls-holocene", "Paleobiology Database — Holocene fossil collections", "Paleobiology Database — Holocene fossil collections",
  "earthobs", "geology",
  "https://paleobiodb.org/data1.2/colls/list.json?interval=Holocene&limit=500&show=coords,loc",
  "records",
  "en", "[\"int\",\"paleontology\",\"fossil\",\"coordinates\"]", 86400,
  "Holocene fossil collection records with latitude/longitude, formation, country and collection method.");

VJSON(sci_pbdb_colls_pleistocene, "sci-pbdb-colls-pleistocene", "Paleobiology Database — Pleistocene fossil collections", "Paleobiology Database — Pleistocene fossil collections",
  "earthobs", "geology",
  "https://paleobiodb.org/data1.2/colls/list.json?interval=Pleistocene&limit=500&show=coords,loc",
  "records",
  "en", "[\"int\",\"paleontology\",\"fossil\",\"coordinates\"]", 88200,
  "Pleistocene fossil collection records with coordinates, lithology and stratigraphic context.");

VJSON(sci_pbdb_occs_mammalia, "sci-pbdb-occs-mammalia", "Paleobiology Database — Pleistocene mammal occurrences", "Paleobiology Database — Pleistocene mammal occurrences",
  "earthobs", "geology",
  "https://paleobiodb.org/data1.2/occs/list.json?base_name=Mammalia&interval=Pleistocene&limit=500&show=coords",
  "records",
  "en", "[\"int\",\"paleontology\",\"fossil\",\"coordinates\"]", 90000,
  "Georeferenced Pleistocene mammal fossil occurrences with taxon identification and collection linkage.");

/* sci-pbdb-strata: `limit=all`, one request. Live-verified 2026-09-14: the
 * service holds 29,617 strata and answers all of them in one 3.3 MB body. At
 * `limit=500` the walk hit the 20-page ceiling (10,000 emitted, TRUNCATED), and
 * because a stratum record carries no id field its uid is hashed from its
 * label, so the 15 labels that recur on a LATER page collapsed at the sink
 * (10,000 emitted, 9,985 stored). In one page the in-page collision guard
 * content-hashes every colliding record: all 29,617 are byte-distinct. */
VJSON(sci_pbdb_strata, "sci-pbdb-strata", "Paleobiology Database — global stratigraphic units", "Paleobiology Database — global stratigraphic units",
  "earthobs", "geology",
  "https://paleobiodb.org/data1.2/strata/list.json?lngmin=-180&lngmax=180&latmin=-90&latmax=90&limit=all",
  "records",
  "en", "[\"int\",\"paleontology\",\"stratigraphy\"]", 91800,
  "Named stratigraphic units worldwide with bounding coordinates and fossil collection counts.");

VGEO(sci_quake_geofon_geojson, "sci-quake-geofon-geojson", "GEOFON (GFZ Potsdam) — recent earthquakes GeoJSON", "GEOFON (GFZ Potsdam) — recent earthquakes GeoJSON",
  "earthobs", "hazard",
  "https://geofon.gfz.de/eqinfo/list.php?fmt=geojson",
  "en", "[\"deu\",\"earthquake\",\"seismic\",\"hazard\",\"geojson\"]", 2700,
  "Same GEOFON bulletin as a FeatureCollection with one point per epicentre, magnitude and depth — directly mappable.");

VRSS(sci_quake_geofon_rss, "sci-quake-geofon-rss", "GEOFON (GFZ Potsdam) — recent earthquakes RSS", "GEOFON (GFZ Potsdam) — recent earthquakes RSS",
  "earthobs", "hazard",
  "https://geofon.gfz.de/eqinfo/list.php?fmt=rss",
  "en", "[\"deu\",\"earthquake\",\"seismic\",\"hazard\"]", 1800,
  "GFZ GEOFON global earthquake bulletin as RSS; magnitude, depth, region and origin time for the most recent located events.");

VJSON(sci_res_crossref_funders, "sci-res-crossref-funders", "Crossref — funder registry", "Crossref — funder registry",
  "earthobs", "science",
  "https://api.crossref.org/funders?rows=100",
  "message.items",
  "en", "[\"int\",\"science\",\"funding\",\"registry\"]", 90000,
  "The Crossref Open Funder Registry: funding bodies with country, alternate names and hierarchy.");

VJSON(sci_res_crossref_journals, "sci-res-crossref-journals", "Crossref — journal registry", "Crossref — journal registry",
  "earthobs", "science",
  "https://api.crossref.org/journals?rows=100",
  "message.items",
  "en", "[\"int\",\"science\",\"bibliometrics\",\"registry\"]", 86400,
  "Journals registered with Crossref, their ISSNs, publisher and deposited article counts.");

VJSON(sci_res_crossref_members, "sci-res-crossref-members", "Crossref — member publisher registry", "Crossref — member publisher registry",
  "earthobs", "science",
  "https://api.crossref.org/members?rows=100",
  "message.items",
  "en", "[\"int\",\"science\",\"publisher\",\"registry\"]", 88200,
  "Crossref member organisations with prefixes, deposit counts and metadata coverage.");
