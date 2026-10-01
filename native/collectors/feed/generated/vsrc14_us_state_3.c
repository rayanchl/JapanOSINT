/* Verified-live us_state sources (11), part 3.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* `order=createdAt`: the catalog defaults to relevance order, which is not
 * stable across offset pages (live 2026-09-06: 2 of 100 assets on page 2
 * were repeats of page 1, and as many were skipped; with createdAt the
 * overlap is 0). Seven of these eleven rows measured 2-12% collapsed. */
VJSON(us_socrata_search_school_enrollment, "us-socrata-search-school-enrollment", "Socrata Cross-Portal Search — School Enrollment", "Socrata 横断検索 school enrollment",
  "us_state", "education",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=school%20enrollment&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"school-enrollment\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'school enrollment': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_school_performance, "us-socrata-search-school-performance", "Socrata Cross-Portal Search — School Performance", "Socrata 横断検索 school performance",
  "us_state", "education",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=school%20performance&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"school-performance\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'school performance': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_street_closures, "us-socrata-search-street-closures", "Socrata Cross-Portal Search — Street Closures", "Socrata 横断検索 street closures",
  "us_state", "transport",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=street%20closures&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"street-closures\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'street closures': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_student_demographics, "us-socrata-search-student-demographics", "Socrata Cross-Portal Search — Student Demographics", "Socrata 横断検索 student demographics",
  "us_state", "education",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=student%20demographics&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"student-demographics\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'student demographics': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_surveillance_technology, "us-socrata-search-surveillance-technology", "Socrata Cross-Portal Search — Surveillance Technology", "Socrata 横断検索 surveillance technology",
  "us_state", "security",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=surveillance%20technology&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"surveillance-technology\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'surveillance technology': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_traffic_collisions, "us-socrata-search-traffic-collisions", "Socrata Cross-Portal Search — Traffic Collisions", "Socrata 横断検索 traffic collisions",
  "us_state", "transport",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=traffic%20collisions&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"traffic-collisions\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'traffic collisions': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_transit_ridership, "us-socrata-search-transit-ridership", "Socrata Cross-Portal Search — Transit Ridership", "Socrata 横断検索 transit ridership",
  "us_state", "transport",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=transit%20ridership&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"transit-ridership\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'transit ridership': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_use_of_force, "us-socrata-search-use-of-force", "Socrata Cross-Portal Search — Use Of Force", "Socrata 横断検索 use of force",
  "us_state", "crime",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=use%20of%20force&order=dataset_id", /* createdAt ties re-served one asset across offset pages and skipped another (1,304 fetched, 1,303 distinct); dataset_id is unique: 1,304/1,304, 2026-09-15 */
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"use-of-force\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'use of force': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_vacant_properties, "us-socrata-search-vacant-properties", "Socrata Cross-Portal Search — Vacant Properties", "Socrata 横断検索 vacant properties",
  "us_state", "geospatial",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=vacant%20properties&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"vacant-properties\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'vacant properties': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_vendor_payments, "us-socrata-search-vendor-payments", "Socrata Cross-Portal Search — Vendor Payments", "Socrata 横断検索 vendor payments",
  "us_state", "procurement",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=vendor%20payments&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"vendor-payments\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'vendor payments': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_water_testing, "us-socrata-search-water-testing", "Socrata Cross-Portal Search — Water Testing", "Socrata 横断検索 water testing",
  "us_state", "environment",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=water%20testing&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"water-testing\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'water testing': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");
