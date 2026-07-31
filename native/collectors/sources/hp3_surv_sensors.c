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
  { .id = "SENSOR_COMMUNITY_STATIONS", .name = "Sensor.Community — global citizen air sensor network",
    .name_ja = "Sensor.Community 大気センサ網", .category = "surveillance",
    .portal = "https://sensor.community", .record_type = "air-sensor",
    .tags = "\"sensor\",\"air-quality\",\"surveillance\",\"citizen\"",
    .free_tier = 1,
    .url = "https://data.sensor.community/static/v2/data.json",
    .filter_query = 1, .title_keys = "location.country,sensor.sensor_type.name",
    .id_keys = "id", .date_keys = "timestamp",
    .lat_key = "location.latitude", .lon_key = "location.longitude",
    .description = "The full five-minute snapshot of the open citizen sensor "
      "network — every active particulate, temperature, humidity and noise "
      "sensor worldwide with its coordinates, sensor model and current "
      "readings. Tens of thousands of devices, no key, no cap" },

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
    .description = "The federated seismic network's station list — network "
      "code, station code, coordinates, elevation, the operating institution "
      "and the start and end dates of operation. A global map of who runs "
      "geophysical instrumentation and where" },

  { .id = "EMSC_SEISMIC_EVENTS", .name = "EMSC — European-Mediterranean seismic events",
    .name_ja = "欧州地中海地震センター 地震情報", .category = "surveillance",
    .portal = "https://www.seismicportal.eu", .record_type = "seismic-event",
    .tags = "\"sensor\",\"seismic\",\"hazard\"", .free_tier = 1,
    .url = "https://www.seismicportal.eu/fdsnws/event/1/query?format=json&limit=1000",
    .array_path = "features", .filter_query = 1,
    .title_keys = "properties.flynn_region,properties.magtype",
    .id_keys = "id", .date_keys = "properties.time",
    .lat_key = "properties.lat", .lon_key = "properties.lon",
    .description = "Located seismic events with magnitude, depth, region, "
      "contributing agency and the authoritative solution — the feed used to "
      "distinguish a natural event from an explosion by depth and location" },

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
  { .id = "USGS_WATER_SITES", .name = "USGS NWIS — water monitoring site inventory",
    .name_ja = "米国地質調査所 水文観測点", .category = "surveillance",
    .portal = "https://waterservices.usgs.gov", .record_type = "water-station",
    .tags = "\"us\",\"sensor\",\"water\",\"hydrology\"", .free_tier = 1,
    .type = "dataset", .mode = HP_CSV,
    .url = "https://waterservices.usgs.gov/nwis/site/?format=rdb&stateCd=ca"
      "&siteOutput=expanded&siteStatus=all",
    .filter_query = 1, .title_keys = "station_nm,site_no", .id_keys = "site_no",
    .lat_key = "dec_lat_va", .lon_key = "dec_long_va",
    .description = "The USGS site file requested in expanded form — station "
      "name and number, coordinates, drainage area, aquifer, well depth, the "
      "agency operating it and the data types collected. The expanded output is "
      "requested explicitly rather than accepting the seven-column default" },

  { .id = "NOAA_NDBC_BUOYS", .name = "NOAA NDBC — marine buoy & station observations",
    .name_ja = "NOAA 海洋ブイ観測", .category = "surveillance",
    .portal = "https://www.ndbc.noaa.gov", .record_type = "marine-station",
    .tags = "\"sensor\",\"ocean\",\"weather\"", .free_tier = 1,
    .type = "dataset", .mode = HP_CSV, .csv_no_header = 1,
    .url = "https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt",
    .filter_query = 1, .title_keys = "col0", .id_keys = "col0",
    .lat_key = "col1", .lon_key = "col2",
    .description = "The latest observation from every National Data Buoy "
      "Center station — wind, pressure, wave height, sea temperature and the "
      "station's fixed position. Offshore buoys are the standing sensor network "
      "for maritime activity and storm tracking" },

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

  { .id = "NWS_STATIONS_API", .name = "US National Weather Service — observation stations API",
    .name_ja = "米国気象局 観測所API", .category = "surveillance",
    .portal = "https://api.weather.gov", .record_type = "weather-station",
    .tags = "\"us\",\"sensor\",\"weather\"", .free_tier = 1,
    .url = "https://api.weather.gov/stations?limit=500",
    .array_path = "features", .filter_query = 1,
    .title_keys = "properties.name,properties.stationIdentifier",
    .id_keys = "properties.stationIdentifier",
    .next_path = "pagination.next", .page_max = 40,
    .description = "Every NWS-recognised observing station as GeoJSON — "
      "identifier, name, elevation, time zone and the forecast zone and county "
      "it belongs to, with the observation endpoint behind each" },

  { .id = "NOAA_SPACE_WEATHER", .name = "NOAA SWPC — space weather observations & alerts",
    .name_ja = "NOAA 宇宙天気観測", .category = "surveillance",
    .portal = "https://services.swpc.noaa.gov", .record_type = "space-weather",
    .tags = "\"space\",\"sensor\",\"geomagnetic\"", .free_tier = 1,
    .url = "https://services.swpc.noaa.gov/products/alerts.json",
    .filter_query = 1, .title_keys = "message,product_id",
    .id_keys = "product_id", .date_keys = "issue_datetime",
    .description = "Geomagnetic storm, radio blackout and radiation storm "
      "alerts with the observing satellite and the affected frequency bands — "
      "the standard explanation to rule out before attributing an HF or GNSS "
      "outage to jamming" },

  /* ── Space, fire and earth observation ────────────────────────────────── */
  { .id = "CELESTRAK_SATELLITE_CATALOG", .name = "CelesTrak — satellite catalogue & orbital elements",
    .name_ja = "衛星軌道要素カタログ", .category = "surveillance",
    .portal = "https://celestrak.org", .record_type = "space-object",
    .tags = "\"space\",\"satellite\",\"surveillance\"", .free_tier = 1,
    .url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=json",
    .filter_query = 1, .title_keys = "OBJECT_NAME,OBJECT_ID",
    .id_keys = "NORAD_CAT_ID", .date_keys = "EPOCH",
    .description = "Every tracked active satellite with its NORAD catalogue "
      "number, international designator, epoch and full orbital element set. "
      "Requesting the active group returns the whole catalogue rather than one "
      "constellation, so an operator's entire fleet is visible at once" },

  { .id = "SATNOGS_GROUND_STATIONS", .name = "SatNOGS — open satellite ground station network",
    .name_ja = "SatNOGS 地上局ネットワーク", .category = "surveillance",
    .portal = "https://network.satnogs.org", .record_type = "ground-station",
    .tags = "\"space\",\"sensor\",\"radio\"", .free_tier = 1,
    .url = "https://network.satnogs.org/api/stations/?format=json",
    .filter_query = 1, .title_keys = "name,description", .id_keys = "id",
    .lat_key = "lat", .lon_key = "lng",
    .page_param = "page", .page_size = 25, .page_max = 60,
    .description = "The open ground-station network — each station's operator, "
      "coordinates, altitude, antenna configuration, supported frequency ranges "
      "and observation history. A public map of amateur and institutional "
      "satellite receiving capability" },

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
    .description = "Near-real-time thermal anomaly detections from VIIRS — "
      "coordinates, brightness temperature, fire radiative power and "
      "confidence. Used far beyond wildfire: gas flaring, industrial thermal "
      "signatures and the destruction of structures all appear here" },

  { .id = "STAC_SENTINEL_IMAGERY", .name = "Earth Search STAC — Sentinel & Landsat scene index",
    .name_ja = "衛星画像シーン検索", .category = "surveillance",
    .portal = "https://earth-search.aws.element84.com", .record_type = "satellite-scene",
    .tags = "\"satellite\",\"imagery\",\"remote-sensing\"", .free_tier = 1,
    .url = "https://earth-search.aws.element84.com/v1/search",
    .post_body = "{\"collections\":[\"sentinel-2-l2a\"],\"limit\":500,"
      "\"query\":{\"eo:cloud_cover\":{\"lt\":30}}}",
    .content_type = "application/json",
    .array_path = "features", .filter_query = 1,
    .title_keys = "id,collection", .id_keys = "id",
    .date_keys = "properties.datetime",
    .description = "The open STAC catalogue of Sentinel-2 and Landsat scenes "
      "on public cloud storage — scene identifier, acquisition time, cloud "
      "cover, tile and the direct asset URLs per band. Free imagery for any "
      "coordinate, with a full acquisition history" },

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
    .description = "The joint UN and European Commission disaster alert system "
      "— earthquakes, cyclones, floods, volcanoes and drought with the "
      "estimated population affected, the alert level and the coordination "
      "reporting from responding agencies" },
};

HP_REGISTER_TABLE(HP3_SURV_SENSORS)
