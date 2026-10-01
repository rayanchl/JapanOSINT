/* Verified-live us_civilian sources (14), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(global_nominatim_details, "global-nominatim-details", "Nominatim place details", "Nominatim place details",
  "us_civilian", "civilian",
  "https://nominatim.openstreetmap.org/details.php?osmtype=W&osmid=23733659&class=building&format=json&addressdetails=1&hierarchy=1&linkedplaces=1&keywords=1",
  ".", /* the place IS the record. Path keywords.address emitted 229 search-index token rows (224 stored, 2026-09-14) and discarded the names, addresstags, hierarchy, centroid and geometry this description promises; "." keeps the whole document on one row */
  "en", "[\"us\",\"civilian\",\"batch16\",\"high-penetrancy\"]", 21600,
  "Deepest Nominatim record: place_id and parent_place_id, all names including old_name, addresstags, calculated_postcode, indexed_date, importance, rank_address/rank_search, isarea, centroid, geometry, admin hierarchy and linked places. The genuine detail hop behind a search hit.");

VJSON(us_brookline_311_open311_services, "us-brookline-311-open311-services", "Brookline MA 311 - service catalogue", "Brookline MA 311 - service catalogue",
  "us_civilian", "civilian",
  "https://spot.brooklinema.gov/open311/v2/services.json",
  "",
  "en", "[\"us\",\"civilian\",\"batch16\",\"high-penetrancy\"]", 21600,
  "22 service_code/service_name/description/group entries (Damaged Sign, Dead Animals, Graffiti, Public Works...) used to filter the request feed.");

/* Carto rows have no "id"; the table's row key is "cartodb_id" (live
 * 2026-09-06, present and distinct on every row). Sweep: 5 emitted, 1 stored. */
VJSON_KEYED(us_phl_carto_shootings, "us-phl-carto-shootings", "Philadelphia shooting victims", "Philadelphia shooting victims",
  "us_civilian", "civilian",
  "https://phl.carto.com/api/v2/sql?q=SELECT%20*%20FROM%20shootings%20LIMIT%205",
  "rows",
  "en", "[\"us\",\"civilian\",\"batch16\",\"high-penetrancy\"]", 21600,
  "Per-victim record: year, dist, dc_key, code, date_, time, race, sex, latino, age, wound, officer_involved, offender_injured/deceased, location. Joins to incidents_part1_part2 on dc_key.",
  "cartodb_id");

/* Open311 records identify themselves by "service_request_id" (live
 * 2026-09-06), which is not on the fixed id precedence list; the hash
 * fallback collapsed same-titled cases (sweep: 100 emitted, 97 stored). */
VJSON_KEYED(us_sf_311_open311_requests, "us-sf-311-open311-requests", "San Francisco 311 - Open311 GeoReport v2 requests", "San Francisco 311 - Open311 GeoReport v2 requests",
  "us_civilian", "civilian",
  "https://mobile311.sfgov.org/open311/v2/requests.json?page_size=5",
  "",
  "en", "[\"us\",\"civilian\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 21600,
  "Live 311 case feed: service_request_id, status, status_notes, service_name, service_code, description (free text, frequently contains licence plates, vehicle descriptions and named complaints), requested_datetime, updated_datetime, address, lat/long, media_url. Supports &service_code=, &start_date=, &end_date=, &status= filters. One of the only surviving Open311 GeoReport servers in the US.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.",
  "service_request_id");

