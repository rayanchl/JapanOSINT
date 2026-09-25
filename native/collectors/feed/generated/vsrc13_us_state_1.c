/* Verified-live us_state sources (60), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

/* 2026-09-08 EMITS_NOTHING fix for the six "* — ArcGIS service directory" rows
 * (nam-cageo-conservation-services, nam-caltrans-services,
 *  nam-delaware-firstmap-services, nam-mass-gis-services, nam-md-imap-services,
 *  nam-nh-geodata-services).
 *
 * They were VJSON rows with array_path "folders". Fetched live 2026-09-08, an
 * ArcGIS Server root answers
 *   {"currentVersion":11.3,"folders":["Agriculture","Biota",...],"services":[]}
 * so "folders" is an array of bare STRINGS, which jsonlist cannot turn into
 * records (no field to label), and root "services" is EMPTY on four of the six
 * (gis.conservation.ca.gov, firstmap.delaware.gov, mdgeodata.md.gov) or holds
 * one stray entry on the other two. Hence: fetch fine, emit nothing, forever.
 *
 * The services are one level down, at /rest/services/<folder>?f=json, e.g.
 * mdgeodata.md.gov/imap/rest/services/Transportation?f=json ->
 *   {"folders":[],"services":[{"name":"Transportation/MD_AlternativeFuel",
 *                              "type":"FeatureServer"}, ...]}
 * with `name` already folder-qualified, so records from different folders can
 * never be confused. Verified live on Maryland and Caltrans; on both, the
 * returned sub-folder "folders" array is empty, so the directory is one level
 * deep and a single pass over the root's folder list is exhaustive.
 *
 * Pointing a row at one folder would satisfy the emit gate while discarding the
 * other twenty-three (house rule 2), so this walks EVERY folder the root named
 * and emits every service in each. Nothing is invented: the folder list comes
 * from the server's own response, and the only URL this composes is the
 * documented /rest/services/<folder> child of the endpoint already declared.
 * A folder whose own fetch fails is skipped rather than aborting the run, so
 * one bad folder cannot cost the other twenty. */
static int ags_dir_collect(const source_ctx *c, intel_sink *s, const char *id,
                           const char *base, const char *cat, const char *tags) {
  char root[512];
  snprintf(root, sizeof root, "%s?f=json", base);
  cJSON *doc = pw_fetch_json(c, root, NULL);
  if (!doc) return -1;
  int n = jsonlist_emit_ex(s, id, doc, "services", cat, "en", tags, NULL);
  cJSON *folders = cJSON_GetObjectItemCaseSensitive(doc, "folders");
  cJSON *f;
  cJSON_ArrayForEach(f, folders) {
    if (!cJSON_IsString(f) || !f->valuestring || !*f->valuestring) continue;
    char url[768];
    snprintf(url, sizeof url, "%s/%s?f=json", base, f->valuestring);
    cJSON *sub = pw_fetch_json(c, url, NULL);
    if (!sub) continue;
    n += jsonlist_emit_ex(s, id, sub, "services", cat, "en", tags, NULL);
    cJSON_Delete(sub);
  }
  cJSON_Delete(doc);
  return n;
}

/* BASE is the service root WITHOUT the query string; the declared .url stays
 * byte-identical to the endpoint these rows already carried, so no duplicate
 * endpoint is introduced. */
#define AGSDIR(SYM, ID, NAME, NAMEJA, CAT, BASE, TAGS, IVAL, DESC)            \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                  \
    int n = ags_dir_collect(c, s, ID, BASE, CAT, TAGS);                       \
    if (n < 0) { fprintf(stderr, "[%s] fetch failed\n", ID); return -1; }     \
    return 0; }                                                               \
  static const source_def SYM = {                                             \
    .id = ID, .collector = "us_state", .name = NAME, .name_ja = NAMEJA,       \
    .update_interval_sec = IVAL, .run = run_##SYM,                            \
    .category = CAT, .type = "api", .url = BASE "?f=json",                    \
    .description = DESC, .layer = NULL, .free_tier = 1 };                     \
  REGISTER_SOURCE(SYM)

VGEO(nam_ags_arizona_social_vulnerability_index_azsvi_by_tracts_a, "nam-ags-arizona-social-vulnerability-index-azsvi-by-tracts-a", "Arizona Social Vulnerability Index (AZSVI) by Tracts — ADHS AZSVIOverall", "Arizona Social Vulnerability Index (AZSVI) by Tracts — ADHS AZSVIOverall",
  "us_state", "health",
  "https://services1.arcgis.com/mpVYz37anSdrK4d8/arcgis/rest/services/Arizona_Social_Vulnerability_Index_Overall/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"health\",\"gis\",\"arcgis\"]", 86400,
  "ArcGIS FeatureServer layer 'ADHS AZSVIOverall' from the 'Arizona Social Vulnerability Index (AZSVI) by Tracts' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_bpd_crimes_bpd_crimes, "nam-ags-bpd-crimes-bpd-crimes", "BPD Crimes — BPD Crimes", "BPD Crimes — BPD Crimes",
  "us_state", "opendata",
  "https://services1.arcgis.com/WHM6qC35aMtyAAlN/arcgis/rest/services/BPD_Crimes_Public/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"opendata\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer 'BPD Crimes' from the 'BPD Crimes' service, GeoJSON with per-feature geometry.");

#include "_vjson_idkeys.inc"
/* 2026-09-08 COLLISION triage (emitted 10,000 / stored 9,195, 805 lost).
 * Cause (A), a dimension declared as the record id — but the declaration is in
 * lib/geojson.c, not in this row, so there is nothing to change here.
 *
 * Measured live 2026-09-08 against this exact layer (88,247 features total):
 *   - offset paging is STABLE. Twenty pages of 500 at resultOffset 0..9,500
 *     returned 10,000 features with 10,000 DISTINCT OBJECTIDs, with and
 *     without &orderByFields=OBJECTID. So this is not cause (C).
 *   - the same 10,000 features carry only 9,084 distinct `stop_id`
 *     ("stop_id":"105" appears 8 times, "154" 7 times, ...). GTFS stop ids are
 *     unique per AGENCY, not statewide, and this layer is every California
 *     agency's stops in one table.
 *   - lib/geojson.c NATIVE_ID_KEYS puts "stop_id" AHEAD of the feature's own
 *     top-level `id`, and ArcGIS f=geojson sets that `id` to OBJECTID, which
 *     IS unique. So feature_uid keys on the group, not the record.
 *   - 10,000 - 9,084 = 916 collisions; 805 were lost and the rest were rescued
 *     by gj_collision_map(), which only sees collisions WITHIN one 500-feature
 *     page. Colliding stops from different agencies mostly land in different
 *     pages, which is why most of them get through the guard.
 *
 * The general repair belongs in lib/geojson.c (a properties key that is only
 * unique within a group must not outrank the feature's own id). This row no
 * longer waits for it: VGEO_IDKEYS (_vjson_idkeys.inc) leaves the walk in
 * geojson_emit_paged untouched and only re-keys each emitted item on the
 * OBJECTID its own properties carry. (agency, stop_id) is not a substitute —
 * 9,953 distinct over the same 10,000 features on 2026-09-15. */
VGEO_IDKEYS(nam_ags_california_transit_stops_ca_transit_stops, "nam-ags-california-transit-stops-ca-transit-stops", "California Transit Stops — CA Transit Stops", "California Transit Stops — CA Transit Stops",
  "us_state", "transport",
  "https://caltrans-gis.dot.ca.gov/arcgis/rest/services/CHrailroad/CA_Transit_Stops/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"transport\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer 'CA Transit Stops' from the 'California Transit Stops' service, GeoJSON with per-feature geometry.",
  "OBJECTID");

VGEO(nam_ags_cfp_priority_highway_freight_network_truck_crashes_a, "nam-ags-cfp-priority-highway-freight-network-truck-crashes-a", "CFP Priority Highway Freight Network — Truck Crashes All / Truck VMT TrkCrshRt", "CFP Priority Highway Freight Network — Truck Crashes All / Truck VMT TrkCrshRt",
  "us_state", "transport",
  "https://services2.arcgis.com/aIrBD8yn1TDTEXoz/arcgis/rest/services/CFP_Priority_Highway_Freight_Network_(PHFN)_group_layer_view/FeatureServer/1/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"transport\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer 'Truck Crashes All / Truck VMT TrkCrshRt' from the 'CFP Priority Highway Freight Network' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_cfp_priority_highway_freight_network_truck_crashes_p, "nam-ags-cfp-priority-highway-freight-network-truck-crashes-p", "CFP Priority Highway Freight Network — Truck Crashes per Mile TrkCrshPM", "CFP Priority Highway Freight Network — Truck Crashes per Mile TrkCrshPM",
  "us_state", "transport",
  "https://services2.arcgis.com/aIrBD8yn1TDTEXoz/arcgis/rest/services/CFP_Priority_Highway_Freight_Network_(PHFN)_group_layer_view/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"transport\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer 'Truck Crashes per Mile TrkCrshPM' from the 'CFP Priority Highway Freight Network' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_indiana_trails_inventory_open_trails_allopentrails, "nam-ags-indiana-trails-inventory-open-trails-allopentrails", "Indiana Trails Inventory Open Trails — AllOpenTrails", "Indiana Trails Inventory Open Trails — AllOpenTrails",
  "us_state", "transport",
  "https://gisdata.in.gov/server/rest/services/Hosted/Trails_AGOL_RO/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"transport\",\"gis\",\"arcgis\"]", 86400,
  "ArcGIS FeatureServer layer 'AllOpenTrails' from the 'Indiana Trails Inventory Open Trails' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_land_ownership_land_ownership, "nam-ags-land-ownership-land-ownership", "Land Ownership — Land Ownership", "Land Ownership — Land Ownership",
  "us_state", "opendata",
  "https://gis.trustlands.utah.gov/mapping/rest/services/Land_Ownership/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"opendata\",\"gis\",\"arcgis\"]", 86400,
  "ArcGIS FeatureServer layer 'Land Ownership' from the 'Land Ownership' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_lion_bike_network_view_lion, "nam-ags-lion-bike-network-view-lion", "LION Bike Network View — LION", "LION Bike Network View — LION",
  "us_state", "transport",
  "https://services.arcgis.com/wmZOI9vyUBq1zTZx/arcgis/rest/services/LION_Bike_Network_View/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"transport\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer 'LION' from the 'LION Bike Network View' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_maine_stream_habitat_viewer_crossings, "nam-ags-maine-stream-habitat-viewer-crossings", "Maine Stream Habitat Viewer — Crossings", "Maine Stream Habitat Viewer — Crossings",
  "us_state", "environment",
  "https://services1.arcgis.com/RbMX0mRVOFNTdLzd/arcgis/rest/services/Maine_Stream_Habitat_Viewer/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"environment\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer 'Crossings' from the 'Maine Stream Habitat Viewer' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_medical_authoritative_data_nursing_homes, "nam-ags-medical-authoritative-data-nursing-homes", "Medical Authoritative Data — Nursing Homes", "Medical Authoritative Data — Nursing Homes",
  "us_state", "health",
  "https://services2.arcgis.com/xtuWQvb2YQnp0z3F/arcgis/rest/services/Medical_Authoritative_Data/FeatureServer/1/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"health\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer 'Nursing Homes' from the 'Medical Authoritative Data' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_montana_air_quality_monitoring_data_montana_air_qual, "nam-ags-montana-air-quality-monitoring-data-montana-air-qual", "Montana Air Quality Monitoring Data — Montana Air Quality Monitoring Data", "Montana Air Quality Monitoring Data — Montana Air Quality Monitoring Data",
  "us_state", "environment",
  "https://gis.mtdeq.us/hosting/rest/services/Hosted/Montana_Air_Quality_Monitoring_Data_REV24/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"environment\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer 'Montana Air Quality Monitoring Data' from the 'Montana Air Quality Monitoring Data' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_oahu_pedestrian_and_bicycle_crash_cluster_classified, "nam-ags-oahu-pedestrian-and-bicycle-crash-cluster-classified", "Oahu Pedestrian and Bicycle Crash Cluster Classified Streets — Oahu Pedestrian and Bicycle Crash Cluster Classified Streets", "Oahu Pedestrian and Bicycle Crash Cluster Classified Streets — Oahu Pedestrian and Bicycle Crash Cluster Classified Streets",
  "us_state", "transport",
  "https://services.arcgis.com/HQ0xoN0EzDPBOEci/arcgis/rest/services/Oahu_Pedestrian_and_Bicycle_Crash_Cluster_Classified_Streets/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"transport\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer 'Oahu Pedestrian and Bicycle Crash Cluster Classified Streets' from the 'Oahu Pedestrian and Bicycle Crash Cluster Classified Streets' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_police_crime_mapping_crimedata, "nam-ags-police-crime-mapping-crimedata", "Police Crime Mapping — CrimeData", "Police Crime Mapping — CrimeData",
  "us_state", "crime",
  "https://services6.arcgis.com/yCArG7wGXGyWLqav/arcgis/rest/services/Police_Crime_Mapping/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"crime\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer 'CrimeData' from the 'Police Crime Mapping' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_wv_efire_inform_view_origins, "nam-ags-wv-efire-inform-view-origins", "WV EFire InFORM view — Origins", "WV EFire InFORM view — Origins",
  "us_state", "opendata",
  "https://services6.arcgis.com/iQxzOWEp1MXm2FTa/arcgis/rest/services/WV_EFire_InFORM_view/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"opendata\",\"gis\",\"arcgis\"]", 86400,
  "ArcGIS FeatureServer layer 'Origins' from the 'WV EFire InFORM view' service, GeoJSON with per-feature geometry.");

VGEO(nam_ags_wv_efire_inform_view_perimeters, "nam-ags-wv-efire-inform-view-perimeters", "WV EFire InFORM view — Perimeters", "WV EFire InFORM view — Perimeters",
  "us_state", "opendata",
  "https://services6.arcgis.com/iQxzOWEp1MXm2FTa/arcgis/rest/services/WV_EFire_InFORM_view/FeatureServer/1/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=500",
  "en", "[\"usa\",\"opendata\",\"gis\",\"arcgis\"]", 86400,
  "ArcGIS FeatureServer layer 'Perimeters' from the 'WV EFire InFORM view' service, GeoJSON with per-feature geometry.");

AGSDIR(nam_cageo_conservation_services, "nam-cageo-conservation-services", "California Department of Conservation — ArcGIS service directory", "California Department of Conservation — ArcGIS service directory",
  "opendata",
  "https://gis.conservation.ca.gov/server/rest/services",
  "[\"usa\",\"ca\",\"gis\",\"arcgis\"]", 86400,
  "Index of the California Geological Survey and Conservation Department GIS services. Walks all 12 folders the server root names (Base, CalGEM, CGS, CGS_Earthquake_Hazard_Zones, DLRP, DMR, DO, Hosted, MOL, Test, Utilities, WellSTAR) and emits every service in each; the root's own services array is empty.");

VRSS(nam_calif_oal_rss, "nam-calif-oal-rss", "California Office of Administrative Law — rulemaking notices", "California Office of Administrative Law — rulemaking notices",
  "us_state", "gazette",
  "https://oal.ca.gov/feed/",
  "en", "[\"usa\",\"ca\",\"gazette\",\"legal\"]", 3600,
  "Notices and bulletins published by California's rulemaking review office.");

VGEO(nam_caltrans_airport_runways_airport_runways, "nam-caltrans-airport-runways-airport-runways", "Caltrans — Airport Runways (Airport Runways)", "Caltrans — Airport Runways (Airport Runways)",
  "us_state", "transport",
  "https://caltrans-gis.dot.ca.gov/arcgis/rest/services/CHaviation/Airport_Runways/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"ca\",\"transport\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Caltrans: Airport Runways, returned as GeoJSON with per-feature geometry.");

VGEO(nam_caltrans_all_roads_all_roads, "nam-caltrans-all-roads-all-roads", "Caltrans — All Roads (All Roads)", "Caltrans — All Roads (All Roads)",
  "us_state", "transport",
  "https://caltrans-gis.dot.ca.gov/arcgis/rest/services/CHhighway/All_Roads/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"ca\",\"transport\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Caltrans: All Roads, returned as GeoJSON with per-feature geometry.");

VGEO(nam_caltrans_cctv_cctv, "nam-caltrans-cctv-cctv", "Caltrans — CCTV (CCTV)", "Caltrans — CCTV (CCTV)",
  "us_state", "opendata",
  "https://caltrans-gis.dot.ca.gov/arcgis/rest/services/CHhighway/CCTV/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"ca\",\"opendata\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Caltrans: CCTV, returned as GeoJSON with per-feature geometry.");

VGEO(nam_caltrans_ccvra_erosion_risk_ccvra_erosion_risk, "nam-caltrans-ccvra-erosion-risk-ccvra-erosion-risk", "Caltrans — CCVRA Erosion Risk (CCVRA Erosion Risk)", "Caltrans — CCVRA Erosion Risk (CCVRA Erosion Risk)",
  "us_state", "opendata",
  "https://caltrans-gis.dot.ca.gov/arcgis/rest/services/CCVRA/CCVRA_Erosion_Risk/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"ca\",\"opendata\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Caltrans: CCVRA Erosion Risk, returned as GeoJSON with per-feature geometry.");

VGEO(nam_caltrans_crs_functional_classification_crs_functional_class, "nam-caltrans-crs-functional-classification-crs-functional-class", "Caltrans — CRS - Functional Classification (CRS Functional Classification)", "Caltrans — CRS - Functional Classification (CRS Functional Classification)",
  "us_state", "opendata",
  "https://caltrans-gis.dot.ca.gov/arcgis/rest/services/CHhighway/CRS_Functional_Classification/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"ca\",\"opendata\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Caltrans: CRS - Functional Classification, returned as GeoJSON with per-feature geometry.");

VGEO(nam_caltrans_hospital_heliports_hospital_heliports, "nam-caltrans-hospital-heliports-hospital-heliports", "Caltrans — Hospital Heliports (Hospital Heliports)", "Caltrans — Hospital Heliports (Hospital Heliports)",
  "us_state", "health",
  "https://caltrans-gis.dot.ca.gov/arcgis/rest/services/CHaviation/Hospital_Heliports/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"ca\",\"health\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Caltrans: Hospital Heliports, returned as GeoJSON with per-feature geometry.");

VGEO(nam_caltrans_local_bridges_local_bridges, "nam-caltrans-local-bridges-local-bridges", "Caltrans — Local Bridges (Local Bridges)", "Caltrans — Local Bridges (Local Bridges)",
  "us_state", "transport",
  "https://caltrans-gis.dot.ca.gov/arcgis/rest/services/CHhighway/Local_Bridges/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"ca\",\"transport\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Caltrans: Local Bridges, returned as GeoJSON with per-feature geometry.");

VGEO(nam_caltrans_public_airport_public_airports, "nam-caltrans-public-airport-public-airports", "Caltrans — Public Airports (Public Airport)", "Caltrans — Public Airports (Public Airport)",
  "us_state", "transport",
  "https://caltrans-gis.dot.ca.gov/arcgis/rest/services/CHaviation/Public_Airport/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"ca\",\"transport\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Caltrans: Public Airports, returned as GeoJSON with per-feature geometry.");

AGSDIR(nam_caltrans_services, "nam-caltrans-services", "Caltrans — ArcGIS service directory", "Caltrans — ArcGIS service directory",
  "transport",
  "https://caltrans-gis.dot.ca.gov/arcgis/rest/services",
  "[\"usa\",\"ca\",\"gis\",\"transport\"]", 86400,
  "Index of Caltrans public GIS services covering highways, bridges and transit assets. Walks all 20 folders the server root names and emits every service in each; the root itself lists only CEPS_V2.");

VGEO(nam_colorado_alternative_fuels_and_electric_vehicle_c, "nam-colorado-alternative-fuels-and-electric-vehicle-c", "Colorado — Alternative Fuels and Electric Vehicle Charging Station Locations in Colorado", "Colorado — Alternative Fuels and Electric Vehicle Charging Station Locations in Colorado",
  "us_state", "transport",
  "https://data.colorado.gov/resource/team-3ugz.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"co\",\"transport\",\"opendata\"]", 1800,
  "Socrata open-data feed from Colorado: Alternative Fuels and Electric Vehicle Charging Station Locations in Colorado, one JSON record per row.");

VGEO(nam_colorado_boulder_county_building_footprints, "nam-colorado-boulder-county-building-footprints", "Colorado — Boulder County Building Footprints", "Colorado — Boulder County Building Footprints",
  "us_state", "opendata",
  "https://data.colorado.gov/resource/emiz-7jkv.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"co\",\"opendata\",\"opendata\"]", 21600,
  "Socrata open-data feed from Colorado: Boulder County Building Footprints, one JSON record per row.");

VGEO(nam_ct_covid_19_reported_patient_impact_and_hos, "nam-ct-covid-19-reported-patient-impact-and-hos", "Connecticut — COVID-19 Reported Patient Impact and Hospital Capacity by Facility", "Connecticut — COVID-19 Reported Patient Impact and Hospital Capacity by Facility",
  "us_state", "health",
  "https://data.ct.gov/resource/vvtn-9xef.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ct\",\"health\",\"opendata\"]", 21600,
  "Socrata open-data feed from Connecticut: COVID-19 Reported Patient Impact and Hospital Capacity by Facility, one JSON record per row.");

VGEO(nam_ct_education_directory, "nam-ct-education-directory", "Connecticut — Education Directory", "Connecticut — Education Directory",
  "us_state", "opendata",
  "https://data.ct.gov/resource/9k2y-kqxn.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ct\",\"opendata\",\"opendata\"]", 21600,
  "Socrata open-data feed from Connecticut: Education Directory, one JSON record per row.");

VGEO(nam_delaware_delaware_public_education_organization_d, "nam-delaware-delaware-public-education-organization-d", "Delaware — Delaware Public Education Organization Directory", "Delaware — Delaware Public Education Organization Directory",
  "us_state", "opendata",
  "https://data.delaware.gov/resource/p3ez-si4g.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"de\",\"opendata\",\"opendata\"]", 86400,
  "Socrata open-data feed from Delaware: Delaware Public Education Organization Directory, one JSON record per row.");

AGSDIR(nam_delaware_firstmap_services, "nam-delaware-firstmap-services", "Delaware FirstMap — ArcGIS service directory", "Delaware FirstMap — ArcGIS service directory",
  "opendata",
  "https://enterprise.firstmap.delaware.gov/arcgis/rest/services",
  "[\"usa\",\"de\",\"gis\",\"arcgis\"]", 86400,
  "Delaware's statewide FirstMap ArcGIS server. Walks all 12 folders the server root names (Basemaps, Biota, Boundaries, Elevation, Environmental, Geology, Hydrology, Location, PlanningCadastre, Society, Transportation, Utilities) and emits every service in each; the root's own services array is empty.");

VGEO(nam_delaware_permitted_septic_systems, "nam-delaware-permitted-septic-systems", "Delaware — Permitted Septic Systems", "Delaware — Permitted Septic Systems",
  "us_state", "opendata",
  "https://data.delaware.gov/resource/mv7j-tx3u.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"de\",\"opendata\",\"opendata\"]", 21600,
  "Socrata open-data feed from Delaware: Permitted Septic Systems, one JSON record per row.");

#include "lib/arcgis_dir.h"

/* nam-fl-fdot-services / nam-indiana-gis-services / nam-nc-onemap-services:
 * each row emitted the directory ROOT's "services" array only, so every service
 * inside a folder was never fetched. Measured 2026-09-14 (root + in folders):
 * FDOT 75 + 73, Indiana 72 + 867, NC OneMap 46 + 90. lib/arcgis_dir.c walks the
 * root and every folder. */
static int run_nam_fl_fdot_services(const source_ctx *c, intel_sink *s) {
  return arcgis_dir_walk(c, s, "nam-fl-fdot-services",
                         "https://gis.fdot.gov/arcgis/rest/services",
                         "transport", "en", "[\"usa\",\"fl\",\"gis\",\"transport\"]");
}
static const source_def nam_fl_fdot_services = {
  .id = "nam-fl-fdot-services", .collector = "us_state",
  .name = "Florida DOT — ArcGIS service directory",
  .name_ja = "Florida DOT — ArcGIS service directory",
  .update_interval_sec = 86400, .run = run_nam_fl_fdot_services,
  .category = "transport", .type = "api",
  .url = "https://gis.fdot.gov/arcgis/rest/services?f=json",
  .description = "Every Florida Department of Transportation public GIS service, root and folders.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(nam_fl_fdot_services);

static int run_nam_indiana_gis_services(const source_ctx *c, intel_sink *s) {
  return arcgis_dir_walk(c, s, "nam-indiana-gis-services",
                         "https://gisdata.in.gov/server/rest/services",
                         "opendata", "en", "[\"usa\",\"in\",\"gis\",\"arcgis\"]");
}
static const source_def nam_indiana_gis_services = {
  .id = "nam-indiana-gis-services", .collector = "us_state",
  .name = "Indiana GIS — ArcGIS service directory",
  .name_ja = "Indiana GIS — ArcGIS service directory",
  .update_interval_sec = 86400, .run = run_nam_indiana_gis_services,
  .category = "opendata", .type = "api",
  .url = "https://gisdata.in.gov/server/rest/services?f=json",
  .description = "Every public statewide Indiana ArcGIS service, root and folders.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(nam_indiana_gis_services);

AGSDIR(nam_mass_gis_services, "nam-mass-gis-services", "MassGIS — ArcGIS service directory", "MassGIS — ArcGIS service directory",
  "opendata",
  "https://arcgisserver.digital.mass.gov/arcgisserver/rest/services",
  "[\"usa\",\"ma\",\"gis\",\"arcgis\"]", 86400,
  "Massachusetts statewide GIS server. Walks all 24 folders the server root names (AGOL, Basemaps, DCAM, DCR, DEP, DFG, DHCD, DOEServices, DPH, DPS, DUA, EOLWD, EPSServices, FEMA, FWE, GeocodeServicesArcMap, HealthConnector, Legislature, LiDAR, MEMA, NHESP, PublicSafety, Transportation, Utilities) plus the geocode services listed at the root, and emits every service in each.");

AGSDIR(nam_md_imap_services, "nam-md-imap-services", "Maryland iMAP — ArcGIS service directory", "Maryland iMAP — ArcGIS service directory",
  "opendata",
  "https://mdgeodata.md.gov/imap/rest/services",
  "[\"usa\",\"md\",\"gis\",\"arcgis\"]", 86400,
  "Maryland's statewide iMAP ArcGIS server. Walks all 24 folders the server root names (Agriculture through Weather) and emits every service in each, e.g. Transportation/MD_AnnualAverageDailyTraffic as both FeatureServer and MapServer; the root's own services array is empty.");

VGEO(nam_md_md_bepscoveredbuildings_maryland_beps_covered_buil, "nam-md-md-bepscoveredbuildings-maryland-beps-covered-buil", "Maryland iMAP — Maryland BEPS Covered Buildings (MD BEPSCoveredBuildings)", "Maryland iMAP — Maryland BEPS Covered Buildings (MD BEPSCoveredBuildings)",
  "us_state", "opendata",
  "https://mdgeodata.md.gov/imap/rest/services/Environment/MD_BEPSCoveredBuildings/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"md\",\"opendata\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Maryland iMAP: Maryland BEPS Covered Buildings, returned as GeoJSON with per-feature geometry.");

VGEO(nam_md_md_censusdata_census_tracts, "nam-md-md-censusdata-census-tracts", "Maryland iMAP — Census Tracts (MD CensusData)", "Maryland iMAP — Census Tracts (MD CensusData)",
  "us_state", "opendata",
  "https://mdgeodata.md.gov/imap/rest/services/Demographics/MD_CensusData/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"md\",\"opendata\",\"gis\",\"arcgis\"]", 86400,
  "ArcGIS FeatureServer layer from Maryland iMAP: Census Tracts, returned as GeoJSON with per-feature geometry.");

VGEO(nam_md_md_chesapeakebaydeadzones_chesapeake_bay_dead_zone, "nam-md-md-chesapeakebaydeadzones-chesapeake-bay-dead-zone", "Maryland iMAP — Chesapeake Bay Dead Zones (MD ChesapeakeBayDeadZones)", "Maryland iMAP — Chesapeake Bay Dead Zones (MD ChesapeakeBayDeadZones)",
  "us_state", "opendata",
  "https://mdgeodata.md.gov/imap/rest/services/Environment/MD_ChesapeakeBayDeadZones/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"md\",\"opendata\",\"gis\",\"arcgis\"]", 86400,
  "ArcGIS FeatureServer layer from Maryland iMAP: Chesapeake Bay Dead Zones, returned as GeoJSON with per-feature geometry.");

VGEO(nam_md_md_coastalresiliencyassessment_priority_shoreline, "nam-md-md-coastalresiliencyassessment-priority-shoreline", "Maryland iMAP — Priority Shoreline Areas (MD CoastalResiliencyAssessment)", "Maryland iMAP — Priority Shoreline Areas (MD CoastalResiliencyAssessment)",
  "us_state", "environment",
  "https://mdgeodata.md.gov/imap/rest/services/Environment/MD_CoastalResiliencyAssessment/FeatureServer/2/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"md\",\"environment\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Maryland iMAP: Priority Shoreline Areas, returned as GeoJSON with per-feature geometry.");

VGEO(nam_md_md_criticalareas_critical_areas_counties, "nam-md-md-criticalareas-critical-areas-counties", "Maryland iMAP — Critical Areas Counties (MD CriticalAreas)", "Maryland iMAP — Critical Areas Counties (MD CriticalAreas)",
  "us_state", "opendata",
  "https://mdgeodata.md.gov/imap/rest/services/Environment/MD_CriticalAreas/FeatureServer/1/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"md\",\"opendata\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Maryland iMAP: Critical Areas Counties, returned as GeoJSON with per-feature geometry.");

VGEO(nam_md_md_criticalareas_critical_areas_towns, "nam-md-md-criticalareas-critical-areas-towns", "Maryland iMAP — Critical Areas Towns (MD CriticalAreas)", "Maryland iMAP — Critical Areas Towns (MD CriticalAreas)",
  "us_state", "opendata",
  "https://mdgeodata.md.gov/imap/rest/services/Environment/MD_CriticalAreas/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"md\",\"opendata\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Maryland iMAP: Critical Areas Towns, returned as GeoJSON with per-feature geometry.");

VGEO(nam_md_md_focalareas_targeted_ecological_areas, "nam-md-md-focalareas-targeted-ecological-areas", "Maryland iMAP — Targeted Ecological Areas (MD FocalAreas)", "Maryland iMAP — Targeted Ecological Areas (MD FocalAreas)",
  "us_state", "environment",
  "https://mdgeodata.md.gov/imap/rest/services/Environment/MD_FocalAreas/FeatureServer/1/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"md\",\"environment\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Maryland iMAP: Targeted Ecological Areas, returned as GeoJSON with per-feature geometry.");

VGEO(nam_md_md_nutrientmanagementsetbacksfromwaterways_rivers, "nam-md-md-nutrientmanagementsetbacksfromwaterways-rivers", "Maryland iMAP — Rivers Streams and Ditches (MD NutrientManagementSetbacksFromWaterways)", "Maryland iMAP — Rivers Streams and Ditches (MD NutrientManagementSetbacksFromWaterways)",
  "us_state", "environment",
  "https://mdgeodata.md.gov/imap/rest/services/Agriculture/MD_NutrientManagementSetbacksFromWaterways/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"md\",\"environment\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Maryland iMAP: Rivers Streams and Ditches, returned as GeoJSON with per-feature geometry.");

VGEO(nam_md_md_pointsourcedischarges_point_source_discharges, "nam-md-md-pointsourcedischarges-point-source-discharges", "Maryland iMAP — Point Source Discharges (MD PointSourceDischarges)", "Maryland iMAP — Point Source Discharges (MD PointSourceDischarges)",
  "us_state", "opendata",
  "https://mdgeodata.md.gov/imap/rest/services/Environment/MD_PointSourceDischarges/FeatureServer/1/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"md\",\"opendata\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Maryland iMAP: Point Source Discharges, returned as GeoJSON with per-feature geometry.");

VGEO(nam_md_md_protectedlands_md_environmental_trust_easements, "nam-md-md-protectedlands-md-environmental-trust-easements", "Maryland iMAP — MD Environmental Trust Easements (MD ProtectedLands)", "Maryland iMAP — MD Environmental Trust Easements (MD ProtectedLands)",
  "us_state", "environment",
  "https://mdgeodata.md.gov/imap/rest/services/Environment/MD_ProtectedLands/FeatureServer/2/query?where=1%3D1&outFields=*&outSR=4326&f=geojson&resultRecordCount=200",
  "en", "[\"usa\",\"md\",\"environment\",\"gis\",\"arcgis\"]", 21600,
  "ArcGIS FeatureServer layer from Maryland iMAP: MD Environmental Trust Easements, returned as GeoJSON with per-feature geometry.");

VGEO(nam_michigan_annual_hospitalization_by_county_filtere, "nam-michigan-annual-hospitalization-by-county-filtere", "Michigan — Annual Hospitalization by County, FILTERED", "Michigan — Annual Hospitalization by County, FILTERED",
  "us_state", "health",
  "https://data.michigan.gov/resource/sktc-hg8i.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"mi\",\"health\",\"opendata\"]", 21600,
  "Socrata open-data feed from Michigan: Annual Hospitalization by County, FILTERED, one JSON record per row.");

VGEO(nam_michigan_hospitalization_by_county, "nam-michigan-hospitalization-by-county", "Michigan — Hospitalization by County", "Michigan — Hospitalization by County",
  "us_state", "health",
  "https://data.michigan.gov/resource/xjme-bs8s.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"mi\",\"health\",\"opendata\"]", 21600,
  "Socrata open-data feed from Michigan: Hospitalization by County, one JSON record per row.");

VGEO(nam_michigan_map_annual_hospitalization, "nam-michigan-map-annual-hospitalization", "Michigan — Map: Annual Hospitalization", "Michigan — Map: Annual Hospitalization",
  "us_state", "health",
  "https://data.michigan.gov/resource/5g37-kq23.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"mi\",\"health\",\"opendata\"]", 21600,
  "Socrata open-data feed from Michigan: Map: Annual Hospitalization, one JSON record per row.");

VGEO(nam_michigan_mdot_bureau_of_bridges_and_structures_co, "nam-michigan-mdot-bureau-of-bridges-and-structures-co", "Michigan — MDOT Bureau of Bridges and Structures - Common Bridge Inventory Items - Includes MDOT Owned Bridges and Local Agency NBI Bridges with Size and Design Info", "Michigan — MDOT Bureau of Bridges and Structures - Common Bridge Inventory Items - Includes MDOT Owned Bridges and Local Agency NBI Bridges with Size and Design Info",
  "us_state", "transport",
  "https://data.michigan.gov/resource/6rbe-zjpu.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"mi\",\"transport\",\"opendata\"]", 86400,
  "Socrata open-data feed from Michigan: MDOT Bureau of Bridges and Structures - Common Bridge Inventory Items - Includes MDOT Owned Bridges and Local Agency NBI Bridges with Size and Design Info, one JSON record per row.");

static int run_nam_nc_onemap_services(const source_ctx *c, intel_sink *s) {
  return arcgis_dir_walk(c, s, "nam-nc-onemap-services",
                         "https://services.nconemap.gov/secure/rest/services",
                         "opendata", "en", "[\"usa\",\"nc\",\"gis\",\"arcgis\"]");
}
static const source_def nam_nc_onemap_services = {
  .id = "nam-nc-onemap-services", .collector = "us_state",
  .name = "NC OneMap — ArcGIS service directory",
  .name_ja = "NC OneMap — ArcGIS service directory",
  .update_interval_sec = 86400, .run = run_nam_nc_onemap_services,
  .category = "opendata", .type = "api",
  .url = "https://services.nconemap.gov/secure/rest/services?f=json",
  .description = "Every North Carolina statewide OneMap ArcGIS service, root and folders.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(nam_nc_onemap_services);

AGSDIR(nam_nh_geodata_services, "nam-nh-geodata-services", "NH GRANIT — ArcGIS service directory", "NH GRANIT — ArcGIS service directory",
  "opendata",
  "https://nhgeodata.unh.edu/nhgeodata/rest/services",
  "[\"usa\",\"nh\",\"gis\",\"arcgis\"]", 86400,
  "New Hampshire's GRANIT statewide geodata server. Walks all 15 folders the server root names (APB, BE, CAD, CSD, EC, EDP, GG, Hosted, IBM, IWR, LGN, OC, TN, Topical, Utilities) and emits every service in each; the root itself lists only WPPT_PredictedMarshMigration.");

VGEO(nam_pa_covid_19_unusable_vaccine_current_county, "nam-pa-covid-19-unusable-vaccine-current-county", "Pennsylvania — COVID-19 Unusable Vaccine Current County Health", "Pennsylvania — COVID-19 Unusable Vaccine Current County Health",
  "us_state", "health",
  "https://data.pa.gov/resource/r5v4-zbqw.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"pa\",\"health\",\"opendata\"]", 3600,
  "Socrata open-data feed from Pennsylvania: COVID-19 Unusable Vaccine Current County Health, one JSON record per row.");

VGEO(nam_texas_texas_commission_on_environmental_qualit_2, "nam-texas-texas-commission-on-environmental-qualit-2", "Texas — Texas Commission on Environmental Quality - Texas Water Districts", "Texas — Texas Commission on Environmental Quality - Texas Water Districts",
  "us_state", "environment",
  "https://data.texas.gov/resource/hr84-s96f.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"tx\",\"environment\",\"opendata\"]", 86400,
  "Socrata open-data feed from Texas: Texas Commission on Environmental Quality - Texas Water Districts, one JSON record per row.");

VGEO(nam_texas_texas_commission_on_environmental_qualit_3, "nam-texas-texas-commission-on-environmental-qualit-3", "Texas — Texas Commission on Environmental Quality - Administrative Orders Issued", "Texas — Texas Commission on Environmental Quality - Administrative Orders Issued",
  "us_state", "environment",
  "https://data.texas.gov/resource/u66a-qggj.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"tx\",\"environment\",\"opendata\"]", 21600,
  "Socrata open-data feed from Texas: Texas Commission on Environmental Quality - Administrative Orders Issued, one JSON record per row.");

VGEO(nam_texas_texas_commission_on_environmental_qualit_4, "nam-texas-texas-commission-on-environmental-qualit-4", "Texas — Texas Commission on Environmental Quality - Notices Of Violation (NOV)", "Texas — Texas Commission on Environmental Quality - Notices Of Violation (NOV)",
  "us_state", "environment",
  "https://data.texas.gov/resource/mwzi-gyw7.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"tx\",\"environment\",\"opendata\"]", 21600,
  "Socrata open-data feed from Texas: Texas Commission on Environmental Quality - Notices Of Violation (NOV), one JSON record per row.");

VGEO(nam_texas_texas_commission_on_environmental_qualit_5, "nam-texas-texas-commission-on-environmental-qualit-5", "Texas — Texas Commission on Environmental Quality - Operating Dry Cleaner Registrations", "Texas — Texas Commission on Environmental Quality - Operating Dry Cleaner Registrations",
  "us_state", "environment",
  "https://data.texas.gov/resource/qfph-9bnd.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"tx\",\"environment\",\"opendata\"]", 21600,
  "Socrata open-data feed from Texas: Texas Commission on Environmental Quality - Operating Dry Cleaner Registrations, one JSON record per row.");

VGEO(nam_texas_texas_commission_on_environmental_qualit_6, "nam-texas-texas-commission-on-environmental-qualit-6", "Texas — Texas Commission on Environmental Quality - Petroleum Storage Tank (PST) Delivery Certificates", "Texas — Texas Commission on Environmental Quality - Petroleum Storage Tank (PST) Delivery Certificates",
  "us_state", "environment",
  "https://data.texas.gov/resource/jx8f-z4hu.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"tx\",\"environment\",\"opendata\"]", 3600,
  "Socrata open-data feed from Texas: Texas Commission on Environmental Quality - Petroleum Storage Tank (PST) Delivery Certificates, one JSON record per row.");

VJSON(nam_utah_gis_services, "nam-utah-gis-services", "Utah — ArcGIS Online organisation service directory", "Utah — ArcGIS Online organisation service directory",
  "us_state", "opendata",
  "https://services.arcgis.com/ZzrwjTRez6FJiOq4/arcgis/rest/services?f=json",
  "services",
  "en", "[\"usa\",\"ut\",\"gis\",\"arcgis\"]", 86400,
  "Index of the State of Utah's hosted feature services on ArcGIS Online.");
