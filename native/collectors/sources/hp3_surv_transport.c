/* collectors/sources/hp3_surv_transport.c — movement telemetry.
 *
 * Transport telemetry is the most literal form of open surveillance: vehicles,
 * trains, ships and aircraft continuously broadcasting identity and position,
 * and public authorities publishing the result. The tree already has several
 * ADS-B feeds and an AIS stream; what was missing is everything around them —
 * the national rail telemetry APIs, the coastal AIS feeds that governments
 * publish openly, the transit operator and feed registries that say who runs
 * what, and the Japanese public-transport data platform.
 *
 * The registries matter as much as the live positions. Transitland and the
 * Japanese GTFS repository answer "which organisation operates transport in
 * this place, and what feed do they publish" — an operator directory keyed to
 * geography, which is the entry point to everything downstream. */
#include "../../lib/hpengine.h"

static const hp_source HP3_SURV_TRANSPORT[] = {
  /* ── Operator and feed registries ─────────────────────────────────────── */
  { .id = "TRANSITLAND_OPERATORS", .name = "Transitland — global transit operator registry",
    .name_ja = "Transitland 交通事業者登録", .category = "transport",
    .portal = "https://transit.land", .record_type = "transit-operator",
    .tags = "\"transit\",\"registry\",\"mobility\"",
    .key_env = "TRANSITLAND_API_KEY", .free_tier = 1,
    .url = "https://transit.land/api/v2/rest/operators?search={q}&limit=100"
      "&apikey={key}",
    .array_path = "operators", .title_keys = "name,short_name",
    .id_keys = "onestop_id",
    .next_path = "meta.next", .page_max = 40,
    .description = "Every public transport operator Transitland knows about, "
      "worldwide — the legal operating name, the agencies it runs, the feeds it "
      "publishes and the places it serves. An operator directory keyed to "
      "geography rather than to a company register" },

  { .id = "TRANSITLAND_FEEDS", .name = "Transitland — transit feed & data source registry",
    .name_ja = "Transitland データ提供元登録", .category = "transport",
    .portal = "https://transit.land", .record_type = "transit-feed",
    .tags = "\"transit\",\"registry\",\"gtfs\"", .key_env = "TRANSITLAND_API_KEY",
    .free_tier = 1,
    .url = "https://transit.land/api/v2/rest/feeds?search={q}&limit=100&apikey={key}",
    .array_path = "feeds", .title_keys = "name,spec", .id_keys = "onestop_id",
    .next_path = "meta.next", .page_max = 40,
    .description = "The feed registry behind it — GTFS and GTFS-Realtime "
      "endpoints, their licence terms, authorisation requirements and fetch "
      "history. Where a jurisdiction's live vehicle positions can actually be "
      "obtained, and under what conditions" },

  { .id = "JP_GTFS_DATA_REPOSITORY", .name = "Japan — public transport GTFS data repository",
    .name_ja = "GTFSデータリポジトリ 事業者一覧", .category = "transport",
    .portal = "https://api.gtfs-data.jp", .record_type = "jp-transit-operator",
    .tags = "\"jp\",\"transit\",\"gtfs\",\"registry\"", .free_tier = 1,
    .url = "https://api.gtfs-data.jp/v2/organizations",
    .array_path = "body", .filter_query = 1,
    .title_keys = "organization_name,organization_name_kana",
    .id_keys = "organization_id",
    .description = "The Japanese GTFS repository's operator list — every bus "
      "and rail operator publishing schedule data, with its official name, "
      "reading, prefecture and the feeds it maintains. The directory of "
      "Japanese transport operators as data publishers" },

  { .id = "JP_ODPT_TRANSPORT", .name = "Japan ODPT — public transport open data",
    .name_ja = "公共交通オープンデータ", .category = "transport",
    .portal = "https://api.odpt.org", .record_type = "jp-transit-record",
    .tags = "\"jp\",\"transit\",\"realtime\"", .key_env = "ODPT_CONSUMER_KEY",
    .free_tier = 1,
    .url = "https://api.odpt.org/api/v4/odpt:Railway?acl:consumerKey={key}",
    .filter_query = 1, .title_keys = "dc:title,odpt:operator",
    .id_keys = "owl:sameAs", .date_keys = "dc:date",
    .description = "The Public Transportation Open Data Center — railway "
      "lines, stations, operators, timetables and, for participating "
      "operators, live train location and delay information across the Tokyo "
      "metropolitan network" },

  /* ── Rail telemetry ───────────────────────────────────────────────────── */
  { .id = "FI_DIGITRAFFIC_RAIL", .name = "Finland Digitraffic — live rail traffic API",
    .name_ja = "フィンランド 鉄道リアルタイム", .category = "transport",
    .portal = "https://rata.digitraffic.fi", .record_type = "rail-movement",
    .tags = "\"fi\",\"rail\",\"realtime\",\"surveillance\"", .free_tier = 1,
    .url = "https://rata.digitraffic.fi/api/v1/train-locations/latest",
    .filter_query = 1, .title_keys = "trainNumber,departureDate",
    .id_keys = "trainNumber", .date_keys = "timestamp",
    .description = "Live GPS positions of every train on the Finnish network, "
      "open and unauthenticated — train number, speed, coordinates and "
      "timestamp, alongside the timetable, composition and cause-code APIs from "
      "the same service" },

  { .id = "FI_DIGITRAFFIC_RAIL_STATIONS", .name = "Finland Digitraffic — rail infrastructure metadata",
    .name_ja = "フィンランド 鉄道施設データ", .category = "transport",
    .portal = "https://rata.digitraffic.fi", .record_type = "rail-infrastructure",
    .tags = "\"fi\",\"rail\",\"infrastructure\"", .free_tier = 1,
    .url = "https://rata.digitraffic.fi/api/v1/metadata/stations",
    .filter_query = 1, .title_keys = "stationName,stationShortCode",
    .id_keys = "stationUICCode",
    .lat_key = "latitude", .lon_key = "longitude",
    .description = "Every Finnish station and passing loop with its UIC code, "
      "coordinates, whether it handles passengers and its country code — "
      "including the Russian border crossings at Vainikkala and Imatra" },

  { .id = "DE_DB_TRANSPORT_REST", .name = "Germany — Deutsche Bahn public transport API",
    .name_ja = "ドイツ鉄道 公共交通API", .category = "transport",
    .portal = "https://v6.db.transport.rest", .record_type = "rail-station",
    .tags = "\"de\",\"rail\",\"realtime\"", .free_tier = 1,
    .url = "https://v6.db.transport.rest/locations?query={q}&results=100"
      "&stops=true&addresses=true&poi=true",
    .filter_query = 1, .title_keys = "name,type", .id_keys = "id",
    .lat_key = "location.latitude", .lon_key = "location.longitude",
    .description = "The open DB HAFAS interface — stations and stops with "
      "coordinates and product classes, and behind them the departure boards "
      "with live delays and platform changes for the whole German network" },

  { .id = "BE_IRAIL_API", .name = "Belgium iRail — NMBS/SNCB open rail API",
    .name_ja = "ベルギー鉄道API", .category = "transport",
    .portal = "https://api.irail.be", .record_type = "rail-station",
    .tags = "\"be\",\"rail\",\"realtime\"", .free_tier = 1,
    .url = "https://api.irail.be/stations/?format=json&lang=en",
    .array_path = "station", .filter_query = 1,
    .title_keys = "name,standardname", .id_keys = "id",
    .lat_key = "locationY", .lon_key = "locationX",
    .description = "The Belgian rail open API — the full station list with "
      "coordinates, and the liveboard and vehicle endpoints that give real "
      "departure times, delays and train composition" },

  { .id = "US_AMTRAK_TRAIN_POSITIONS", .name = "Amtrak — live train positions (Amtraker)",
    .name_ja = "米国アムトラック 列車位置", .category = "transport",
    .portal = "https://api-v3.amtraker.com", .record_type = "rail-movement",
    .tags = "\"us\",\"rail\",\"realtime\",\"surveillance\"", .free_tier = 1,
    .url = "https://api-v3.amtraker.com/v3/trains",
    .filter_query = 1, .title_keys = "routeName,trainID",
    .id_keys = "trainID", .date_keys = "updatedAt",
    .lat_key = "lat", .lon_key = "lon",
    .description = "Live positions of every Amtrak train — train number, "
      "route, heading, speed, the station list with scheduled and actual times, "
      "and the delay at each stop" },

  { .id = "UK_BODS_BUS_DATA", .name = "UK — Bus Open Data Service datasets & operators",
    .name_ja = "英国 バスオープンデータ", .category = "transport",
    .portal = "https://data.bus-data.dft.gov.uk", .record_type = "uk-transit-feed",
    .tags = "\"uk\",\"transit\",\"realtime\"", .key_env = "UK_BODS_API_KEY",
    .free_tier = 1,
    .url = "https://data.bus-data.dft.gov.uk/api/v1/dataset/?api_key={key}"
      "&limit=100&status=published",
    .array_path = "results", .title_keys = "name,operatorName",
    .id_keys = "id", .date_keys = "modified",
    .next_path = "next", .page_max = 40,
    .description = "Every bus operator in England is legally required to "
      "publish timetable, fares and live location data here. The dataset list "
      "is therefore also an operator register — the licensed name, the "
      "National Operator Code and the services run" },

  { .id = "CH_OPENTRANSPORTDATA", .name = "Switzerland — open transport data platform",
    .name_ja = "スイス 公共交通オープンデータ", .category = "transport",
    .portal = "https://opentransportdata.swiss", .record_type = "ch-transit-feed",
    .tags = "\"ch\",\"transit\",\"realtime\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://opentransportdata.swiss/en/search/?q={q}",
    .base = "https://opentransportdata.swiss", .filter_query = 1,
    .description = "Switzerland's national access point — the full timetable, "
      "real-time delay feed, service facilities and the stop register that "
      "assigns a unique identifier to every platform in the country" },

  /* ── Maritime telemetry ───────────────────────────────────────────────── */
  { .id = "FI_DIGITRAFFIC_AIS", .name = "Finland Digitraffic — open AIS vessel positions",
    .name_ja = "フィンランド AIS船舶位置", .category = "maritime",
    .portal = "https://meri.digitraffic.fi", .record_type = "vessel-position",
    .tags = "\"fi\",\"ais\",\"maritime\",\"surveillance\"", .free_tier = 1,
    .url = "https://meri.digitraffic.fi/api/ais/v1/locations",
    .array_path = "features", .filter_query = 1,
    .title_keys = "mmsi,properties.navStat", .id_keys = "mmsi",
    .date_keys = "properties.timestampExternal",
    .description = "Live AIS positions for the Baltic and Gulf of Finland, "
      "published openly by the Finnish Transport Infrastructure Agency — MMSI, "
      "coordinates, speed, course, heading and navigational status. Open AIS "
      "for one of the most strategically watched sea areas in Europe" },

  { .id = "FI_DIGITRAFFIC_VESSEL_METADATA", .name = "Finland Digitraffic — AIS vessel metadata",
    .name_ja = "フィンランド 船舶メタデータ", .category = "maritime",
    .portal = "https://meri.digitraffic.fi", .record_type = "vessel",
    .tags = "\"fi\",\"ais\",\"maritime\",\"vessel\"", .free_tier = 1,
    .url = "https://meri.digitraffic.fi/api/ais/v1/vessels",
    .filter_query = 1, .title_keys = "name,callSign", .id_keys = "mmsi",
    .date_keys = "timestamp",
    .description = "The static AIS record behind each position — vessel name, "
      "call sign, IMO number, ship type, dimensions, draught, destination and "
      "reported ETA. Destination fields are self-declared and their "
      "inconsistencies are themselves a signal" },

  { .id = "DK_OPEN_AIS_ARCHIVE", .name = "Denmark — open AIS data archive",
    .name_ja = "デンマーク AISデータ公開", .category = "maritime",
    .portal = "https://web.ais.dk", .record_type = "vessel-position",
    .tags = "\"dk\",\"ais\",\"maritime\",\"surveillance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://web.ais.dk/aisdata/?q={q}",
    .base = "https://web.ais.dk", .filter_query = 1,
    .description = "The Danish Maritime Authority publishes raw historical AIS "
      "for Danish waters as daily bulk files, free and without registration — "
      "the only openly downloadable multi-year AIS archive covering the Baltic "
      "approaches and the Great Belt" },

  { .id = "NO_BARENTSWATCH_AIS", .name = "Norway BarentsWatch — coastal AIS & fisheries activity",
    .name_ja = "ノルウェー 沿岸AIS/漁業活動", .category = "maritime",
    .portal = "https://www.barentswatch.no", .record_type = "vessel-position",
    .tags = "\"no\",\"ais\",\"maritime\",\"fisheries\"",
    .key_env = "BARENTSWATCH_TOKEN", .free_tier = 1,
    .url = "https://live.ais.barentswatch.no/v1/latest/combined",
    .headers = { "Authorization: Bearer {key}", NULL },
    .filter_query = 1, .title_keys = "name,shipType", .id_keys = "mmsi",
    .date_keys = "msgtime",
    .lat_key = "latitude", .lon_key = "longitude",
    .description = "Norwegian coastal AIS with the fisheries activity layer — "
      "vessel positions along the Norwegian coast and in the Barents Sea, "
      "including the catch and landing reports joined to the vessel" },

  { .id = "GLOBAL_FISHING_WATCH_VESSELS", .name = "Global Fishing Watch — vessel identity & activity",
    .name_ja = "世界漁業監視 船舶識別", .category = "maritime",
    .portal = "https://gateway.api.globalfishingwatch.org",
    .record_type = "fishing-vessel",
    .tags = "\"ais\",\"fisheries\",\"maritime\",\"surveillance\"",
    .key_env = "GFW_API_TOKEN", .free_tier = 1,
    .url = "https://gateway.api.globalfishingwatch.org/v3/vessels/search?"
      "query={q}&datasets[0]=public-global-vessel-identity:latest&limit=100",
    .headers = { "Authorization: Bearer {key}", NULL },
    .array_path = "entries", .title_keys = "selfReportedInfo.shipname,registryInfo.flag",
    .id_keys = "selfReportedInfo.ssvid",
    .next_path = "since", .page_max = 30,
    .description = "Fishing vessel identity reconciled across AIS, VMS and "
      "national registries — flag history, ownership, authorisations, and the "
      "encounters, loitering events and port visits that indicate "
      "transshipment. The standard tool for IUU fishing and sanctions evasion "
      "at sea" },

  /* ── Aviation telemetry ───────────────────────────────────────────────── */
  { .id = "AIRPLANES_LIVE_CALLSIGN", .name = "airplanes.live — ADS-B by callsign",
    .name_ja = "航空機ADS-B コールサイン検索", .category = "transport",
    .portal = "https://api.airplanes.live", .record_type = "aircraft-position",
    .tags = "\"adsb\",\"aviation\",\"realtime\",\"surveillance\"", .free_tier = 1,
    .url = "https://api.airplanes.live/v2/callsign/{qU}",
    .array_path = "ac", .title_keys = "flight,t", .id_keys = "hex",
    .lat_key = "lat", .lon_key = "lon",
    .description = "Live ADS-B state vectors for a flight callsign — ICAO24 "
      "address, registration, type, altitude, speed, squawk and position, from "
      "a network that does not filter military or blocked registrations" },

  { .id = "AIRPLANES_LIVE_REGISTRATION", .name = "airplanes.live — ADS-B by registration or hex",
    .name_ja = "航空機ADS-B 登録記号検索", .category = "transport",
    .portal = "https://api.airplanes.live", .record_type = "aircraft-position",
    .tags = "\"adsb\",\"aviation\",\"realtime\",\"surveillance\"", .free_tier = 1,
    .url = "https://api.airplanes.live/v2/reg/{qU}",
    .array_path = "ac", .title_keys = "flight,t", .id_keys = "hex",
    .lat_key = "lat", .lon_key = "lon",
    .description = "The same network keyed on a tail number, which is the "
      "pivot that matters when starting from an aircraft register entry rather "
      "than from a flight — position, altitude, emitter category and the "
      "squawk code including emergency codes" },

  { .id = "AIRPLANES_LIVE_MILITARY", .name = "airplanes.live — military and special aircraft",
    .name_ja = "航空機ADS-B 軍用機", .category = "transport",
    .portal = "https://api.airplanes.live", .record_type = "aircraft-position",
    .tags = "\"adsb\",\"aviation\",\"military\",\"surveillance\"", .free_tier = 1,
    .url = "https://api.airplanes.live/v2/mil",
    .array_path = "ac", .filter_query = 1,
    .title_keys = "flight,t", .id_keys = "hex",
    .lat_key = "lat", .lon_key = "lon",
    .description = "Aircraft currently broadcasting with military ICAO "
      "allocations — type, callsign, altitude and position. Tanker and ISR "
      "orbits, transport surges and unusual VIP movements are all visible in "
      "this one feed" },

  { .id = "OPENSKY_FLIGHT_HISTORY", .name = "OpenSky — flights by aircraft over time",
    .name_ja = "OpenSky 航空機飛行履歴", .category = "transport",
    .portal = "https://opensky-network.org", .record_type = "flight",
    .tags = "\"adsb\",\"aviation\",\"history\"", .want = HP_ICAO24,
    .key_env = "OPENSKY_CREDENTIALS", .free_tier = 1,
    .url = "https://opensky-network.org/api/flights/aircraft?icao24={ql}"
      "&begin=1420070400&end=2000000000",
    .headers = { "Authorization: Basic {keyb64}", NULL },
    .filter_query = 1,
    .title_keys = "callsign,estDepartureAirport", .id_keys = "icao24",
    .date_keys = "firstSeen",
    .description = "The historical flight list for one airframe — every "
      "departure and arrival airport pair OpenSky has observed, with first and "
      "last seen times. Requested over the network's full retained window so "
      "the aircraft's whole observed history is returned, not a recent slice" },

  { .id = "SPACE_LAUNCH_SCHEDULE", .name = "Launch Library — orbital launch schedule & history",
    .name_ja = "打上げスケジュール/履歴", .category = "transport",
    .portal = "https://ll.thespacedevs.com", .record_type = "space-launch",
    .tags = "\"space\",\"launch\",\"schedule\"", .free_tier = 1,
    .url = "https://ll.thespacedevs.com/2.2.0/launch/?search={q}&limit=100",
    .array_path = "results", .title_keys = "name,launch_service_provider.name",
    .id_keys = "id", .date_keys = "net",
    .lat_key = "pad.latitude", .lon_key = "pad.longitude",
    .next_path = "next", .page_max = 40,
    .description = "Orbital and suborbital launches past and scheduled — the "
      "provider, the vehicle, the pad with coordinates, the mission and its "
      "orbit, and the payload customers where disclosed" },

  { .id = "US_BTS_TRANSPORT_STATISTICS", .name = "US BTS — transportation statistics & carrier data",
    .name_ja = "米国運輸統計局 データ", .category = "transport",
    .portal = "https://www.transtats.bts.gov", .record_type = "us-transport-statistic",
    .tags = "\"us\",\"aviation\",\"statistics\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.transtats.bts.gov/DataIndex.asp?q={q}",
    .base = "https://www.transtats.bts.gov", .filter_query = 1,
    .description = "The Bureau of Transportation Statistics data index — "
      "airline on-time performance by tail number, the T-100 segment traffic "
      "by carrier and route, air carrier financials and the aircraft inventory "
      "each carrier reports" },

  { .id = "EU_RAIL_RINF_INFRASTRUCTURE", .name = "EU RINF — register of railway infrastructure",
    .name_ja = "EU 鉄道インフラ登録", .category = "transport",
    .portal = "https://rinf.era.europa.eu", .record_type = "eu-rail-infrastructure",
    .tags = "\"eu\",\"rail\",\"infrastructure\",\"registry\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://rinf.era.europa.eu/API/Search?q={q}",
    .base = "https://rinf.era.europa.eu", .filter_query = 1,
    .description = "The EU register of railway infrastructure — every section "
      "of line with its infrastructure manager, gauge, electrification, "
      "signalling system, axle load and tunnel and platform parameters. The "
      "physical rail network as a queryable dataset" },
};

HP_REGISTER_TABLE(HP3_SURV_TRANSPORT)
