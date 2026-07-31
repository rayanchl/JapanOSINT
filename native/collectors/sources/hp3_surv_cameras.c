/* collectors/sources/hp3_surv_cameras.c — state-operated camera networks.
 *
 * Every developed country runs a large, publicly documented camera estate on
 * its road network, and most publish the camera inventory as an API: the site
 * identifier, the road and direction, the coordinates and the current image
 * URL. That inventory is the useful part. A single frame is a curiosity; the
 * inventory is a map of where the state can see, updated as cameras are added
 * and removed.
 *
 * These are the transport agencies' own feeds, not third-party webcam
 * aggregators (those live in `cam_*.c`). Several are open without a key —
 * Caltrans, Singapore LTA, Finnish Digitraffic, Ontario and Alberta 511, NZTA —
 * and the rest declare `key_env`, so a missing credential yields an honest
 * empty rather than a fabricated camera list.
 *
 * The lat/lon keys are set wherever the upstream provides them, so each camera
 * lands as a geolocated record rather than a row of text. */
#include "../../lib/hpengine.h"

static const hp_source HP3_SURV_CAMERAS[] = {
  /* ── North America ────────────────────────────────────────────────────── */
  { .id = "CAM_CALTRANS_D3", .name = "Caltrans — California CCTV inventory (open)",
    .name_ja = "カリフォルニア州道路カメラ", .category = "surveillance",
    .portal = "https://cwwp2.dot.ca.gov", .record_type = "road-camera",
    .tags = "\"us\",\"camera\",\"traffic\",\"surveillance\"", .free_tier = 1,
    .url = "https://cwwp2.dot.ca.gov/data/d3/cctv/cctvStatusD03.json",
    .array_path = "data", .filter_query = 1,
    .title_keys = "cctv.location.locationName,cctv.location.route",
    .id_keys = "cctv.index",
    .lat_key = "cctv.location.latitude", .lon_key = "cctv.location.longitude",
    .description = "Caltrans publishes its CCTV status files district by "
      "district without authentication — camera index, route and postmile, "
      "county, coordinates, in-service flag and the streaming and still-image "
      "URLs for each device" },

  { .id = "CAM_WSDOT_HIGHWAY", .name = "WSDOT — Washington State highway cameras",
    .name_ja = "ワシントン州道路カメラ", .category = "surveillance",
    .portal = "https://wsdot.wa.gov", .record_type = "road-camera",
    .tags = "\"us\",\"camera\",\"traffic\",\"surveillance\"",
    .key_env = "WSDOT_ACCESS_CODE", .free_tier = 1,
    .url = "https://www.wsdot.wa.gov/Traffic/api/HighwayCameras/"
      "HighwayCamerasREST.svc/GetCamerasAsJson?AccessCode={key}",
    .title_keys = "Title,Description", .id_keys = "CameraID",
    .lat_key = "CameraLocation.Latitude", .lon_key = "CameraLocation.Longitude",
    .description = "Washington State DOT's camera inventory — camera ID, "
      "owner, road name, milepost, direction, coordinates and the image URL, "
      "for the full state network in one call" },

  { .id = "CAM_511NY_STATEWIDE", .name = "511NY — New York State camera & event feed",
    .name_ja = "ニューヨーク州道路カメラ", .category = "surveillance",
    .portal = "https://511ny.org", .record_type = "road-camera",
    .tags = "\"us\",\"camera\",\"traffic\",\"surveillance\"",
    .key_env = "NY511_API_KEY", .free_tier = 1,
    .url = "https://511ny.org/api/getcameras?key={key}&format=json",
    .title_keys = "Name,RoadwayName", .id_keys = "ID",
    .lat_key = "Latitude", .lon_key = "Longitude",
    .description = "New York State's 511 camera inventory covering the "
      "Thruway, state highways and the New York City metropolitan network — "
      "name, roadway, direction, coordinates, disabled flag and image URL" },

  { .id = "CAM_OHGO_OHIO", .name = "OHGO — Ohio DOT camera & incident API",
    .name_ja = "オハイオ州道路カメラ", .category = "surveillance",
    .portal = "https://publicapi.ohgo.com", .record_type = "road-camera",
    .tags = "\"us\",\"camera\",\"traffic\",\"surveillance\"",
    .key_env = "OHGO_API_KEY", .free_tier = 1,
    .url = "https://publicapi.ohgo.com/api/v1/cameras?page-size=500",
    .headers = { "Authorization: APIKEY {key}", NULL },
    .array_path = "results", .filter_query = 1,
    .title_keys = "description,location", .id_keys = "id",
    .lat_key = "latitude", .lon_key = "longitude",
    .page_param = "page-all", .page_size = 500, .page_max = 20,
    .description = "Ohio's traffic camera inventory with the direction, route "
      "and coordinates of each device, alongside the incident, construction and "
      "digital-sign feeds from the same API" },

  { .id = "CAM_COTRIP_COLORADO", .name = "COtrip — Colorado DOT camera inventory",
    .name_ja = "コロラド州道路カメラ", .category = "surveillance",
    .portal = "https://data.cotrip.org", .record_type = "road-camera",
    .tags = "\"us\",\"camera\",\"traffic\",\"surveillance\"",
    .key_env = "COTRIP_API_KEY", .free_tier = 1,
    .url = "https://data.cotrip.org/api/v1/cameras?apiKey={key}",
    .array_path = "features", .filter_query = 1,
    .title_keys = "properties.name,properties.direction",
    .id_keys = "properties.id",
    .description = "Colorado DOT's GeoJSON camera feed — every mountain-pass "
      "and interstate camera with its coordinates, direction and current view "
      "URL, in a state where the camera network doubles as the avalanche and "
      "closure monitoring system" },

  { .id = "CAM_511ON_ONTARIO", .name = "Ontario 511 — provincial road cameras",
    .name_ja = "オンタリオ州道路カメラ", .category = "surveillance",
    .portal = "https://511on.ca", .record_type = "road-camera",
    .tags = "\"ca\",\"camera\",\"traffic\",\"surveillance\"", .free_tier = 1,
    .url = "https://511on.ca/api/v2/get/cameras?format=json&lang=en",
    .filter_query = 1, .title_keys = "Name,Roadway", .id_keys = "Id",
    .lat_key = "Latitude", .lon_key = "Longitude",
    .description = "Ontario's provincial camera inventory, open without a key "
      "— device ID, roadway, direction, coordinates and image URL for the "
      "400-series highways and the northern Ontario network" },

  { .id = "CAM_511AB_ALBERTA", .name = "Alberta 511 — provincial road cameras",
    .name_ja = "アルバータ州道路カメラ", .category = "surveillance",
    .portal = "https://511.alberta.ca", .record_type = "road-camera",
    .tags = "\"ca\",\"camera\",\"traffic\",\"surveillance\"", .free_tier = 1,
    .url = "https://511.alberta.ca/api/v2/get/cameras?format=json&lang=en",
    .filter_query = 1, .title_keys = "Name,Roadway", .id_keys = "Id",
    .lat_key = "Latitude", .lon_key = "Longitude",
    .description = "Alberta's open camera inventory across the provincial "
      "highway network, including the remote northern corridors serving the oil "
      "sands where no other imagery is routinely available" },

  { .id = "CAM_DRIVEBC_BC", .name = "DriveBC — British Columbia highway cameras",
    .name_ja = "BC州道路カメラ", .category = "surveillance",
    .portal = "https://www.drivebc.ca", .record_type = "road-camera",
    .tags = "\"ca\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.drivebc.ca/mobile/pub/cameras/index.html?q={q}",
    .base = "https://www.drivebc.ca", .filter_query = 1,
    .description = "British Columbia's highway camera network — the camera "
      "name, highway, elevation and the refreshed still images used for pass "
      "and border-approach monitoring" },

  /* ── Europe ───────────────────────────────────────────────────────────── */
  { .id = "CAM_FI_DIGITRAFFIC_WEATHERCAM", .name = "Finland Digitraffic — weather camera stations",
    .name_ja = "フィンランド 道路気象カメラ", .category = "surveillance",
    .portal = "https://tie.digitraffic.fi", .record_type = "road-camera",
    .tags = "\"fi\",\"camera\",\"traffic\",\"surveillance\"", .free_tier = 1,
    .url = "https://tie.digitraffic.fi/api/weathercam/v1/stations",
    .array_path = "features", .filter_query = 1,
    .title_keys = "properties.name,properties.roadAddress.roadNumber",
    .id_keys = "id",
    .description = "The Finnish Transport Infrastructure Agency's full camera "
      "station inventory as GeoJSON, open and unauthenticated — station id, "
      "road address, coordinates, collection status and the list of presets "
      "(directions) each camera cycles through" },

  { .id = "CAM_NO_VEGVESEN", .name = "Norway Statens vegvesen — road cameras & sensors",
    .name_ja = "ノルウェー 道路カメラ", .category = "surveillance",
    .portal = "https://www.vegvesen.no", .record_type = "road-camera",
    .tags = "\"no\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.vegvesen.no/trafikk/kart/webkamera?q={q}",
    .base = "https://www.vegvesen.no", .filter_query = 1,
    .description = "Norway's road administration camera map — mountain pass, "
      "tunnel portal and ferry-quay cameras with their road reference and "
      "coordinates, a network built for winter closure decisions" },

  { .id = "CAM_UK_NATIONAL_HIGHWAYS", .name = "UK National Highways — CCTV & traffic data sites",
    .name_ja = "英国 幹線道路カメラ", .category = "surveillance",
    .portal = "https://webtris.nationalhighways.co.uk", .record_type = "road-camera",
    .tags = "\"uk\",\"camera\",\"traffic\",\"surveillance\"", .free_tier = 1,
    .url = "https://webtris.nationalhighways.co.uk/api/v1.0/sites",
    .array_path = "sites", .filter_query = 1,
    .title_keys = "Description,Name", .id_keys = "Id",
    .lat_key = "Latitude", .lon_key = "Longitude",
    .description = "The WebTRIS site inventory for England's strategic road "
      "network — every MIDAS loop, ANPR journey-time site and traffic monitoring "
      "point with its coordinates, status and the dates it was in operation. "
      "The physical sensing estate of the motorway network" },

  { .id = "CAM_NL_NDW_CAMERAS", .name = "Netherlands NDW — national road traffic data & cameras",
    .name_ja = "オランダ 道路交通カメラ", .category = "surveillance",
    .portal = "https://www.ndw.nu", .record_type = "road-camera",
    .tags = "\"nl\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.ndw.nu/zoeken?q={q}",
    .base = "https://www.ndw.nu", .filter_query = 1,
    .description = "The Nationaal Dataportaal Wegverkeer — the Dutch camera, "
      "loop and travel-time measurement inventory, together with the open "
      "datasets that publish the raw measurements from each site" },

  { .id = "CAM_CZ_RSD_KAMERY", .name = "Czech Republic ŘSD — motorway camera network",
    .name_ja = "チェコ 高速道路カメラ", .category = "surveillance",
    .portal = "https://kamery.rsd.cz", .record_type = "road-camera",
    .tags = "\"cz\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://kamery.rsd.cz/?q={q}",
    .base = "https://kamery.rsd.cz", .filter_query = 1,
    .description = "The Czech road and motorway directorate's camera network "
      "— device location by kilometre marker on each D-numbered motorway, with "
      "the live still images" },

  { .id = "CAM_PL_GDDKIA", .name = "Poland GDDKiA — national road cameras",
    .name_ja = "ポーランド 国道カメラ", .category = "surveillance",
    .portal = "https://www.gov.pl", .record_type = "road-camera",
    .tags = "\"pl\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.gov.pl/web/gddkia/kamery?q={q}",
    .base = "https://www.gov.pl", .filter_query = 1,
    .description = "Poland's national roads authority camera list — motorway "
      "and expressway devices with their road number and location, including "
      "the border-crossing approaches to Ukraine and Belarus" },

  { .id = "CAM_EE_TARKTEE", .name = "Estonia Tark Tee — road cameras & conditions",
    .name_ja = "エストニア 道路カメラ", .category = "surveillance",
    .portal = "https://tarktee.mnt.ee", .record_type = "road-camera",
    .tags = "\"ee\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://tarktee.mnt.ee/#/pois?q={q}",
    .base = "https://tarktee.mnt.ee", .filter_query = 1,
    .description = "Estonia's transport administration camera and road-weather "
      "station map, including the eastern border approaches at Narva and "
      "Koidula" },

  { .id = "CAM_IS_VEGAGERDIN", .name = "Iceland Vegagerðin — road cameras & weather stations",
    .name_ja = "アイスランド 道路カメラ", .category = "surveillance",
    .portal = "https://www.vegagerdin.is", .record_type = "road-camera",
    .tags = "\"is\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.vegagerdin.is/ferdaupplysingar/vefmyndavelar/?q={q}",
    .base = "https://www.vegagerdin.is", .filter_query = 1,
    .description = "Iceland's road administration webcams, which double as the "
      "country's densest public imaging network for volcanic and glacial "
      "monitoring because they cover routes no other camera reaches" },

  { .id = "CAM_ES_DGT_TRAFFIC", .name = "Spain DGT — traffic cameras & incident map",
    .name_ja = "スペイン 交通カメラ", .category = "surveillance",
    .portal = "https://infocar.dgt.es", .record_type = "road-camera",
    .tags = "\"es\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://infocar.dgt.es/etraffic/BuscarElementos?q={q}",
    .base = "https://infocar.dgt.es", .filter_query = 1,
    .description = "The Dirección General de Tráfico's camera and variable "
      "message sign inventory with the incident feed, covering the Spanish "
      "state road network and the French and Portuguese border approaches" },

  /* ── Asia-Pacific ─────────────────────────────────────────────────────── */
  { .id = "CAM_SG_LTA_TRAFFIC_IMAGES", .name = "Singapore — LTA traffic camera images (open)",
    .name_ja = "シンガポール 交通カメラ", .category = "surveillance",
    .portal = "https://api.data.gov.sg", .record_type = "road-camera",
    .tags = "\"sg\",\"camera\",\"traffic\",\"surveillance\"", .free_tier = 1,
    .url = "https://api.data.gov.sg/v1/transport/traffic-images",
    .array_path = "items", .filter_query = 1,
    .title_keys = "timestamp", .id_keys = "camera_id",
    .description = "Singapore's Land Transport Authority publishes its entire "
      "expressway camera network openly — camera ID, coordinates, image URL, "
      "capture timestamp and the image dimensions, refreshed every minute" },

  { .id = "CAM_HK_TRANSPORT_SNAPSHOTS", .name = "Hong Kong — Transport Department traffic snapshots",
    .name_ja = "香港 交通カメラ", .category = "surveillance",
    .portal = "https://data.gov.hk", .record_type = "road-camera",
    .tags = "\"hk\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://data.gov.hk/en-data/dataset/hk-td-tis_2-traffic-snapshot-images"
      "?q={q}",
    .base = "https://data.gov.hk", .filter_query = 1,
    .description = "Hong Kong's Transport Department camera inventory with the "
      "snapshot image URLs, covering the cross-harbour tunnels, the container "
      "port approaches and the boundary crossings to Shenzhen" },

  { .id = "CAM_TW_FREEWAY_CCTV", .name = "Taiwan — national freeway CCTV inventory",
    .name_ja = "台湾 高速道路カメラ", .category = "surveillance",
    .portal = "https://tisvcloud.freeway.gov.tw", .record_type = "road-camera",
    .tags = "\"tw\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://tisvcloud.freeway.gov.tw/?q={q}",
    .base = "https://tisvcloud.freeway.gov.tw", .filter_query = 1,
    .description = "Taiwan's freeway bureau publishes the CCTV, vehicle "
      "detector and electronic toll gantry inventories as open static files, "
      "with the coordinates and road milepost of every device" },

  { .id = "CAM_KR_ITS_OPENAPI", .name = "Korea ITS — national transport CCTV API",
    .name_ja = "韓国 国家交通情報カメラ", .category = "surveillance",
    .portal = "https://www.its.go.kr", .record_type = "road-camera",
    .tags = "\"kr\",\"camera\",\"traffic\",\"surveillance\"",
    .key_env = "KR_ITS_API_KEY", .free_tier = 1,
    .url = "https://openapi.its.go.kr:9443/cctvInfo?apiKey={key}&type=all"
      "&cctvType=1&minX=124.0&maxX=132.0&minY=33.0&maxY=39.0&getType=json",
    .array_path = "response.data", .filter_query = 1,
    .title_keys = "cctvname,roadsectionid", .id_keys = "cctvurl",
    .lat_key = "coordy", .lon_key = "coordx",
    .description = "Korea's national transport information centre CCTV "
      "service, requested across the full national bounding box so the whole "
      "camera estate is returned rather than one region — name, coordinates, "
      "road section and the HLS stream URL for each device" },

  { .id = "CAM_NZ_NZTA_CAMERAS", .name = "New Zealand NZTA — state highway cameras",
    .name_ja = "NZ 国道カメラ", .category = "surveillance",
    .portal = "https://trafficnz.info", .record_type = "road-camera",
    .tags = "\"nz\",\"camera\",\"traffic\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://trafficnz.info/camera/?q={q}",
    .base = "https://trafficnz.info", .filter_query = 1,
    .description = "Waka Kotahi's state highway camera network — camera "
      "identifier, region, highway and direction, with the still image and the "
      "outage status published for each site" },

  { .id = "CAM_AU_NSW_LIVE", .name = "Australia NSW — live traffic cameras",
    .name_ja = "豪州NSW州 交通カメラ", .category = "surveillance",
    .portal = "https://api.transport.nsw.gov.au", .record_type = "road-camera",
    .tags = "\"au\",\"camera\",\"traffic\",\"surveillance\"",
    .key_env = "NSW_TRANSPORT_API_KEY", .free_tier = 1,
    .url = "https://api.transport.nsw.gov.au/v1/live/cameras/open",
    .headers = { "Authorization: apikey {key}", NULL },
    .array_path = "features", .filter_query = 1,
    .title_keys = "properties.title,properties.region",
    .id_keys = "properties.id",
    .description = "Transport for NSW's live camera inventory as GeoJSON — "
      "title, region, view direction, coordinates and image URL for the Sydney "
      "motorway network and the regional highways" },
};

HP_REGISTER_TABLE(HP3_SURV_CAMERAS)
