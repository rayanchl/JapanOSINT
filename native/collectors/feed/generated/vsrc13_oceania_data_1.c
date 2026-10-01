/* Verified-live oceania_data sources (60), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VGEO(sas_arc_nsw_bfpl, "sas-arc-nsw-bfpl", "NSW DCCEEW — bushfire prone land", "NSW DCCEEW — bushfire prone land",
  "oceania_data", "hazard",
  "https://mapprod3.environment.nsw.gov.au/arcgis/rest/services/Fire/BFPL/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=50&geometryPrecision=4&f=geojson",
  "en", "[\"aus\",\"nsw\",\"bushfire\",\"geo\"]", 86400,
  "Bushfire prone land polygons used for NSW development planning controls.");

VGEO(sas_arc_nsw_edufac, "sas-arc-nsw-edufac", "NSW Spatial Services — education facilities", "NSW Spatial Services — education facilities",
  "oceania_data", "opendata",
  "https://portal.spatial.nsw.gov.au/server/rest/services/NSW_FOI_Education_Facilities/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"nsw\",\"education\",\"geo\"]", 86400,
  "Point locations of NSW schools, TAFEs and universities with names and types.");

VGEO(sas_arc_nsw_emergfac, "sas-arc-nsw-emergfac", "NSW Spatial Services — emergency service facilities", "NSW Spatial Services — emergency service facilities",
  "oceania_data", "hazard",
  "https://portal.spatial.nsw.gov.au/server/rest/services/NSW_FOI_Emergency_Service_Facilities/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"nsw\",\"emergency\",\"geo\"]", 86400,
  "Point locations and attributes of NSW fire, ambulance, police and SES facilities.");

#include "lib/arcgis_dir.h"

/* sas-arc-*: ArcGIS REST services directories. Each old row pointed the JSON
 * emitter at "folders" — bare folder-name strings (NSW env 41, QLD 57, TAS 8,
 * WA SLIP 5 on 2026-09-14) — and emitted nothing on every run.
 * lib/arcgis_dir.c walks each folder listing and emits the services. */
static int run_sas_arc_nsw_env(const source_ctx *c, intel_sink *s) {
  return arcgis_dir_walk(c, s, "sas-arc-nsw-env",
                         "https://mapprod3.environment.nsw.gov.au/arcgis/rest/services",
                         "environment", "en", "[\"aus\",\"nsw\",\"gis\"]");
}
static const source_def sas_arc_nsw_env = {
  .id = "sas-arc-nsw-env", .collector = "oceania_data",
  .name = "NSW DCCEEW — environment ArcGIS folder directory",
  .name_ja = "NSW DCCEEW — environment ArcGIS folder directory",
  .update_interval_sec = 86400, .run = run_sas_arc_nsw_env,
  .category = "environment", .type = "api",
  .url = "https://mapprod3.environment.nsw.gov.au/arcgis/rest/services?f=json",
  .description = "Services published in the NSW environment department ArcGIS directory, covering fire, flood, soil and vegetation.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(sas_arc_nsw_env);

VGEO(sas_arc_nsw_fire_hist, "sas-arc-nsw-fire-hist", "NSW NPWS — fire history polygons", "NSW NPWS — fire history polygons",
  "oceania_data", "hazard",
  "https://mapprod3.environment.nsw.gov.au/arcgis/rest/services/Fire/NPWS_Fire_History/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=50&geometryPrecision=4&f=geojson",
  "en", "[\"aus\",\"nsw\",\"bushfire\",\"geo\"]", 86400,
  "Mapped extents and dates of past bushfires and hazard reduction burns on NSW park estate.");

VGEO(sas_arc_nsw_foi, "sas-arc-nsw-foi", "NSW Spatial Services — features of interest", "NSW Spatial Services — features of interest",
  "oceania_data", "opendata",
  "https://portal.spatial.nsw.gov.au/server/rest/services/NSW_Features_of_Interest_Category/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"nsw\",\"geo\",\"poi\"]", 86400,
  "Named points of interest across NSW: landmarks, community and utility features.");

VGEO(sas_arc_nsw_healthfac, "sas-arc-nsw-healthfac", "NSW Spatial Services — health facilities", "NSW Spatial Services — health facilities",
  "oceania_data", "health",
  "https://portal.spatial.nsw.gov.au/server/rest/services/NSW_FOI_Health_Facilities/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"nsw\",\"health\",\"geo\"]", 86400,
  "Point locations of NSW hospitals, community health and aged-care facilities.");

VGEO(sas_arc_nsw_justfac, "sas-arc-nsw-justfac", "NSW Spatial Services — justice facilities", "NSW Spatial Services — justice facilities",
  "oceania_data", "opendata",
  "https://portal.spatial.nsw.gov.au/server/rest/services/NSW_FOI_Justice_Facilities/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"nsw\",\"justice\",\"geo\"]", 86400,
  "Point locations of NSW courts, correctional centres and justice facilities.");

VGEO(sas_arc_nsw_reef, "sas-arc-nsw-reef", "NSW DCCEEW — seabed reef extent", "NSW DCCEEW — seabed reef extent",
  "oceania_data", "environment",
  "https://mapprod3.environment.nsw.gov.au/arcgis/rest/services/Marine/NSW_seabed_reef_extent/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=50&geometryPrecision=4&f=geojson",
  "en", "[\"aus\",\"nsw\",\"marine\",\"geo\"]", 86400,
  "Mapped rocky reef extents along the NSW coast from seabed mapping surveys.");

/* sas-arc-nsw-spatial: emitted the root "services" array only (82 on
 * 2026-09-14) and never the 57 services inside its 6 folders. */
static int run_sas_arc_nsw_spatial(const source_ctx *c, intel_sink *s) {
  return arcgis_dir_walk(c, s, "sas-arc-nsw-spatial",
                         "https://portal.spatial.nsw.gov.au/server/rest/services",
                         "opendata", "en", "[\"aus\",\"nsw\",\"gis\"]");
}
static const source_def sas_arc_nsw_spatial = {
  .id = "sas-arc-nsw-spatial", .collector = "oceania_data",
  .name = "NSW Spatial Services — ArcGIS service directory",
  .name_ja = "NSW Spatial Services — ArcGIS service directory",
  .update_interval_sec = 86400, .run = run_sas_arc_nsw_spatial,
  .category = "opendata", .type = "api",
  .url = "https://portal.spatial.nsw.gov.au/server/rest/services?f=json",
  .description = "Index of every published NSW Spatial Services map and feature service, root and folders, used to discover new layers.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(sas_arc_nsw_spatial);

VGEO(sas_arc_nsw_transfac, "sas-arc-nsw-transfac", "NSW Spatial Services — transport facilities", "NSW Spatial Services — transport facilities",
  "oceania_data", "transport",
  "https://portal.spatial.nsw.gov.au/server/rest/services/NSW_FOI_Transport_Facilities/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"nsw\",\"transport\",\"geo\"]", 86400,
  "Point locations of NSW airports, wharves, rail stations and freight terminals.");

static int run_sas_arc_qld(const source_ctx *c, intel_sink *s) {
  return arcgis_dir_walk(c, s, "sas-arc-qld",
                         "https://spatial-gis.information.qld.gov.au/arcgis/rest/services",
                         "opendata", "en", "[\"aus\",\"qld\",\"gis\"]");
}
static const source_def sas_arc_qld = {
  .id = "sas-arc-qld", .collector = "oceania_data",
  .name = "Queensland Spatial — ArcGIS folder directory",
  .name_ja = "Queensland Spatial — ArcGIS folder directory",
  .update_interval_sec = 86400, .run = run_sas_arc_qld,
  .category = "opendata", .type = "api",
  .url = "https://spatial-gis.information.qld.gov.au/arcgis/rest/services?f=json",
  .description = "Services published in the Queensland Government ArcGIS directory, spanning flood, environment and cadastre.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(sas_arc_qld);

VGEO(sas_arc_qld_basins, "sas-arc-qld-basins", "Queensland FloodCheck — drainage basins", "Queensland FloodCheck — drainage basins",
  "oceania_data", "environment",
  "https://spatial-gis.information.qld.gov.au/arcgis/rest/services/FloodCheck/QueenslandBasins/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=50&geometryPrecision=4&f=geojson",
  "en", "[\"aus\",\"qld\",\"water\",\"geo\"]", 86400,
  "Queensland river basin boundaries underpinning flood and water-resource assessment.");

VGEO(sas_arc_qld_firescar, "sas-arc-qld-firescar", "Queensland — fire scar mapping", "Queensland — fire scar mapping",
  "oceania_data", "hazard",
  "https://spatial-gis.information.qld.gov.au/arcgis/rest/services/Environment/FireScarMapping/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=50&geometryPrecision=4&f=geojson",
  "en", "[\"aus\",\"qld\",\"bushfire\",\"geo\"]", 86400,
  "Satellite-derived fire scar polygons across Queensland with burn dates.");

VGEO(sas_arc_qld_floodgauge, "sas-arc-qld-floodgauge", "Queensland FloodCheck — flood gauge network", "Queensland FloodCheck — flood gauge network",
  "oceania_data", "hazard",
  "https://spatial-gis.information.qld.gov.au/arcgis/rest/services/FloodCheck/FloodGauges/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=500&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"qld\",\"flood\",\"geo\"]", 43200,
  "Locations and identifiers of Queensland flood-warning gauges used in FloodCheck.");

VGEO(sas_arc_qld_lga, "sas-arc-qld-lga", "Queensland FloodCheck — local government areas", "Queensland FloodCheck — local government areas",
  "oceania_data", "opendata",
  "https://spatial-gis.information.qld.gov.au/arcgis/rest/services/FloodCheck/LocalGovernmentAuthorities/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=50&geometryPrecision=4&f=geojson",
  "en", "[\"aus\",\"qld\",\"boundaries\",\"geo\"]", 86400,
  "Queensland local government area boundaries as used by the FloodCheck portal.");

VGEO(sas_arc_qld_parksfire, "sas-arc-qld-parksfire", "Queensland Parks — fire history", "Queensland Parks — fire history",
  "oceania_data", "hazard",
  "https://spatial-gis.information.qld.gov.au/arcgis/rest/services/Environment/ParksFireHistory/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=50&geometryPrecision=4&f=geojson",
  "en", "[\"aus\",\"qld\",\"bushfire\",\"geo\"]", 86400,
  "Recorded fire extents and dates on Queensland protected-area estate.");

VGEO(sas_arc_qld_towns, "sas-arc-qld-towns", "Queensland FloodCheck — town locations", "Queensland FloodCheck — town locations",
  "oceania_data", "opendata",
  "https://spatial-gis.information.qld.gov.au/arcgis/rest/services/FloodCheck/Towns/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=500&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"qld\",\"geo\",\"places\"]", 86400,
  "Gazetted Queensland town points used to index flood studies by locality.");

static int run_sas_arc_tas(const source_ctx *c, intel_sink *s) {
  return arcgis_dir_walk(c, s, "sas-arc-tas",
                         "https://services.thelist.tas.gov.au/arcgis/rest/services",
                         "opendata", "en", "[\"aus\",\"tas\",\"gis\"]");
}
static const source_def sas_arc_tas = {
  .id = "sas-arc-tas", .collector = "oceania_data",
  .name = "Tasmania LIST — ArcGIS folder directory",
  .name_ja = "Tasmania LIST — ArcGIS folder directory",
  .update_interval_sec = 86400, .run = run_sas_arc_tas,
  .category = "opendata", .type = "api",
  .url = "https://services.thelist.tas.gov.au/arcgis/rest/services?f=json",
  .description = "Services published in the Tasmanian Land Information System ArcGIS directory.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(sas_arc_tas);

VGEO(sas_arc_tas_geol, "sas-arc-tas-geol", "Tasmania LIST — geology and soils layer", "Tasmania LIST — geology and soils layer",
  "oceania_data", "environment",
  "https://services.thelist.tas.gov.au/arcgis/rest/services/Public/GeologicalAndSoils/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=100&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"tas\",\"geology\",\"geo\"]", 86400,
  "Tasmanian geological units and soil mapping polygons.");

VGEO(sas_arc_tas_infra, "sas-arc-tas-infra", "Tasmania LIST — infrastructure layer", "Tasmania LIST — infrastructure layer",
  "oceania_data", "transport",
  "https://services.thelist.tas.gov.au/arcgis/rest/services/Public/Infrastructure/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"tas\",\"infrastructure\",\"geo\"]", 86400,
  "Tasmanian infrastructure features: utilities, transport and communications assets.");

VGEO(sas_arc_tas_marine, "sas-arc-tas-marine", "Tasmania LIST — marine and coastal layer", "Tasmania LIST — marine and coastal layer",
  "oceania_data", "environment",
  "https://services.thelist.tas.gov.au/arcgis/rest/services/Public/MarineAndCoastal/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"tas\",\"marine\",\"geo\"]", 86400,
  "Tasmanian coastal and marine features including reserves and coastal geomorphology.");

VGEO(sas_arc_tas_natenv, "sas-arc-tas-natenv", "Tasmania LIST — natural environment layer", "Tasmania LIST — natural environment layer",
  "oceania_data", "environment",
  "https://services.thelist.tas.gov.au/arcgis/rest/services/Public/NaturalEnvironment/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=100&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"tas\",\"environment\",\"geo\"]", 86400,
  "Tasmanian vegetation, reserve and natural-values mapping features.");

VJSON(sas_arc_vicorg, "sas-arc-vicorg", "Victorian Government — ArcGIS Online service list", "Victorian Government — ArcGIS Online service list",
  "oceania_data", "opendata",
  "https://services6.arcgis.com/GB33F62SbDxJjwEL/arcgis/rest/services?f=json",
  "services",
  "en", "[\"aus\",\"vic\",\"gis\"]", 86400,
  "Feature services published by the Victorian Government ArcGIS Online organisation.");

VGEO(sas_arc_wa_bounds, "sas-arc-wa-bounds", "WA SLIP — administrative boundaries", "WA SLIP — administrative boundaries",
  "oceania_data", "opendata",
  "https://services.slip.wa.gov.au/public/rest/services/SLIP_Public_Services/Boundaries/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=50&geometryPrecision=4&f=geojson",
  "en", "[\"aus\",\"wa\",\"boundaries\",\"geo\"]", 86400,
  "Western Australian local government and administrative boundary polygons.");

VGEO(sas_arc_wa_edu, "sas-arc-wa-edu", "WA SLIP — education facilities", "WA SLIP — education facilities",
  "oceania_data", "opendata",
  "https://services.slip.wa.gov.au/public/rest/services/SLIP_Public_Services/Education/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"wa\",\"education\",\"geo\"]", 86400,
  "Point locations of Western Australian schools and education facilities.");

VGEO(sas_arc_wa_env, "sas-arc-wa-env", "WA SLIP — environment layer", "WA SLIP — environment layer",
  "oceania_data", "environment",
  "https://services.slip.wa.gov.au/public/rest/services/SLIP_Public_Services/Environment/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=100&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"wa\",\"environment\",\"geo\"]", 86400,
  "Western Australian environmental features: reserves, vegetation and conservation areas.");

VGEO(sas_arc_wa_health, "sas-arc-wa-health", "WA SLIP — health facilities", "WA SLIP — health facilities",
  "oceania_data", "health",
  "https://services.slip.wa.gov.au/public/rest/services/SLIP_Public_Services/Health/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"wa\",\"health\",\"geo\"]", 86400,
  "Point locations of Western Australian hospitals and health service facilities.");

VGEO(sas_arc_wa_infra, "sas-arc-wa-infra", "WA SLIP — infrastructure and utilities", "WA SLIP — infrastructure and utilities",
  "oceania_data", "transport",
  "https://services.slip.wa.gov.au/public/rest/services/SLIP_Public_Services/Infrastructure_and_Utilities/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"wa\",\"infrastructure\",\"geo\"]", 86400,
  "Western Australian utility and infrastructure asset locations.");

VGEO(sas_arc_wa_marine, "sas-arc-wa-marine", "WA SLIP — marine and estuaries", "WA SLIP — marine and estuaries",
  "oceania_data", "environment",
  "https://services.slip.wa.gov.au/public/rest/services/SLIP_Public_Services/Marine_and_Estuaries/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=100&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"wa\",\"marine\",\"geo\"]", 86400,
  "Western Australian marine parks, estuaries and coastal management features.");

VGEO(sas_arc_wa_mining, "sas-arc-wa-mining", "WA SLIP — industry and mining", "WA SLIP — industry and mining",
  "oceania_data", "economy",
  "https://services.slip.wa.gov.au/public/rest/services/SLIP_Public_Services/Industry_and_Mining/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=200&geometryPrecision=5&f=geojson",
  "en", "[\"aus\",\"wa\",\"mining\",\"geo\"]", 86400,
  "Western Australian mine sites, tenements and industrial facility locations.");

static int run_sas_arc_wa_slip(const source_ctx *c, intel_sink *s) {
  return arcgis_dir_walk(c, s, "sas-arc-wa-slip",
                         "https://services.slip.wa.gov.au/public/rest/services",
                         "opendata", "en", "[\"aus\",\"wa\",\"gis\"]");
}
static const source_def sas_arc_wa_slip = {
  .id = "sas-arc-wa-slip", .collector = "oceania_data",
  .name = "WA Landgate SLIP — ArcGIS folder directory",
  .name_ja = "WA Landgate SLIP — ArcGIS folder directory",
  .update_interval_sec = 86400, .run = run_sas_arc_wa_slip,
  .category = "opendata", .type = "api",
  .url = "https://services.slip.wa.gov.au/public/rest/services?f=json",
  .description = "Services published in the Western Australian Shared Location Information Platform public ArcGIS directory.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(sas_arc_wa_slip);

VJSON(sas_ckan_au_cyclone, "sas-ckan-au-cyclone", "data.gov.au — cyclone datasets", "data.gov.au — cyclone datasets",
  "oceania_data", "hazard",
  "https://data.gov.au/data/api/3/action/package_search?q=cyclone&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"cyclone\",\"opendata\"]", 86400,
  "Federal catalogue slice for tropical-cyclone tracks, wind hazard and impact datasets.");

VJSON(sas_ckan_au_emerg, "sas-ckan-au-emerg", "data.gov.au — emergency management datasets", "data.gov.au — emergency management datasets",
  "oceania_data", "hazard",
  "https://data.gov.au/data/api/3/action/package_search?q=emergency&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"emergency\",\"opendata\"]", 86400,
  "Federal catalogue slice for emergency-management datasets: incidents, response boundaries, risk layers.");

VJSON(sas_ckan_au_energy, "sas-ckan-au-energy", "data.gov.au — energy datasets", "data.gov.au — energy datasets",
  "oceania_data", "economy",
  "https://data.gov.au/data/api/3/action/package_search?q=energy&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"energy\",\"opendata\"]", 86400,
  "Federal catalogue slice for electricity, gas and renewable-generation datasets.");

VJSON(sas_ckan_au_env, "sas-ckan-au-env", "data.gov.au — environment datasets", "data.gov.au — environment datasets",
  "oceania_data", "environment",
  "https://data.gov.au/data/api/3/action/package_search?q=environment&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"environment\",\"opendata\"]", 86400,
  "Federal catalogue slice for environmental monitoring, biodiversity and land-cover datasets.");

VJSON(sas_ckan_au_health, "sas-ckan-au-health", "data.gov.au — health datasets", "data.gov.au — health datasets",
  "oceania_data", "health",
  "https://data.gov.au/data/api/3/action/package_search?q=health&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"health\",\"opendata\"]", 86400,
  "Federal catalogue slice for health datasets including facilities, workforce and outcomes.");

VJSON(sas_ckan_au_recent, "sas-ckan-au-recent", "data.gov.au — recently changed datasets with resources", "data.gov.au — recently changed datasets with resources",
  "oceania_data", "opendata",
  "https://data.gov.au/data/api/3/action/current_package_list_with_resources?limit=50",
  "result",
  "en", "[\"aus\",\"opendata\",\"changes\"]", 43200,
  "Most recently updated federal datasets including their downloadable resource URLs.");

VJSON(sas_ckan_au_transport, "sas-ckan-au-transport", "data.gov.au — transport datasets", "data.gov.au — transport datasets",
  "oceania_data", "transport",
  "https://data.gov.au/data/api/3/action/package_search?q=transport&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"transport\",\"opendata\"]", 86400,
  "Federal catalogue slice for transport datasets: road, rail, aviation and maritime networks.");

VJSON(sas_ckan_au_water, "sas-ckan-au-water", "data.gov.au — water-quality datasets", "data.gov.au — water-quality datasets",
  "oceania_data", "environment",
  "https://data.gov.au/data/api/3/action/package_search?q=water+quality&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"water\",\"opendata\"]", 86400,
  "Federal catalogue slice for water-quality monitoring and catchment datasets.");

VJSON(sas_ckan_datagovau, "sas-ckan-datagovau", "data.gov.au — national dataset catalogue", "data.gov.au — national dataset catalogue",
  "oceania_data", "opendata",
  "https://data.gov.au/data/api/3/action/package_search?rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"opendata\",\"catalogue\"]", 86400,
  "Latest 100 datasets published to the Australian federal open-data catalogue, with publisher, licence and resource URLs.");

VJSON(sas_ckan_datagovau_flood, "sas-ckan-datagovau-flood", "data.gov.au — flood datasets", "data.gov.au — flood datasets",
  "oceania_data", "hazard",
  "https://data.gov.au/data/api/3/action/package_search?q=flood&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"flood\",\"opendata\"]", 86400,
  "Federal open-data catalogue slice covering flood mapping, gauges and inundation studies.");

VJSON(sas_ckan_datansw, "sas-ckan-datansw", "NSW Government — open-data catalogue", "NSW Government — open-data catalogue",
  "oceania_data", "opendata",
  "https://data.nsw.gov.au/data/api/3/action/package_search?rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"nsw\",\"opendata\"]", 86400,
  "Latest 100 datasets published by New South Wales agencies.");

VJSON(sas_ckan_datansw_hazard, "sas-ckan-datansw-hazard", "NSW Government — hazard datasets", "NSW Government — hazard datasets",
  "oceania_data", "hazard",
  "https://data.nsw.gov.au/data/api/3/action/package_search?q=hazard&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"nsw\",\"hazard\"]", 86400,
  "NSW catalogue slice for bushfire, flood and other natural-hazard datasets.");

VJSON(sas_ckan_datant, "sas-ckan-datant", "Northern Territory — open-data catalogue", "Northern Territory — open-data catalogue",
  "oceania_data", "opendata",
  "https://data.nt.gov.au/api/3/action/package_search?rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"nt\",\"opendata\"]", 86400,
  "Latest 100 datasets published by Northern Territory agencies.");

VJSON(sas_ckan_dataqld, "sas-ckan-dataqld", "Queensland Government — open-data catalogue", "Queensland Government — open-data catalogue",
  "oceania_data", "opendata",
  "https://www.data.qld.gov.au/api/3/action/package_search?rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"qld\",\"opendata\"]", 86400,
  "Latest 100 datasets published by Queensland agencies.");

VJSON(sas_ckan_dataqld_flood, "sas-ckan-dataqld-flood", "Queensland Government — flood datasets", "Queensland Government — flood datasets",
  "oceania_data", "hazard",
  "https://www.data.qld.gov.au/api/3/action/package_search?q=flood&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"qld\",\"flood\"]", 86400,
  "Queensland catalogue slice for flood studies, gauge networks and historic inundation.");

VJSON(sas_ckan_datasa, "sas-ckan-datasa", "South Australia — open-data catalogue", "South Australia — open-data catalogue",
  "oceania_data", "opendata",
  "https://data.sa.gov.au/data/api/3/action/package_search?rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"sa\",\"opendata\"]", 86400,
  "Latest 100 datasets published by South Australian agencies.");

/* data.sa.gov.au's search index serves some package ids TWICE with different
 * content — the second hit carries a different organization, harvest source,
 * resources and metadata_modified (11 such pairs on page 1, 2026-09-15). Keyed
 * on id alone the later hit overwrote the earlier: 914 emitted, 899 stored
 * (2026-09-14). Both hits are what the upstream returned, so the key is
 * (id, metadata_modified); a byte-identical repeat still collapses. */
#include "_vjson_idkeys.inc"
VJSON_IDKEYS(sas_ckan_datasa_fire, "sas-ckan-datasa-fire", "South Australia — bushfire datasets", "South Australia — bushfire datasets",
  "oceania_data", "hazard",
  "https://data.sa.gov.au/data/api/3/action/package_search?q=bushfire&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"sa\",\"bushfire\"]", 86400,
  "South Australian catalogue slice for bushfire risk and CFS incident datasets.",
  "id+metadata_modified");

VJSON(sas_ckan_datavic, "sas-ckan-datavic", "Victoria — open-data catalogue", "Victoria — open-data catalogue",
  "oceania_data", "opendata",
  "https://discover.data.vic.gov.au/api/3/action/package_search?rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"vic\",\"opendata\"]", 86400,
  "Latest 100 datasets published by Victorian agencies.");

VJSON(sas_ckan_datawa, "sas-ckan-datawa", "Western Australia — open-data catalogue", "Western Australia — open-data catalogue",
  "oceania_data", "opendata",
  "https://catalogue.data.wa.gov.au/api/3/action/package_search?rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"wa\",\"opendata\"]", 86400,
  "Latest 100 datasets published by Western Australian agencies.");

VJSON(sas_ckan_datawa_fire, "sas-ckan-datawa-fire", "Western Australia — fire datasets", "Western Australia — fire datasets",
  "oceania_data", "hazard",
  "https://catalogue.data.wa.gov.au/api/3/action/package_search?q=fire&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"wa\",\"bushfire\"]", 86400,
  "Western Australian catalogue slice for bushfire-prone areas, fire history and DFES data.");

VJSON(sas_ckan_nsw_crime, "sas-ckan-nsw-crime", "NSW Government — crime datasets", "NSW Government — crime datasets",
  "oceania_data", "crime",
  "https://data.nsw.gov.au/data/api/3/action/package_search?q=crime&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"nsw\",\"crime\"]", 86400,
  "NSW catalogue slice for BOCSAR crime statistics and police-reported incident data.");

VJSON(sas_ckan_nsw_env, "sas-ckan-nsw-env", "NSW Government — environment datasets", "NSW Government — environment datasets",
  "oceania_data", "environment",
  "https://data.nsw.gov.au/data/api/3/action/package_search?q=environment&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"nsw\",\"environment\"]", 86400,
  "NSW catalogue slice for air, water, vegetation and biodiversity monitoring.");

VJSON(sas_ckan_nsw_flood, "sas-ckan-nsw-flood", "NSW Government — flood datasets", "NSW Government — flood datasets",
  "oceania_data", "hazard",
  "https://data.nsw.gov.au/data/api/3/action/package_search?q=flood&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"nsw\",\"flood\"]", 86400,
  "NSW catalogue slice for flood studies, gauges and inundation extents.");

VJSON(sas_ckan_nsw_health, "sas-ckan-nsw-health", "NSW Government — health datasets", "NSW Government — health datasets",
  "oceania_data", "health",
  "https://data.nsw.gov.au/data/api/3/action/package_search?q=health&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"nsw\",\"health\"]", 86400,
  "NSW catalogue slice for hospital, notifiable-disease and health-workforce datasets.");

VJSON(sas_ckan_nsw_recent, "sas-ckan-nsw-recent", "NSW Government — recently changed datasets", "NSW Government — recently changed datasets",
  "oceania_data", "opendata",
  "https://data.nsw.gov.au/data/api/3/action/current_package_list_with_resources?limit=50",
  "result",
  "en", "[\"aus\",\"nsw\",\"changes\"]", 43200,
  "Most recently updated NSW datasets including resource download URLs.");

VJSON(sas_ckan_nsw_transport, "sas-ckan-nsw-transport", "NSW Government — transport datasets", "NSW Government — transport datasets",
  "oceania_data", "transport",
  "https://data.nsw.gov.au/data/api/3/action/package_search?q=transport&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"nsw\",\"transport\"]", 86400,
  "NSW catalogue slice for road, rail and public-transport datasets.");

VJSON(sas_ckan_nt_env, "sas-ckan-nt-env", "Northern Territory — environment datasets", "Northern Territory — environment datasets",
  "oceania_data", "environment",
  "https://data.nt.gov.au/api/3/action/package_search?q=environment&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"nt\",\"environment\"]", 86400,
  "Northern Territory catalogue slice for land, vegetation and water-resource datasets.");

VJSON(sas_ckan_nt_flood, "sas-ckan-nt-flood", "Northern Territory — flood datasets", "Northern Territory — flood datasets",
  "oceania_data", "hazard",
  "https://data.nt.gov.au/api/3/action/package_search?q=flood&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"aus\",\"nt\",\"flood\"]", 86400,
  "Northern Territory catalogue slice for flood mapping and monsoon hydrology.");
