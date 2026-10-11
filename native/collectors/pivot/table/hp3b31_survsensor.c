/* collectors/pivot/table/hp3b31_survsensor.c — batch 31: survsensor — public sensing and monitoring networks.
 *
 * The other half of state and civic surveillance is not cameras: it is the
 * standing instrument networks. Seismometers, radiation monitors, air-quality
 * stations, river gauges, weather masts, GNSS reference stations, fire
 * detection satellites and the space-object catalogue. Each publishes both a
 * *station inventory* (what exists, where, operated by whom, since when) and a
 * *measurement stream*.
 *
 * The inventory is what this file is mostly after. A station list is an
 * infrastructure map with an operator attached, and operators are institutions
 * — which makes these feeds a way to place a named agency, university or
 * company at a precise coordinate, independent of any register.
 *
 * Everything here emits geolocated records where the upstream provides
 * coordinates, and every row that needs a credential declares it.
 *
 * PROVENANCE AND VERIFICATION STATUS — read before trusting a row here.
 *
 * Hand-authored in C, not scaffolded by tools/gen_hp_batch.py. This file is
 * the maintained copy. Since 2026-10-05 a manifest RECONSTRUCTED from it exists
 * at docs/candidate-sources-batch31.<beat>.txt, so the manifest-driven gates can
 * finally be pointed at batch 31. It is a record, not a source: never
 * regenerate this file from it. Its fidelity was proven rather than assumed —
 * gen_hp_batch.py run on the manifest reproduces every field of every row here
 * exactly, and a grep of field assignments agrees on both sides.
 *
 * These rows are NOT proof-of-life verified. No row was fetched over the wire,
 * so rules 4, 4b and 4d (fetching is not emitting; emitting is not storing;
 * answering is not answering THE QUESTION) are unmeasured. What HAS been
 * checked is offline and structural: clean build, zero audit-sources findings,
 * no duplicate id or endpoint, every row reachable (rule 3), and every paged
 * endpoint declares its walk (audit_batch_pagination, first run 2026-10-05).
 *
 * With network, run the MANIFEST-driven tools:
 *     python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch31.*.txt --check-filter
 *     python3 tools/audit_batch_emit.py ../docs/candidate-sources-batch31.*.txt \
 *             --bin ./bin/japanosint --jobs 6
 * NOT `audit_registry_emit.py --match hp3b31`, which this header used to
 * suggest: --match is a regex on the source ID, these ids carry no batch
 * prefix, so it selects zero rows and reports nothing wrong.
 * Rows with key_env need their key in the environment, and rows with
 * post_body are POSTs that probe_hp_batch does not send — judge those two
 * classes with audit_batch_emit. Retire whatever comes back EMITS_NOTHING,
 * DROPS_EVERYTHING, COLLISION or FILTER_IGNORED. The engine cannot fabricate:
 * an endpoint that moved yields an honest empty, never an invented record.
 */
#include "lib/hpengine.h"

static const hp_source HP3B31_SURVSENSOR[] = {
  /* ── Air quality ──────────────────────────────────────────────────────── */
  { .id = "OPENAQ_LOCATIONS", .name = "OpenAQ — global air quality station inventory",
    .name_ja = "OpenAQ 大気観測局", .category = "surveillance",
    .portal = "https://openaq.org", .record_type = "air-station",
    .tags = "\"sensor\",\"air-quality\",\"reference\"", .key_env = "OPENAQ_API_KEY",
    .free_tier = 1,
    .url = "https://api.openaq.org/v3/locations?limit=1000",
    .headers = { "X-API-Key: {key}", NULL },
    .array_path = "results", .filter_query = 1,
    .title_keys = "name,country.name", .id_keys = "id",
    .lat_key = "coordinates.latitude", .lon_key = "coordinates.longitude",
    .page_param = "page", .page_max = 30,
    .interval = 86400,
    .description = "Reference-grade government monitoring stations aggregated "
      "across more than a hundred countries — the operating authority, the "
      "instrument, the parameters measured and the first and last observation "
      "dates, which reveal when a country stopped reporting" },

  { .id = "PURPLEAIR_SENSOR_INDEX", .name = "PurpleAir — private air sensor index",
    .name_ja = "PurpleAir センサ一覧", .category = "surveillance",
    .portal = "https://api.purpleair.com", .record_type = "air-sensor",
    .tags = "\"sensor\",\"air-quality\",\"citizen\"", .key_env = "PURPLEAIR_API_KEY",
    .free_tier = 0,
    .url = "https://api.purpleair.com/v1/sensors?fields=name,latitude,longitude,"
      "altitude,location_type,pm2.5,humidity,temperature,last_seen,model,hardware,"
      "private,confidence",
    .headers = { "X-API-Key: {key}", NULL },
    .array_path = "data", .filter_query = 1,
    .title_keys = "name", .id_keys = "sensor_index",
    .interval = 86400,
    .description = "PurpleAir's sensor index with an explicit field list so the "
      "full record is returned — device name (often a household or business "
      "name), indoor or outdoor placement, hardware revision, last-seen time "
      "and coordinates" },

  { .id = "EEA_AIR_QUALITY_STATIONS", .name = "EEA — European air quality station metadata",
    .name_ja = "欧州環境機関 大気観測局", .category = "surveillance",
    .portal = "https://discomap.eea.europa.eu", .record_type = "air-station",
    .tags = "\"eu\",\"sensor\",\"air-quality\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://discomap.eea.europa.eu/App/AQViewer/index.html?fqn={q}",
    .base = "https://discomap.eea.europa.eu", .filter_query = 1,
    .description = "The European Environment Agency's station registry — every "
      "official EU monitoring point with its classification (traffic, "
      "industrial, background), the responsible national authority, the "
      "pollutants measured and the compliance assessment attached to it" },

  /* ── Radiation ────────────────────────────────────────────────────────── */
  { .id = "SAFECAST_MEASUREMENTS", .name = "Safecast — open radiation measurement network",
    .name_ja = "Safecast 放射線測定網", .category = "surveillance",
    .portal = "https://api.safecast.org", .record_type = "radiation-measurement",
    .tags = "\"sensor\",\"radiation\",\"citizen\",\"japan\"", .free_tier = 1,
    .url = "https://api.safecast.org/measurements.json?per_page=1000",
    .filter_query = 1, .title_keys = "location_name,device_id",
    .id_keys = "id", .date_keys = "captured_at",
    .lat_key = "latitude", .lon_key = "longitude",
    .page_param = "page", .page_max = 30,
    .interval = 3600,
    .description = "The open radiation dataset founded after Fukushima — every "
      "uploaded measurement with coordinates, device, unit and capture time. "
      "Over a hundred million points, densest in Japan, and the only "
      "independent check on official dose-rate reporting" },

  { .id = "JP_NRA_RADIATION_MONITORING", .name = "Japan NRA — environmental radiation monitoring",
    .name_ja = "原子力規制委員会 放射線モニタリング", .category = "surveillance",
    .portal = "https://radioactivity.nra.go.jp", .record_type = "radiation-station",
    .tags = "\"jp\",\"radiation\",\"sensor\",\"nuclear\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://radioactivity.nra.go.jp/map/?q={q}",
    .base = "https://radioactivity.nra.go.jp", .filter_query = 1,
    .description = "Japan's Nuclear Regulation Authority monitoring post "
      "network — every prefectural and site-boundary dose-rate post around the "
      "nuclear plants, with location, operator and the published readings" },

  /* ── Seismic, volcanic and geophysical ────────────────────────────────── */
  { .id = "FDSN_STATION_INVENTORY", .name = "FDSN — global seismic station inventory",
    .name_ja = "世界地震観測局 一覧", .category = "surveillance",
    .portal = "https://service.iris.edu", .record_type = "seismic-station",
    .tags = "\"sensor\",\"seismic\",\"infrastructure\"", .free_tier = 1,
    .type = "dataset", .mode = HP_CSV,
    .url = "https://service.iris.edu/fdsnws/station/1/query?level=station"
      "&format=text&nodata=404",
    /* FDSN `format=text` is PIPE-delimited with a `#` header line. The engine's
     * CSV delimiter defaults to "," and does not sniff (lib/hpengine.c, the
     * `char delim[64] = ","` above the csv_delim switch), so without these
     * three opts the whole line landed in a single cell: col1/col2/col3/col5
     * all resolved to nothing, the title fell back to the raw line, and the
     * row stored 151,303 records carrying no parsed field and no coordinates.
     * The count was real, which is exactly why it looked fine. */
    .csv_delim = "pipe", .csv_comment = "#", .csv_no_header = 1,
    /* Station CODE is not unique across networks (CI.PAS and GR.PAS are
     * different stations), and an epoch re-opens under the same code, so
     * identity is network+station+start — `col1` alone collapsed them. */
    .filter_query = 1, .title_keys = "col1,col5", .id_keys = "col0+col1+col6",
    .lat_key = "col2", .lon_key = "col3",
    .interval = 86400,
    .description = "The federated seismic network's station list — network "
      "code, station code, coordinates, elevation, the operating institution "
      "and the start and end dates of operation. A global map of who runs "
      "geophysical instrumentation and where" },

  { .id = "SMITHSONIAN_VOLCANOES", .name = "Smithsonian GVP — volcano & eruption record",
    .name_ja = "スミソニアン 火山データベース", .category = "surveillance",
    .portal = "https://volcano.si.edu", .record_type = "volcano",
    .tags = "\"hazard\",\"volcano\",\"monitoring\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://volcano.si.edu/search_volcano_results.cfm?vname={q}",
    .base = "https://volcano.si.edu", .filter_query = 1,
    .description = "The Global Volcanism Program's catalogue — every Holocene "
      "volcano with its coordinates, type, last known eruption, the weekly "
      "activity reports and the observatory responsible for monitoring it" },

  /* ── Water, ocean and hydrology ───────────────────────────────────────── */
  { .id = "USGS_NWIS_SITE_INVENTORY", .name = "USGS NWIS — water monitoring site inventory",
    .name_ja = "米国地質調査所 水文観測点", .category = "surveillance",
    .portal = "https://waterservices.usgs.gov", .record_type = "water-station",
    .tags = "\"us\",\"sensor\",\"water\",\"hydrology\"", .free_tier = 1,
    .type = "dataset", .mode = HP_CSV,
    .url = "https://waterservices.usgs.gov/nwis/site/?format=rdb&stateCd=ca"
      "&siteOutput=expanded&siteStatus=all",
    .filter_query = 1, .title_keys = "station_nm,site_no", .id_keys = "site_no",
    .lat_key = "dec_lat_va", .lon_key = "dec_long_va",
    .interval = 86400,
    .description = "The USGS site file requested in expanded form — station "
      "name and number, coordinates, drainage area, aquifer, well depth, the "
      "agency operating it and the data types collected. The expanded output is "
      "requested explicitly rather than accepting the seven-column default" },

  { .id = "GLOBAL_WATER_QUALITY_PORTAL", .name = "Water Quality Portal — US monitoring stations",
    .name_ja = "米国 水質観測点", .category = "surveillance",
    .portal = "https://www.waterqualitydata.us", .record_type = "water-station",
    .tags = "\"us\",\"water\",\"environment\",\"sensor\"", .free_tier = 1,
    .type = "dataset", .mode = HP_CSV,
    .url = "https://www.waterqualitydata.us/data/Station/search?mimeType=csv"
      "&organization={q}",
    .filter_query = 1, .title_keys = "MonitoringLocationName,OrganizationFormalName",
    .id_keys = "MonitoringLocationIdentifier",
    .lat_key = "LatitudeMeasure", .lon_key = "LongitudeMeasure",
    .description = "The federated USGS, EPA and state water-quality station "
      "registry — the organisation operating each site, the water body, the "
      "site type and coordinates. Discharge-monitoring sites sit downstream of "
      "named industrial facilities" },

  /* ── Weather and atmosphere ───────────────────────────────────────────── */
  { .id = "NASA_FIRMS_FIRE_DETECTIONS", .name = "NASA FIRMS — satellite active fire detections",
    .name_ja = "NASA 衛星火災検知", .category = "surveillance",
    .portal = "https://firms.modaps.eosdis.nasa.gov", .record_type = "fire-detection",
    .tags = "\"satellite\",\"fire\",\"monitoring\"", .key_env = "FIRMS_MAP_KEY",
    .free_tier = 1, .type = "dataset", .mode = HP_CSV,
    .url = "https://firms.modaps.eosdis.nasa.gov/api/area/csv/{key}/"
      "VIIRS_SNPP_NRT/world/1",
    .filter_query = 1, .title_keys = "satellite,confidence",
    .id_keys = "acq_time", .date_keys = "acq_date",
    .lat_key = "latitude", .lon_key = "longitude",
    .interval = 1800,
    .description = "Near-real-time thermal anomaly detections from VIIRS — "
      "coordinates, brightness temperature, fire radiative power and "
      "confidence. Used far beyond wildfire: gas flaring, industrial thermal "
      "signatures and the destruction of structures all appear here" },

  { .id = "COPERNICUS_EMS_ACTIVATIONS", .name = "Copernicus EMS — emergency mapping activations",
    .name_ja = "コペルニクス 緊急マッピング", .category = "surveillance",
    .portal = "https://emergency.copernicus.eu", .record_type = "ems-activation",
    .tags = "\"eu\",\"satellite\",\"disaster\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://emergency.copernicus.eu/mapping/list-of-components/EMSR?q={q}",
    .base = "https://emergency.copernicus.eu", .filter_query = 1,
    .description = "Every activation of the EU's rapid satellite mapping "
      "service — the requesting authority, the event, the area of interest and "
      "the delivered damage-assessment and reference maps, released openly "
      "including for conflict-related activations" },

  { .id = "GDACS_DISASTER_ALERTS", .name = "GDACS — global disaster alert & coordination",
    .name_ja = "GDACS 災害警報", .category = "surveillance",
    .portal = "https://www.gdacs.org", .record_type = "disaster-alert",
    .tags = "\"disaster\",\"monitoring\",\"multilateral\"", .free_tier = 1,
    .url = "https://www.gdacs.org/gdacsapi/api/events/geteventlist/SEARCH",
    .array_path = "features", .filter_query = 1,
    .title_keys = "properties.name,properties.eventtype",
    .id_keys = "properties.eventid", .date_keys = "properties.fromdate",
    .interval = 1800,
    .description = "The joint UN and European Commission disaster alert system "
      "— earthquakes, cyclones, floods, volcanoes and drought with the "
      "estimated population affected, the alert level and the coordination "
      "reporting from responding agencies" },
};

HP_REGISTER_TABLE(HP3B31_SURVSENSOR)
