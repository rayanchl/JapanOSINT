/* Verified-live us_local sources (37), part 2.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "_vjson_idkeys.inc"

VGEO(nam_montgomerycountymd_crash_reporting_incidents_data, "nam-montgomerycountymd-crash-reporting-incidents-data", "Montgomery County, Maryland — Crash Reporting - Incidents Data", "Montgomery County, Maryland — Crash Reporting - Incidents Data",
  "us_local", "transport",
  "https://data.montgomerycountymd.gov/resource/bhju-22kf.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"md\",\"transport\",\"opendata\"]", 1800,
  "Socrata open-data feed from Montgomery County, Maryland: Crash Reporting - Incidents Data, one JSON record per row.");

VGEO(nam_montgomerycountymd_dps_general_permits, "nam-montgomerycountymd-dps-general-permits", "Montgomery County, Maryland — DPS - General Permits", "Montgomery County, Maryland — DPS - General Permits",
  "us_local", "economy",
  "https://data.montgomerycountymd.gov/resource/q2rb-en4r.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"md\",\"economy\",\"opendata\"]", 21600,
  "Socrata open-data feed from Montgomery County, Maryland: DPS - General Permits, one JSON record per row.");

VGEO(nam_nola_truck_routes, "nam-nola-truck-routes", "City of New Orleans — Truck Routes", "City of New Orleans — Truck Routes",
  "us_local", "transport",
  "https://data.nola.gov/resource/8vmv-aipz.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"la\",\"transport\",\"opendata\"]", 21600,
  "Socrata open-data feed from City of New Orleans: Truck Routes, one JSON record per row.");

VGEO(nam_norfolk_better_buildings_challenge_properties, "nam-norfolk-better-buildings-challenge-properties", "City of Norfolk, Virginia — Better Buildings Challenge Properties", "City of Norfolk, Virginia — Better Buildings Challenge Properties",
  "us_local", "economy",
  "https://data.norfolk.gov/resource/ykcq-76im.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"va\",\"economy\",\"opendata\"]", 21600,
  "Socrata open-data feed from City of Norfolk, Virginia: Better Buildings Challenge Properties, one JSON record per row.");

VGEO(nam_norfolk_blocked_railroad_crossing_incident_repor, "nam-norfolk-blocked-railroad-crossing-incident-repor", "City of Norfolk, Virginia — Blocked Railroad Crossing Incident Reports", "City of Norfolk, Virginia — Blocked Railroad Crossing Incident Reports",
  "us_local", "crime",
  "https://data.norfolk.gov/resource/bguc-8iwd.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"va\",\"crime\",\"opendata\"]", 3600,
  "Socrata open-data feed from City of Norfolk, Virginia: Blocked Railroad Crossing Incident Reports, one JSON record per row.");

VGEO(nam_norfolk_streetlights_and_outages_reports, "nam-norfolk-streetlights-and-outages-reports", "City of Norfolk, Virginia — Streetlights and Outages Reports", "City of Norfolk, Virginia — Streetlights and Outages Reports",
  "us_local", "energy",
  "https://data.norfolk.gov/resource/5qpt-tdve.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"va\",\"energy\",\"opendata\"]", 3600,
  "Socrata open-data feed from City of Norfolk, Virginia: Streetlights and Outages Reports, one JSON record per row.");

/* Registry sweep 2026-10-22: 7 of 10000 records a run collapsed onto a uid
   already written (0.1%). VJSON/VGEO's id precedence list finds no field in
   this Socrata record, so it fell back and keyed on content. Measured over two
   pages before the key was touched: 1,000 of 1,000 distinct — 311's own unique key. */
VJSON_IDKEYS(nam_nyc_311_open, "nam-nyc-311-open", "City of New York — 311 service requests (most recent)", "City of New York — 311 service requests (most recent)",
  "us_local", "opendata",
  "https://data.cityofnewyork.us/resource/erm2-nwe9.json?$limit=500&$order=created_date%20DESC",
  "",
  "en", "[\"usa\",\"ny\",\"311\",\"opendata\"]", 1800,
  "The newest 311 complaints filed in New York City with type, agency, borough and coordinates.",
  "unique_key");

VJSON(nam_nyc_air_quality, "nam-nyc-air-quality", "City of New York — community air quality surveillance", "City of New York — community air quality surveillance",
  "us_local", "environment",
  "https://data.cityofnewyork.us/resource/c3uy-2p5r.json?$limit=1000&$order=:id",
  "",
  "en", "[\"usa\",\"ny\",\"environment\",\"air\"]", 86400,
  "Neighbourhood-level air pollutant measurements from the NYC Community Air Survey.");

VJSON(nam_nyc_arrests_ytd, "nam-nyc-arrests-ytd", "City of New York — NYPD arrests year to date", "City of New York — NYPD arrests year to date",
  "us_local", "crime",
  "https://data.cityofnewyork.us/resource/uip8-fykc.json?$limit=500&$order=:id",
  "",
  "en", "[\"usa\",\"ny\",\"crime\",\"police\"]", 21600,
  "Year-to-date NYPD arrest records with offence description, precinct and location.");

/* Registry sweep 2026-10-22: 2578 of 10000 records a run collapsed onto a uid
   already written (25.8%). VJSON/VGEO's id precedence list finds no field in
   this Socrata record, so it fell back and keyed on content. Measured over two
   pages before the key was touched: 1,000 of 1,000 distinct — NYPD's collision id. */
VJSON_IDKEYS(nam_nyc_collisions_recent, "nam-nyc-collisions-recent", "City of New York — motor vehicle collisions (most recent)", "City of New York — motor vehicle collisions (most recent)",
  "us_local", "transport",
  "https://data.cityofnewyork.us/resource/h9gi-nx95.json?$limit=500&$order=crash_date%20DESC",
  "",
  "en", "[\"usa\",\"ny\",\"transport\",\"crashes\"]", 3600,
  "Latest NYPD collision reports with injuries, fatalities, contributing factor and crash coordinates.",
  "collision_id");

/* Registry sweep 2026-10-22: 2530 of 10000 records a run collapsed onto a uid
   already written (25.3%). VJSON/VGEO's id precedence list finds no field in
   this Socrata record, so it fell back and keyed on content. Measured over two
   pages before the key was touched: 1,000 of 1,000 distinct — the permit sequence number. */
VJSON_IDKEYS(nam_nyc_dob_permits, "nam-nyc-dob-permits", "City of New York — DOB permit issuance", "City of New York — DOB permit issuance",
  "us_local", "economy",
  "https://data.cityofnewyork.us/resource/ipu4-2q9a.json?$limit=500&$order=:id",
  "",
  "en", "[\"usa\",\"ny\",\"permits\",\"economy\"]", 21600,
  "Building permits issued by the NYC Department of Buildings with work type, owner and address.",
  "permit_si_no");

VJSON(nam_nyc_hpd_violations, "nam-nyc-hpd-violations", "City of New York — HPD housing maintenance violations", "City of New York — HPD housing maintenance violations",
  "us_local", "economy",
  "https://data.cityofnewyork.us/resource/wvxf-dwi5.json?$limit=500&$order=:id",
  "",
  "en", "[\"usa\",\"ny\",\"housing\",\"economy\"]", 21600,
  "Open housing-code violations issued against NYC buildings, with class, status and address.");

/* Registry sweep 2026-10-22: 1 of 10000 records a run collapsed onto a uid
   already written (0.0%). VJSON/VGEO's id precedence list finds no field in
   this Socrata record, so it fell back and keyed on content. Measured over two
   pages before the key was touched: 1,000 of 1,000 distinct — 311's own unique key; this row is the same resource as nam-nyc-311-open under a complaint_type filter. */
VJSON_IDKEYS(nam_nyc_noise_complaints, "nam-nyc-noise-complaints", "City of New York — 311 noise complaints", "City of New York — 311 noise complaints",
  "us_local", "opendata",
  "https://data.cityofnewyork.us/resource/erm2-nwe9.json?$limit=500&$where=complaint_type%20like%20%27%25Noise%25%27&$order=:id",
  "",
  "en", "[\"usa\",\"ny\",\"311\",\"opendata\"]", 3600,
  "311 records filtered to noise complaints, a fine-grained proxy for nightlife, construction and unrest activity.",
  "unique_key");

VRSS(nam_nyc_parks_events, "nam-nyc-parks-events", "NYC Parks — public events calendar", "NYC Parks — public events calendar",
  "us_local", "opendata",
  "https://www.nycgovparks.org/xml/events_300_rss.xml",
  "en", "[\"usa\",\"ny\",\"events\",\"opendata\"]", 21600,
  "Scheduled events in New York City parks with location, borough and time, useful for crowd and gathering context.");

/* Registry sweep 2026-10-22: 1887 of 10000 records a run collapsed onto a uid
   already written (18.9%). VJSON/VGEO's id precedence list finds no field in
   this Socrata record, so it fell back and keyed on content. Measured over two
   pages before the key was touched: 1,000 of 1,000 distinct — the record is one VIOLATION of one inspection of one restaurant: camis alone is 968 of 1,000 and camis+inspection_date 988. The sample's whole-record distinctness is also 1,000, so nothing here is an upstream duplicate. */
VJSON_IDKEYS(nam_nyc_restaurant_inspections, "nam-nyc-restaurant-inspections", "City of New York — restaurant inspection results", "City of New York — restaurant inspection results",
  "us_local", "health",
  "https://data.cityofnewyork.us/resource/43nn-pn8j.json?$limit=500&$order=:id",
  "",
  "en", "[\"usa\",\"ny\",\"health\",\"inspections\"]", 21600,
  "DOHMH food-service inspection results with violation codes, scores and grades per establishment.",
  "camis+inspection_date+violation_code");

VJSON(nam_nyc_shootings_ytd, "nam-nyc-shootings-ytd", "City of New York — NYPD shooting incidents year to date", "City of New York — NYPD shooting incidents year to date",
  "us_local", "crime",
  "https://data.cityofnewyork.us/resource/5ucz-vwe8.json?$limit=500&$order=:id",
  "",
  "en", "[\"usa\",\"ny\",\"crime\",\"police\"]", 21600,
  "Year-to-date shooting incidents recorded by NYPD, including borough, precinct and coordinates.");

VGEO(nam_oaklandca_blue_disabled_parking_zones, "nam-oaklandca-blue-disabled-parking-zones", "City of Oakland, California — Blue Disabled Parking Zones", "City of Oakland, California — Blue Disabled Parking Zones",
  "us_local", "transport",
  "https://data.oaklandca.gov/resource/cnpw-bvzz.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ca\",\"transport\",\"opendata\"]", 86400,
  "Socrata open-data feed from City of Oakland, California: Blue Disabled Parking Zones, one JSON record per row.");

VGEO(nam_oaklandca_oakland_bike_parking, "nam-oaklandca-oakland-bike-parking", "City of Oakland, California — Oakland Bike Parking", "City of Oakland, California — Oakland Bike Parking",
  "us_local", "transport",
  "https://data.oaklandca.gov/resource/kd2g-wuf6.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ca\",\"transport\",\"opendata\"]", 21600,
  "Socrata open-data feed from City of Oakland, California: Oakland Bike Parking, one JSON record per row.");

VGEO(nam_oaklandca_residential_parking_permit_zones, "nam-oaklandca-residential-parking-permit-zones", "City of Oakland, California — Residential Parking Permit Zones", "City of Oakland, California — Residential Parking Permit Zones",
  "us_local", "transport",
  "https://data.oaklandca.gov/resource/su5x-2u99.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ca\",\"transport\",\"opendata\"]", 86400,
  "Socrata open-data feed from City of Oakland, California: Residential Parking Permit Zones, one JSON record per row.");

VGEO(nam_princegeorgescountymd_county_click_311_qalert, "nam-princegeorgescountymd-county-click-311-qalert", "Prince George's County, Maryland — County Click 311 - QAlert", "Prince George's County, Maryland — County Click 311 - QAlert",
  "us_local", "opendata",
  "https://data.princegeorgescountymd.gov/resource/y8p6-7ur2.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"md\",\"opendata\",\"opendata\"]", 3600,
  "Socrata open-data feed from Prince George's County, Maryland: County Click 311 - QAlert, one JSON record per row.");

VGEO(nam_providenceri_providence_tree_dataset, "nam-providenceri-providence-tree-dataset", "City of Providence, Rhode Island — Providence Tree Dataset", "City of Providence, Rhode Island — Providence Tree Dataset",
  "us_local", "environment",
  "https://data.providenceri.gov/resource/b77h-59tz.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ri\",\"environment\",\"opendata\"]", 21600,
  "Socrata open-data feed from City of Providence, Rhode Island: Providence Tree Dataset, one JSON record per row.");

/* 8,000 emitted / 7,356 stored (registry sweep). MEASURED AND LEFT ALONE,
   2026-10-23. No id column, so the question is whether a composite beats the
   content-hash fallback — and over the engine's whole 8,000-feature read it
   does not:

       distinct WHOLE features                                 7,355
       distinct property sets                                  7,355
       the fallback actually stores                            7,356
       property_address_address+species+diameter_in_inches     7,323

   An 800-feature sample said that key hit the maximum exactly (765 of 765);
   shipping on it cost 32 records a run, confirmed in the engine (7,324 against
   the fallback's 7,356). "Two pages can be too few", and the fallback wins.
   One thing from the pass is worth keeping: the GEOMETRY is by far the weakest
   key here, 5,261 distinct of 8,000, because the points look snapped to the
   street address — so several trees share a coordinate and a geo row's
   identity is not where it is.
   Its sibling nam-providenceri-providence-tree-dataset (a different dataset)
   keeps the fallback for the same reason, measured earlier. */
VGEO(nam_providenceri_providence_tree_inventory, "nam-providenceri-providence-tree-inventory", "City of Providence, Rhode Island — Providence Tree Inventory", "City of Providence, Rhode Island — Providence Tree Inventory",
  "us_local", "environment",
  "https://data.providenceri.gov/resource/uv9w-h8i4.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ri\",\"environment\",\"opendata\"]", 86400,
  "Socrata open-data feed from City of Providence, Rhode Island: Providence Tree Inventory, one JSON record per row.");

VGEO(nam_providenceri_pvd_parcels, "nam-providenceri-pvd-parcels", "City of Providence, Rhode Island — PVD Parcels", "City of Providence, Rhode Island — PVD Parcels",
  "us_local", "economy",
  "https://data.providenceri.gov/resource/nyp3-msmz.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ri\",\"economy\",\"opendata\"]", 86400,
  "Socrata open-data feed from City of Providence, Rhode Island: PVD Parcels, one JSON record per row.");

VGEO(nam_sccgov_schools_areas, "nam-sccgov-schools-areas", "Santa Clara County, California — Schools Areas", "Santa Clara County, California — Schools Areas",
  "us_local", "opendata",
  "https://data.sccgov.org/resource/ndvh-brsx.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ca\",\"opendata\",\"opendata\"]", 21600,
  "Socrata open-data feed from Santa Clara County, California: Schools Areas, one JSON record per row.");

VGEO(nam_sccgov_zoning, "nam-sccgov-zoning", "Santa Clara County, California — Zoning", "Santa Clara County, California — Zoning",
  "us_local", "opendata",
  "https://data.sccgov.org/resource/tuy6-wtsq.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ca\",\"opendata\",\"opendata\"]", 86400,
  "Socrata open-data feed from Santa Clara County, California: Zoning, one JSON record per row.");

VJSON(nam_seattle_911_fire, "nam-seattle-911-fire", "City of Seattle — real-time fire 911 dispatch", "City of Seattle — real-time fire 911 dispatch",
  "us_local", "hazard",
  "https://data.seattle.gov/resource/kzjm-xkqj.json?$limit=500&$order=:id",
  "",
  "en", "[\"usa\",\"wa\",\"fire\",\"hazard\"]", 1800,
  "Seattle Fire Department 911 dispatches with incident type, address and coordinates.");

/* Registry sweep 2026-10-22: 218 of 10000 records a run collapsed onto a uid
   already written (2.2%). VJSON/VGEO's id precedence list finds no field in
   this Socrata record, so it fell back and keyed on content. Measured over two
   pages before the key was touched: 1,000 of 1,000 distinct — the permit number. NOT `link`, which a minimal-key search finds unique and which is a URL. */
VJSON_IDKEYS(nam_seattle_building_permits, "nam-seattle-building-permits", "City of Seattle — building permits", "City of Seattle — building permits",
  "us_local", "economy",
  "https://data.seattle.gov/resource/76t5-zqzr.json?$limit=500&$order=:id",
  "",
  "en", "[\"usa\",\"wa\",\"permits\",\"economy\"]", 21600,
  "Seattle construction permits with work type, value, status and location.",
  "permitnum");

/* Registry sweep 2026-10-22: 16 of 10000 records a run collapsed onto a uid
   already written (0.2%). VJSON/VGEO's id precedence list finds no field in
   this Socrata record, so it fell back and keyed on content. Measured over two
   pages before the key was touched: 1,000 of 1,000 distinct — the offense id. NOT `offense_date`, which a minimal-key search finds unique in one sample and which is a TIMESTAMP. */
VJSON_IDKEYS(nam_seattle_police_reports, "nam-seattle-police-reports", "City of Seattle — police report offences", "City of Seattle — police report offences",
  "us_local", "crime",
  "https://data.seattle.gov/resource/tazs-3rd5.json?$limit=500&$order=:id",
  "",
  "en", "[\"usa\",\"wa\",\"crime\",\"police\"]", 21600,
  "SPD offence records with NIBRS classification, precinct, beat and reporting time.",
  "offense_id");

VJSON(nam_septa_busdetours, "nam-septa-busdetours", "SEPTA — active bus detours", "SEPTA — active bus detours",
  "us_local", "transport",
  "https://www3.septa.org/api/BusDetours/index.php",
  "",
  "en", "[\"usa\",\"pa\",\"transit\",\"transport\"]", 1800,
  "Bus routes currently on detour with the reason, start date and affected street segments.");

/* nam-sf-311-cases: moved to collectors/pivot/table/hp3_sfopendata.c (2026-10-13), read whole across runs.
   This branch had keyed the VJSON row on `service_request_id` from the registry
   sweep (73 of 10,000 collapsing, then 1,000 of 1,000 distinct over two pages);
   the hp row it moved to keys on the same field, measured independently. */

VJSON(nam_sf_air_traffic_passenger, "nam-sf-air-traffic-passenger", "City and County of San Francisco — SFO passenger traffic", "City and County of San Francisco — SFO passenger traffic",
  "us_local", "economy",
  "https://data.sf.gov/resource/rkru-6vcg.json?$limit=5000&$order=:id",
  "",
  "en", "[\"usa\",\"ca\",\"aviation\",\"economy\"]", 86400,
  "Monthly SFO passenger counts by airline, terminal and geographic region.");

/* nam-sf-building-permits: moved to collectors/pivot/table/hp3_sfopendata.c (2026-10-13), read whole across runs.
   This branch had keyed the VJSON row on `record_id` (60 of 10,000 collapsing,
   then 1,000 of 1,000 distinct); the hp row keys on the same field. */

/* nam-sf-fire-calls: moved to collectors/pivot/table/hp3_sfopendata.c (2026-10-13), read whole across runs.
   This branch had keyed the VJSON row on `rowid` (2 of 10,000 collapsing, then
   1,000 of 1,000 distinct); the hp row keys on the same field. */

VJSON(nam_sf_police_incidents, "nam-sf-police-incidents", "City and County of San Francisco — police incident reports", "City and County of San Francisco — police incident reports",
  "us_local", "crime",
  "https://data.sf.gov/resource/wg3w-h783.json?$limit=5000&$order=:id",
  "",
  "en", "[\"usa\",\"ca\",\"crime\",\"police\"]", 21600,
  "SFPD incident reports since 2018 with category, resolution, district and coordinates.");

/* Registry sweep 2026-10-22: 320 of 8000 records a run collapsed onto a uid
   already written (4.0%). VJSON/VGEO's id precedence list finds no field in
   this Socrata record, so it fell back and keyed on content. Measured over two
   pages before the key was touched: 800 of 800 distinct — the impound number. */
VGEO_IDKEYS(nam_sonomacounty_ca_animal_shelter_intake_and_outcome, "nam-sonomacounty-ca-animal-shelter-intake-and-outcome", "Sonoma County, California — Animal Shelter Intake and Outcome", "Sonoma County, California — Animal Shelter Intake and Outcome",
  "us_local", "opendata",
  "https://data.sonomacounty.ca.gov/resource/924a-vesw.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ca\",\"opendata\",\"opendata\"]", 21600,
  "Socrata open-data feed from Sonoma County, California: Animal Shelter Intake and Outcome, one JSON record per row.",
  "impound_number");

VGEO(nam_sonomacounty_ca_sonoma_county_rebuilding_permits, "nam-sonomacounty-ca-sonoma-county-rebuilding-permits", "Sonoma County, California — Sonoma County Rebuilding Permits", "Sonoma County, California — Sonoma County Rebuilding Permits",
  "us_local", "economy",
  "https://data.sonomacounty.ca.gov/resource/652y-5ihx.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ca\",\"economy\",\"opendata\"]", 21600,
  "Socrata open-data feed from Sonoma County, California: Sonoma County Rebuilding Permits, one JSON record per row.");

VGEO(nam_sonomacounty_ca_sonoma_county_sheriff_s_office_incident, "nam-sonomacounty-ca-sonoma-county-sheriff-s-office-incident", "Sonoma County, California — Sonoma County Sheriff's Office Incident Data", "Sonoma County, California — Sonoma County Sheriff's Office Incident Data",
  "us_local", "crime",
  "https://data.sonomacounty.ca.gov/resource/3rsj-iche.geojson?$limit=400&$order=:id",
  "en", "[\"usa\",\"ca\",\"crime\",\"opendata\"]", 3600,
  "Socrata open-data feed from Sonoma County, California: Sonoma County Sheriff's Office Incident Data, one JSON record per row.");
