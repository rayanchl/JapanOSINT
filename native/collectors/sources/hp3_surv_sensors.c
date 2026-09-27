/* collectors/sources/hp3_surv_sensors.c — public sensing and monitoring networks.
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
 * coordinates, and every row that needs a credential declares it. */
#include "../../lib/hpengine.h"

static const hp_source HP3_SURV_SENSORS[] = {
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
    .page_param = "page", .page_size = 1000, .page_max = 30,
    .interval = 3600,
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
    .interval = 3600,
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
    .page_param = "page", .page_size = 1000, .page_max = 30,
    .interval = 3600,
    .description = "The open radiation dataset founded after Fukushima — every "
      "uploaded measurement with coordinates, device, unit and capture time. "
      "Over a hundred million points, densest in Japan, and the only "
      "independent check on official dose-rate reporting" },

  { .id = "EU_EURDEP_RADIOLOGICAL", .name = "EURDEP — European radiological data exchange",
    .name_ja = "欧州 放射線データ交換", .category = "surveillance",
    .portal = "https://remap.jrc.ec.europa.eu", .record_type = "radiation-station",
    .tags = "\"eu\",\"radiation\",\"sensor\",\"nuclear\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://remap.jrc.ec.europa.eu/Simple.aspx?q={q}",
    .base = "https://remap.jrc.ec.europa.eu", .filter_query = 1,
    .description = "The European radiological monitoring platform — around "
      "5,000 national dose-rate stations reporting hourly, with the operating "
      "country, the station identifier and its coordinates. The network that "
      "detects a release before any government announces one" },

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
    .filter_query = 1, .title_keys = "col1,col5", .id_keys = "col1",
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
  { .id = "WMO_OSCAR_STATIONS", .name = "WMO OSCAR/Surface — official observing stations",
    .name_ja = "WMO 観測所登録", .category = "surveillance",
    .portal = "https://oscar.wmo.int", .record_type = "weather-station",
    .tags = "\"sensor\",\"weather\",\"multilateral\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://oscar.wmo.int/surface/#/search/station?q={q}",
    .base = "https://oscar.wmo.int", .filter_query = 1,
    .description = "The World Meteorological Organization's authoritative "
      "registry of surface observing stations — WIGOS identifier, the operating "
      "national service, the programmes it reports to, the instruments "
      "installed and the coordinates" },

  /* ── Space, fire and earth observation ────────────────────────────────── */
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
    .interval = 3600,
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

  { .id = "IGS_GNSS_STATIONS", .name = "IGS — global GNSS reference station network",
    .name_ja = "国際GNSS事業 基準局", .category = "surveillance",
    .portal = "https://network.igs.org", .record_type = "gnss-station",
    .tags = "\"sensor\",\"gnss\",\"geodesy\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://network.igs.org/?q={q}",
    .base = "https://network.igs.org", .filter_query = 1,
    .description = "The International GNSS Service station network — the "
      "operating agency, receiver and antenna model, the satellite systems "
      "tracked and the precise coordinates. GNSS reference stations are also "
      "the standing detection network for interference and spoofing" },

  { .id = "GDACS_DISASTER_ALERTS", .name = "GDACS — global disaster alert & coordination",
    .name_ja = "GDACS 災害警報", .category = "surveillance",
    .portal = "https://www.gdacs.org", .record_type = "disaster-alert",
    .tags = "\"disaster\",\"monitoring\",\"multilateral\"", .free_tier = 1,
    .url = "https://www.gdacs.org/gdacsapi/api/events/geteventlist/SEARCH",
    .array_path = "features", .filter_query = 1,
    .title_keys = "properties.name,properties.eventtype",
    .id_keys = "properties.eventid", .date_keys = "properties.fromdate",
    .interval = 3600,
    .description = "The joint UN and European Commission disaster alert system "
      "— earthquakes, cyclones, floods, volcanoes and drought with the "
      "estimated population affected, the alert level and the coordination "
      "reporting from responding agencies" },
};

HP_REGISTER_TABLE(HP3_SURV_SENSORS)
