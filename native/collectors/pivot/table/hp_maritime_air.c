/* collectors/sources/hp_maritime_air.c — aircraft and vessel identity depth.
 *
 * Live position feeds were already covered. Position is the least durable fact
 * about an aircraft or ship; identity and history are the durable ones. These
 * rows resolve a Mode-S hex, tail number, callsign, IMO or vessel name into the
 * registry record, the operator, the photographic history, the scheduled route
 * and — for ships — the port-state inspection and detention record, which is the
 * closest thing to a compliance history a vessel has. */
#include "lib/hpengine.h"

static const hp_source HP_MARAIR[] = {
  /* ── Aircraft identity ─────────────────────────────────────────────────── */
  { .id = "HEXDB_AIRPORT_IATA", .name = "hexdb.io — airport by IATA code",
    .name_ja = "hexdb — IATAコードで空港照会", .category = "transport",
    .portal = "https://hexdb.io", .record_type = "airport",
    .tags = "\"aviation\",\"airports\"", .free_tier = 1,
    .url = "https://hexdb.io/api/v1/airport/iata/{qU}",
    .title_keys = "airport,region_name", .id_keys = "iata",
    .lat_key = "latitude", .lon_key = "longitude",
    .description = "Resolves the IATA/ICAO codes found in a route string to the "
      "airport's name, country, region and coordinates — the step that turns "
      "'HND-DXB' in a callsign route into two mapped places" },

  { .id = "HEXDB_REG_LOOKUP", .name = "hexdb.io — registration to hex",
    .name_ja = "hexdb — 登録記号→ICAO24", .category = "transport",
    .portal = "https://hexdb.io", .record_type = "aircraft",
    .tags = "\"aviation\",\"registry\"", .free_tier = 1,
    .url = "https://hexdb.io/reg-hex?reg={qU}",
    .title_keys = "hex,value", .id_keys = "hex",
    .description = "Resolves a tail number to its Mode-S hex so a registry-side "
      "identity (owner, lien holder, airworthiness) can be joined to live ADS-B "
      "observations of the same airframe" },

  { .id = "HEXDB_CALLSIGN_ROUTE", .name = "hexdb.io — callsign route",
    .name_ja = "hexdb — コールサイン運航区間", .category = "transport",
    .portal = "https://hexdb.io", .record_type = "flight-route",
    .tags = "\"aviation\",\"routes\"", .free_tier = 1,
    .url = "https://hexdb.io/callsign-route-iata?callsign={qU}",
    .title_keys = "route,value", .id_keys = "route",
    .description = "The scheduled origin/destination pair behind a flight callsign — "
      "what makes a bare callsign in a track log interpretable as a specific city "
      "pair, including multi-leg routes" },

  { .id = "ADSBLOL_HEX", .name = "adsb.lol — live state by hex",
    .name_ja = "adsb.lol — ICAO24で現況取得", .category = "transport",
    .portal = "https://adsb.lol", .record_type = "aircraft-state",
    .tags = "\"aviation\",\"adsb\"", .want = HP_ICAO24, .free_tier = 1,
    .url = "https://api.adsb.lol/v2/hex/{ql}", .array_path = "ac",
    .title_keys = "flight,r,t", .id_keys = "hex",
    .lat_key = "lat", .lon_key = "lon",
    .description = "Current state vector for one airframe — position, altitude, "
      "speed, track, squawk, registration, type and emergency flag, from the "
      "community ADS-B network with no key" },

  { .id = "ADSBLOL_CALLSIGN", .name = "adsb.lol — live state by callsign",
    .name_ja = "adsb.lol — コールサインで現況取得", .category = "transport",
    .portal = "https://adsb.lol", .record_type = "aircraft-state",
    .tags = "\"aviation\",\"adsb\"", .free_tier = 1,
    .url = "https://api.adsb.lol/v2/callsign/{qU}", .array_path = "ac",
    .title_keys = "flight,r,t", .id_keys = "hex",
    .lat_key = "lat", .lon_key = "lon",
    .description = "Aircraft currently transmitting a given callsign — resolves a "
      "flight number seen in a schedule, a NOTAM or a social post to a live airframe "
      "and position" },

  { .id = "ADSBLOL_REGISTRATION", .name = "adsb.lol — live state by registration",
    .name_ja = "adsb.lol — 登録記号で現況取得", .category = "transport",
    .portal = "https://adsb.lol", .record_type = "aircraft-state",
    .tags = "\"aviation\",\"adsb\"", .free_tier = 1,
    .url = "https://api.adsb.lol/v2/registration/{qU}", .array_path = "ac",
    .title_keys = "r,flight,t", .id_keys = "hex",
    .lat_key = "lat", .lon_key = "lon",
    .description = "Live state for a tail number — the direct route from a registry "
      "record or a corporate-jet report to whether that airframe is airborne right "
      "now, and where" },

  { .id = "ADSBLOL_SQUAWK", .name = "adsb.lol — aircraft by squawk code",
    .name_ja = "adsb.lol — スコーク別機体", .category = "transport",
    .portal = "https://adsb.lol", .record_type = "aircraft-state",
    .tags = "\"aviation\",\"adsb\",\"emergency\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://api.adsb.lol/v2/sqk/{qd}", .array_path = "ac",
    .title_keys = "flight,r,t", .id_keys = "hex",
    .lat_key = "lat", .lon_key = "lon",
    .description = "All aircraft currently transmitting a squawk code — 7500/7600/"
      "7700 for emergencies, or a discrete code assigned by a specific control "
      "centre, which is how state and military flights are grouped" },

  { .id = "AIRPLANES_LIVE_HEX", .name = "airplanes.live — live state by hex",
    .name_ja = "airplanes.live — ICAO24現況", .category = "transport",
    .portal = "https://airplanes.live", .record_type = "aircraft-state",
    .tags = "\"aviation\",\"adsb\"", .want = HP_ICAO24, .free_tier = 1,
    .url = "https://api.airplanes.live/v2/hex/{ql}", .array_path = "ac",
    .title_keys = "flight,r,t", .id_keys = "hex",
    .lat_key = "lat", .lon_key = "lon",
    .description = "Independent second ADS-B network for the same airframe — "
      "different receiver coverage, and it does not filter blocked registrations the "
      "same way, so gaps in one feed are often filled by the other" },

  { .id = "OPENSKY_AIRCRAFT_META", .name = "OpenSky — aircraft metadata",
    .name_ja = "OpenSky — 機体メタデータ", .category = "transport",
    .portal = "https://opensky-network.org", .record_type = "aircraft",
    .tags = "\"aviation\",\"registry\"", .want = HP_ICAO24, .free_tier = 1,
    .url = "https://opensky-network.org/api/metadata/aircraft/icao/{ql}",
    .title_keys = "registration,model", .id_keys = "icao24",
    .date_keys = "registered",
    .description = "OpenSky's aircraft database record — registration, model, "
      "serial, owner, operator ICAO, built and registration dates, and category "
      "description for one ICAO24 address" },

  { .id = "PLANESPOTTERS_PHOTOS", .name = "Planespotters — airframe photo history",
    .name_ja = "Planespotters — 機体写真履歴", .category = "transport",
    .portal = "https://www.planespotters.net", .record_type = "aircraft-photo",
    .tags = "\"aviation\",\"imagery\"", .free_tier = 1,
    .url = "https://api.planespotters.net/pub/photos/reg/{qU}",
    .array_path = "photos", .title_keys = "photographer,link", .id_keys = "id",
    .description = "Photographs of a specific airframe with photographer, date and "
      "thumbnail links — livery changes over time are how re-registrations and quiet "
      "operator changes get caught" },

  { .id = "TC_CCARCS_REGISTRY", .name = "Transport Canada — civil aircraft register",
    .name_ja = "カナダ 航空機登録", .category = "transport",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://wwwapps.tc.gc.ca", .record_type = "aircraft",
    .tags = "\"ca\",\"aviation\",\"ownership\"", .free_tier = 1,
    .url = "https://wwwapps.tc.gc.ca/saf-sec-sur/2/ccarcs-riacc/RchSimpRes.aspx?cn={q}",
    .base = "https://wwwapps.tc.gc.ca", .filter_query = 1,
    .description = "Canadian Civil Aircraft Register search by mark or owner name — "
      "registered owner, address, aircraft type and certificate status, published "
      "free of charge" },

  { .id = "UK_GINFO_REGISTRY", .name = "UK CAA G-INFO — aircraft register",
    .name_ja = "英国 航空機登録(G-INFO)", .category = "transport",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://siteapps.caa.co.uk", .record_type = "aircraft",
    .tags = "\"uk\",\"aviation\",\"ownership\"", .free_tier = 1,
    .url = "https://siteapps.caa.co.uk/g-info/?regmark={qU}",
    .base = "https://siteapps.caa.co.uk", .filter_query = 0,
    .description = "UK aircraft register entries — registered owner or operator, "
      "address, aircraft type, certificate of airworthiness and previous "
      "registrations for a G- mark" },

  { .id = "OURAIRPORTS_LOOKUP", .name = "OurAirports — global airport & heliport data",
    .name_ja = "OurAirports — 世界の空港データ", .category = "transport",
    .type = "dataset", .mode = HP_CSV,
    .portal = "https://ourairports.com", .record_type = "airport",
    .tags = "\"aviation\",\"airports\"", .free_tier = 1,
    .url = "https://davidmegginson.github.io/ourairports-data/airports.csv",
    .filter_query = 1, .title_keys = "name",
    .id_keys = "ident", .lat_key = "latitude_deg", .lon_key = "longitude_deg",
    .description = "Every airport, airstrip and heliport on earth filtered to the "
      "query — ICAO/IATA/local codes, type, elevation, coordinates, region and "
      "scheduled-service flag, including the small private strips no commercial "
      "database bothers with" },

  { .id = "AVIATIONSTACK_FLIGHT", .name = "aviationstack — flight schedule & status",
    .name_ja = "aviationstack — 便情報", .category = "transport",
    .portal = "https://aviationstack.com", .record_type = "flight",
    .tags = "\"aviation\",\"schedules\"", .key_env = "AVIATIONSTACK_KEY",
    .free_tier = 1,
    .url = "https://api.aviationstack.com/v1/flights?access_key={key}&flight_iata={qU}",
    .array_path = "data", .title_keys = "flight.iata,airline.name",
    .id_keys = "flight.icao", .date_keys = "flight_date",
    .description = "Scheduled and actual times, gates, terminals, aircraft "
      "registration and codeshare parents for a flight number — the commercial layer "
      "that ADS-B alone cannot supply" },

  /* ── Vessels ───────────────────────────────────────────────────────────── */
  { .id = "PARIS_MOU_INSPECTIONS", .name = "Paris MoU — port state inspections",
    .name_ja = "パリMOU — 船舶検査/拘留記録", .category = "transport",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://parismou.org", .record_type = "vessel-inspection",
    .tags = "\"maritime\",\"inspections\",\"compliance\"", .free_tier = 1,
    .url = "https://parismou.org/inspection-search/inspection-search?search={q}",
    .base = "https://parismou.org", .filter_query = 1,
    .description = "European port-state control inspections of a vessel — deficiency "
      "counts, detentions, the inspecting authority and dates. A detention history is "
      "the hardest public evidence of a substandard operator" },

  { .id = "VESSEL_SANCTIONS_CHECK", .name = "OpenSanctions — sanctioned vessels",
    .name_ja = "制裁対象船舶の照会", .category = "transport",
    .portal = "https://www.opensanctions.org", .record_type = "vessel-sanction",
    .tags = "\"maritime\",\"sanctions\"", .key_env = "OPENSANCTIONS_API_KEY",
    .headers = { "Authorization: ApiKey {key}", NULL }, .free_tier = 1,
    .url = "https://api.opensanctions.org/search/default?q={q}&schema=Vessel&limit=25",
    .array_path = "results", .title_keys = "caption", .id_keys = "id",
    .description = "Vessel-schema entities in the sanctions corpus — matched by name, "
      "IMO or MMSI, with flag, tonnage, former names and the designating programme. "
      "Shadow-fleet tankers are renamed constantly, so the former-name list matters" },

  { .id = "USCG_PSIX_VESSEL", .name = "USCG PSIX — US vessel documentation & casualties",
    .name_ja = "米国沿岸警備隊 船舶情報(PSIX)", .category = "transport",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://cgmix.uscg.mil", .record_type = "vessel",
    .tags = "\"maritime\",\"us\",\"ownership\"", .free_tier = 1,
    .url = "https://cgmix.uscg.mil/PSIX/PSIXSearch.aspx?VesselName={q}",
    .base = "https://cgmix.uscg.mil", .filter_query = 1,
    .description = "US Coast Guard Port State Information Exchange — vessel "
      "documentation, owner/operator, dimensions, inspection deficiencies and "
      "casualty records for US-flag and US-calling vessels" },

  { .id = "GFW_VESSEL_SEARCH", .name = "Global Fishing Watch — vessel identity",
    .name_ja = "Global Fishing Watch — 漁船識別", .category = "transport",
    .portal = "https://globalfishingwatch.org", .record_type = "vessel",
    .tags = "\"maritime\",\"fishing\",\"identity\"", .key_env = "GFW_API_TOKEN",
    .free_tier = 1,
    .url = "https://gateway.api.globalfishingwatch.org/v3/vessels/search?query={q}"
           "&datasets%5B0%5D=public-global-vessel-identity%3Alatest&limit=25",
    .headers = { "Authorization: Bearer {key}", NULL },
    .array_path = "entries", .title_keys = "selfReportedInfo.0.shipname,combinedSourcesInfo.0.shipname",
    .id_keys = "selfReportedInfo.0.id",
    .description = "Vessel identity across AIS self-reporting and public registries — "
      "name changes, flag changes, MMSI/IMO/call-sign history, owner and gear type. "
      "Flag-hopping is the standard evasion pattern and this is where it is visible" },
};

HP_REGISTER_TABLE(HP_MARAIR)
