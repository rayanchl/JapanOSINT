/* collectors/pivot/table/hp3b31_survtransport.c — batch 31: survtransport — movement telemetry.
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
 * geography, which is the entry point to everything downstream.
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

static const hp_source HP3B31_SURVTRANSPORT[] = {
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

  { .id = "JP_ODPT_TRANSPORT", .name = "Japan ODPT — public transport open data",
    .name_ja = "公共交通オープンデータ", .category = "transport",
    .portal = "https://api.odpt.org", .record_type = "jp-transit-record",
    .tags = "\"jp\",\"transit\",\"realtime\"", .key_env = "ODPT_CONSUMER_KEY",
    .free_tier = 1,
    .url = "https://api.odpt.org/api/v4/odpt:Railway?acl:consumerKey={key}",
    .filter_query = 1, .title_keys = "dc:title,odpt:operator",
    .id_keys = "owl:sameAs", .date_keys = "dc:date",
    .interval = 21600,
    .description = "The Public Transportation Open Data Center — railway "
      "lines, stations, operators, timetables and, for participating "
      "operators, live train location and delay information across the Tokyo "
      "metropolitan network" },

  /* ── Rail telemetry ───────────────────────────────────────────────────── */
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
    .interval = 86400,
    .description = "The Belgian rail open API — the full station list with "
      "coordinates, and the liveboard and vehicle endpoints that give real "
      "departure times, delays and train composition" },

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
    .interval = 21600,
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
    .interval = 600,
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

HP_REGISTER_TABLE(HP3B31_SURVTRANSPORT)
