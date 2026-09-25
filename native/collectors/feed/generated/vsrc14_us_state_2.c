/* Verified-live us_state sources (60), part 2.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "_vjson_idkeys.inc"

VJSON(us_socrata_discovery_pennsylvania, "us-socrata-discovery-pennsylvania", "Commonwealth of Pennsylvania Open Data — Asset Discovery", "ペンシルベニア州 データ資産一覧",
  "us_state", "registry",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&domains=data.pa.gov",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"discovery\",\"pennsylvania\"]", 86400,
  "Every dataset, chart, map, filtered view and file published on data.pa.gov, via the Socrata federated discovery API: identifier, owner, category, column schema and update time.");

VJSON(us_socrata_discovery_seattle, "us-socrata-discovery-seattle", "City of Seattle Open Data — Asset Discovery", "シアトル市 データ資産一覧",
  "us_state", "registry",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&domains=data.seattle.gov&search_context=data.seattle.gov",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"discovery\",\"seattle\"]", 86400,
  "Every dataset, chart, map, filtered view and file published on data.seattle.gov, via the Socrata federated discovery API: identifier, owner, category, column schema and update time.");

VJSON(us_socrata_discovery_sfgov, "us-socrata-discovery-sfgov", "City of San Francisco Open Data — Asset Discovery", "サンフランシスコ市 データ資産一覧",
  "us_state", "registry",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&domains=data.sfgov.org",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"discovery\",\"sfgov\"]", 86400,
  "Every dataset, chart, map, filtered view and file published on data.sfgov.org, via the Socrata federated discovery API: identifier, owner, category, column schema and update time.");

VJSON(us_socrata_discovery_texas, "us-socrata-discovery-texas", "State of Texas Open Data — Asset Discovery", "テキサス州 データ資産一覧",
  "us_state", "registry",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&domains=data.texas.gov",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"discovery\",\"texas\"]", 86400,
  "Every dataset, chart, map, filtered view and file published on data.texas.gov, via the Socrata federated discovery API: identifier, owner, category, column schema and update time.");

VJSON(us_socrata_discovery_washington, "us-socrata-discovery-washington", "Washington State Open Data — Asset Discovery", "ワシントン州 データ資産一覧",
  "us_state", "registry",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&domains=data.wa.gov",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"discovery\",\"washington\"]", 86400,
  "Every dataset, chart, map, filtered view and file published on data.wa.gov, via the Socrata federated discovery API: identifier, owner, category, column schema and update time.");

/* Socrata `/api/views/metadata/v1` pages on `limit` + `page`, NOT on `offset`.
 * Measured 2026-09-11 against data.bts.gov: `?limit=100` then `&offset=100`
 * re-serves the SAME 100 records (identical ids), which is why all 31 rows
 * below reported 200 emitted / 100 stored — one page counted twice, and the
 * other 496 of the portal's 596 assets never fetched. `&page=2` does advance
 * (pages 1-6 = 596 records, 596 distinct, page 7 empty).
 * jsonlist_emit_paged() picks its cursor from the page-size parameter the URL
 * declares, and `limit` is paired with `offset` there; `page_size` is the
 * pair that advances `page`. Declaring both — `limit=500` (which this API
 * honours, bounding the page) alongside `page_size=500` (which it ignores,
 * and which tells the walk the page size) — makes the walk follow the cursor
 * the upstream actually implements. Verified disjoint pages on
 * data.cityofnewyork.us, data.ny.gov, www.dallasopendata.com, healthdata.gov
 * (500 + 500, zero id overlap) and opendata.fcc.gov (100, then empty). */
VJSON(us_socrata_metadata_austin, "us-socrata-metadata-austin", "City of Austin Open Data — Asset Metadata", "オースティン市 メタデータ",
  "us_state", "opendata",
  "https://data.austintexas.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"austin\"]", 86400,
  "Portal-side metadata for the assets hosted on data.austintexas.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_bts, "us-socrata-metadata-bts", "US Bureau of Transportation Statistics — Asset Metadata", "米国運輸統計局 メタデータ",
  "us_state", "opendata",
  "https://data.bts.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"bts\"]", 86400,
  "Portal-side metadata for the assets hosted on data.bts.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_cambridge, "us-socrata-metadata-cambridge", "City of Cambridge Open Data — Asset Metadata", "ケンブリッジ市 メタデータ",
  "us_state", "opendata",
  "https://data.cambridgema.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"cambridge\"]", 86400,
  "Portal-side metadata for the assets hosted on data.cambridgema.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_cdc, "us-socrata-metadata-cdc", "US CDC Open Data — Asset Metadata", "米国CDC メタデータ",
  "us_state", "opendata",
  "https://data.cdc.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"cdc\"]", 86400,
  "Portal-side metadata for the assets hosted on data.cdc.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_chicago, "us-socrata-metadata-chicago", "City of Chicago Open Data — Asset Metadata", "シカゴ市 メタデータ",
  "us_state", "opendata",
  "https://data.cityofchicago.org/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"chicago\"]", 86400,
  "Portal-side metadata for the assets hosted on data.cityofchicago.org: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_chronicdata, "us-socrata-metadata-chronicdata", "US CDC Chronic Disease Data — Asset Metadata", "米国CDC 慢性疾患 メタデータ",
  "us_state", "opendata",
  "https://chronicdata.cdc.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"chronicdata\"]", 86400,
  "Portal-side metadata for the assets hosted on chronicdata.cdc.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_colorado, "us-socrata-metadata-colorado", "State of Colorado Open Data — Asset Metadata", "コロラド州 メタデータ",
  "us_state", "opendata",
  "https://data.colorado.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"colorado\"]", 86400,
  "Portal-side metadata for the assets hosted on data.colorado.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_connecticut, "us-socrata-metadata-connecticut", "State of Connecticut Open Data — Asset Metadata", "コネチカット州 メタデータ",
  "us_state", "opendata",
  "https://data.ct.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"connecticut\"]", 86400,
  "Portal-side metadata for the assets hosted on data.ct.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_dallas, "us-socrata-metadata-dallas", "City of Dallas Open Data — Asset Metadata", "ダラス市 メタデータ",
  "us_state", "opendata",
  "https://www.dallasopendata.com/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"dallas\"]", 86400,
  "Portal-side metadata for the assets hosted on www.dallasopendata.com: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_dot, "us-socrata-metadata-dot", "US DOT Open Data — Asset Metadata", "米国運輸省 メタデータ",
  "us_state", "opendata",
  "https://data.transportation.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"dot\"]", 86400,
  "Portal-side metadata for the assets hosted on data.transportation.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_fcc, "us-socrata-metadata-fcc", "US FCC Open Data — Asset Metadata", "米国FCC メタデータ",
  "us_state", "opendata",
  "https://opendata.fcc.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"fcc\"]", 86400,
  "Portal-side metadata for the assets hosted on opendata.fcc.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_healthdata, "us-socrata-metadata-healthdata", "US HealthData.gov — Asset Metadata", "米国保健データ メタデータ",
  "us_state", "opendata",
  "https://healthdata.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"healthdata\"]", 86400,
  "Portal-side metadata for the assets hosted on healthdata.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_illinois, "us-socrata-metadata-illinois", "State of Illinois Open Data — Asset Metadata", "イリノイ州 メタデータ",
  "us_state", "opendata",
  "https://data.illinois.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"illinois\"]", 86400,
  "Portal-side metadata for the assets hosted on data.illinois.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_kansascity, "us-socrata-metadata-kansascity", "Kansas City Open Data — Asset Metadata", "カンザスシティ メタデータ",
  "us_state", "opendata",
  "https://data.kcmo.org/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"kansascity\"]", 86400,
  "Portal-side metadata for the assets hosted on data.kcmo.org: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_kingcounty, "us-socrata-metadata-kingcounty", "King County Open Data — Asset Metadata", "キング郡 メタデータ",
  "us_state", "opendata",
  "https://data.kingcounty.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"kingcounty\"]", 86400,
  "Portal-side metadata for the assets hosted on data.kingcounty.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_losangeles, "us-socrata-metadata-losangeles", "City of Los Angeles Open Data — Asset Metadata", "ロサンゼルス市 メタデータ",
  "us_state", "opendata",
  "https://data.lacity.org/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"losangeles\"]", 86400,
  "Portal-side metadata for the assets hosted on data.lacity.org: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_maryland, "us-socrata-metadata-maryland", "State of Maryland Open Data — Asset Metadata", "メリーランド州 メタデータ",
  "us_state", "opendata",
  "https://opendata.maryland.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"maryland\"]", 86400,
  "Portal-side metadata for the assets hosted on opendata.maryland.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_michigan, "us-socrata-metadata-michigan", "State of Michigan Open Data — Asset Metadata", "ミシガン州 メタデータ",
  "us_state", "opendata",
  "https://data.michigan.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"michigan\"]", 86400,
  "Portal-side metadata for the assets hosted on data.michigan.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_missouri, "us-socrata-metadata-missouri", "State of Missouri Open Data — Asset Metadata", "ミズーリ州 メタデータ",
  "us_state", "opendata",
  "https://data.mo.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"missouri\"]", 86400,
  "Portal-side metadata for the assets hosted on data.mo.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_montgomery, "us-socrata-metadata-montgomery", "Montgomery County Open Data — Asset Metadata", "モンゴメリー郡 メタデータ",
  "us_state", "opendata",
  "https://data.montgomerycountymd.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"montgomery\"]", 86400,
  "Portal-side metadata for the assets hosted on data.montgomerycountymd.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_newjersey, "us-socrata-metadata-newjersey", "State of New Jersey Open Data — Asset Metadata", "ニュージャージー州 メタデータ",
  "us_state", "opendata",
  "https://data.nj.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"newjersey\"]", 86400,
  "Portal-side metadata for the assets hosted on data.nj.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_nyc, "us-socrata-metadata-nyc", "New York City Open Data — Asset Metadata", "ニューヨーク市 メタデータ",
  "us_state", "opendata",
  "https://data.cityofnewyork.us/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"nyc\"]", 86400,
  "Portal-side metadata for the assets hosted on data.cityofnewyork.us: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_nyhealth, "us-socrata-metadata-nyhealth", "New York State Health Data — Asset Metadata", "ニューヨーク州保健 メタデータ",
  "us_state", "opendata",
  "https://health.data.ny.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"nyhealth\"]", 86400,
  "Portal-side metadata for the assets hosted on health.data.ny.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_nystate, "us-socrata-metadata-nystate", "New York State Open Data — Asset Metadata", "ニューヨーク州 メタデータ",
  "us_state", "opendata",
  "https://data.ny.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"nystate\"]", 86400,
  "Portal-side metadata for the assets hosted on data.ny.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_oakland, "us-socrata-metadata-oakland", "City of Oakland Open Data — Asset Metadata", "オークランド市 メタデータ",
  "us_state", "opendata",
  "https://data.oaklandca.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"oakland\"]", 86400,
  "Portal-side metadata for the assets hosted on data.oaklandca.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_oregon, "us-socrata-metadata-oregon", "State of Oregon Open Data — Asset Metadata", "オレゴン州 メタデータ",
  "us_state", "opendata",
  "https://data.oregon.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"oregon\"]", 86400,
  "Portal-side metadata for the assets hosted on data.oregon.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_pennsylvania, "us-socrata-metadata-pennsylvania", "Commonwealth of Pennsylvania Open Data — Asset Metadata", "ペンシルベニア州 メタデータ",
  "us_state", "opendata",
  "https://data.pa.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"pennsylvania\"]", 86400,
  "Portal-side metadata for the assets hosted on data.pa.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_seattle, "us-socrata-metadata-seattle", "City of Seattle Open Data — Asset Metadata", "シアトル市 メタデータ",
  "us_state", "opendata",
  "https://data.seattle.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"seattle\"]", 86400,
  "Portal-side metadata for the assets hosted on data.seattle.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_sfgov, "us-socrata-metadata-sfgov", "City of San Francisco Open Data — Asset Metadata", "サンフランシスコ市 メタデータ",
  "us_state", "opendata",
  "https://data.sfgov.org/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"sfgov\"]", 86400,
  "Portal-side metadata for the assets hosted on data.sfgov.org: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_texas, "us-socrata-metadata-texas", "State of Texas Open Data — Asset Metadata", "テキサス州 メタデータ",
  "us_state", "opendata",
  "https://data.texas.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"texas\"]", 86400,
  "Portal-side metadata for the assets hosted on data.texas.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

VJSON(us_socrata_metadata_washington, "us-socrata-metadata-washington", "Washington State Open Data — Asset Metadata", "ワシントン州 メタデータ",
  "us_state", "opendata",
  "https://data.wa.gov/api/views/metadata/v1?limit=500&page_size=500&page=1",
  "",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"metadata\",\"washington\"]", 86400,
  "Portal-side metadata for the assets hosted on data.wa.gov: name, description, category, tags, attribution, licence, owner and provenance for each view.");

/* Discovery API offset paging is NOT deterministic without an explicit order:
 * live 2026-09-07, two fetches of `q=arrests&offset=100` returned different
 * result sets and page 2 re-served 7 of page 1's assets, which is where the
 * sweep's 2-8% per-query losses came from (assets have a unique permalink;
 * the same asset re-served twice upserts onto itself while the one it
 * displaced is never fetched). `order=createdAt` is accepted by the API and
 * makes the walk repeatable (identical pages on re-fetch, 0 overlap). */
VJSON(us_socrata_search_311_requests, "us-socrata-search-311-requests", "Socrata Cross-Portal Search — 311 Service Requests", "Socrata 横断検索 311 service requests",
  "us_state", "government",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=311%20service%20requests&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"311-requests\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching '311 service requests': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

/* Keyed on resource.id for the whole us-socrata-search-* family below. A
 * Socrata catalog/v1 result is {resource:{id,name,...}, classification:{...},
 * metadata:{domain}, permalink, link}: the four-by-four dataset id lives at
 * resource.id and NOT at the top level, so the jsonlist precedence list found
 * nothing and every row hashed (title, link, date). Two assets on different
 * portals routinely carry the same title, and those collapsed onto one uid.
 *
 * Measured live 2026-09-16, walking each row's own endpoint to exhaustion:
 * air-monitoring 751 records / 751 distinct resource.id / 751 distinct
 * serialisations, stored 750; calls-for-service 1,749 / 1,749 / 1,749, stored
 * 1,748; environmental-violations 245 / 245 / 245. broadband-access is included
 * for family consistency and its count does not move — 557 records but only 556
 * distinct serialisations AND 556 distinct resource.id, so its one collapse is
 * a byte-identical repeat the upstream itself serves twice. resource.id is
 * present on every record of all four. The dotted path needs VJSON_IDKEYS;
 * VJSON_KEYED takes a top-level field name only. */
VJSON_IDKEYS(us_socrata_search_air_monitoring, "us-socrata-search-air-monitoring", "Socrata Cross-Portal Search — Air Monitoring", "Socrata 横断検索 air monitoring",
  "us_state", "environment",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=air%20monitoring&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"air-monitoring\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'air monitoring': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.",
  "resource.id");

VJSON(us_socrata_search_arrests, "us-socrata-search-arrests", "Socrata Cross-Portal Search — Arrests", "Socrata 横断検索 arrests",
  "us_state", "crime",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=arrests&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"arrests\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'arrests': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

/* The cross-portal search rows below key on the hashed fallback and two of
 * them were flagged COLLISION at 557/556 and 1055/1054. Checked 2026-09-11 by
 * walking the offset pages to the declared resultSetSize: "broadband access"
 * returns 557 records of which 556 are distinct by resource.id AND by
 * permalink — the federated index itself hands the same asset back twice,
 * because `order=createdAt` has ties and the offset window straddles them.
 * "building permits" measured 1,058 of 1,058 distinct on a clean walk. The
 * single collapsed row per run is the upstream repeating a record, so the
 * dedupe is CORRECT and the key is left alone. */
VJSON(us_socrata_search_body_worn_camera, "us-socrata-search-body-worn-camera", "Socrata Cross-Portal Search — Body Worn Camera", "Socrata 横断検索 body worn camera",
  "us_state", "cameras",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=body%20worn%20camera&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"body-worn-camera\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'body worn camera': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

/* resource.id — see the measurement note on us-socrata-search-air-monitoring. */
VJSON_IDKEYS(us_socrata_search_broadband_access, "us-socrata-search-broadband-access", "Socrata Cross-Portal Search — Broadband Access", "Socrata 横断検索 broadband access",
  "us_state", "telecom",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=broadband%20access&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"broadband-access\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'broadband access': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.",
  "resource.id");

VJSON(us_socrata_search_budget_expenditures, "us-socrata-search-budget-expenditures", "Socrata Cross-Portal Search — Budget Expenditures", "Socrata 横断検索 budget expenditures",
  "us_state", "economy",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=budget%20expenditures&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"budget-expenditures\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'budget expenditures': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_building_permits, "us-socrata-search-building-permits", "Socrata Cross-Portal Search — Building Permits", "Socrata 横断検索 building permits",
  "us_state", "geospatial",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=building%20permits&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"building-permits\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'building permits': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_business_licenses, "us-socrata-search-business-licenses", "Socrata Cross-Portal Search — Business Licenses", "Socrata 横断検索 business licenses",
  "us_state", "corporate",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=business%20licenses&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"business-licenses\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'business licenses': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

/* resource.id — see the measurement note on us-socrata-search-air-monitoring. */
VJSON_IDKEYS(us_socrata_search_calls_for_service, "us-socrata-search-calls-for-service", "Socrata Cross-Portal Search — Calls For Service", "Socrata 横断検索 calls for service",
  "us_state", "crime",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=calls%20for%20service&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"calls-for-service\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'calls for service': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.",
  "resource.id");

VJSON(us_socrata_search_campaign_contributions, "us-socrata-search-campaign-contributions", "Socrata Cross-Portal Search — Campaign Contributions", "Socrata 横断検索 campaign contributions",
  "us_state", "transparency",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=campaign%20contributions&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"campaign-contributions\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'campaign contributions': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_contractor_registry, "us-socrata-search-contractor-registry", "Socrata Cross-Portal Search — Contractor Registry", "Socrata 横断検索 contractor registry",
  "us_state", "corporate",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=contractor%20registry&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"contractor-registry\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'contractor registry': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_cybersecurity_incidents, "us-socrata-search-cybersecurity-incidents", "Socrata Cross-Portal Search — Cybersecurity Incidents", "Socrata 横断検索 cybersecurity incidents",
  "us_state", "cyber",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=cybersecurity%20incidents&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"cybersecurity-incidents\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'cybersecurity incidents': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_emergency_shelters, "us-socrata-search-emergency-shelters", "Socrata Cross-Portal Search — Emergency Shelters", "Socrata 横断検索 emergency shelters",
  "us_state", "disaster",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=emergency%20shelters&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"emergency-shelters\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'emergency shelters': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

/* resource.id — see the measurement note on us-socrata-search-air-monitoring. */
VJSON_IDKEYS(us_socrata_search_environmental_violations, "us-socrata-search-environmental-violations", "Socrata Cross-Portal Search — Environmental Violations", "Socrata 横断検索 environmental violations",
  "us_state", "environment",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=environmental%20violations&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"environmental-violations\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'environmental violations': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.",
  "resource.id");

VJSON(us_socrata_search_evictions, "us-socrata-search-evictions", "Socrata Cross-Portal Search — Evictions", "Socrata 横断検索 evictions",
  "us_state", "geospatial",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=evictions&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"evictions\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'evictions': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_hospital_quality, "us-socrata-search-hospital-quality", "Socrata Cross-Portal Search — Hospital Quality", "Socrata 横断検索 hospital quality",
  "us_state", "health",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=hospital%20quality&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"hospital-quality\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'hospital quality': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_inspections, "us-socrata-search-inspections", "Socrata Cross-Portal Search — Restaurant Inspections", "Socrata 横断検索 restaurant inspections",
  "us_state", "health",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=restaurant%20inspections&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"inspections\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'restaurant inspections': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_license_plate_reader, "us-socrata-search-license-plate-reader", "Socrata Cross-Portal Search — License Plate Reader", "Socrata 横断検索 license plate reader",
  "us_state", "security",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=license%20plate%20reader&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"license-plate-reader\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'license plate reader': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_lobbyist_registry, "us-socrata-search-lobbyist-registry", "Socrata Cross-Portal Search — Lobbyist Registration", "Socrata 横断検索 lobbyist registration",
  "us_state", "transparency",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=lobbyist%20registration&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"lobbyist-registry\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'lobbyist registration': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_police_stops, "us-socrata-search-police-stops", "Socrata Cross-Portal Search — Police Stops", "Socrata 横断検索 police stops",
  "us_state", "crime",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=police%20stops&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"police-stops\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'police stops': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_power_outages, "us-socrata-search-power-outages", "Socrata Cross-Portal Search — Power Outages", "Socrata 横断検索 power outages",
  "us_state", "energy",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=power%20outages&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"power-outages\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'power outages': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_procurement_contracts, "us-socrata-search-procurement-contracts", "Socrata Cross-Portal Search — Procurement Contracts", "Socrata 横断検索 procurement contracts",
  "us_state", "procurement",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=procurement%20contracts&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"procurement-contracts\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'procurement contracts': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_property_assessments, "us-socrata-search-property-assessments", "Socrata Cross-Portal Search — Property Assessments", "Socrata 横断検索 property assessments",
  "us_state", "geospatial",
  "https://api.us.socrata.com/api/catalog/v1?limit=2000&q=property%20assessments&order=createdAt",   /* offset paging at limit=100 overlaps (1,261 fetched, 1,259 distinct, twice, with two different pairs); one limit=2000 request returns all 1,261 distinct (2026-09-15) */
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"property-assessments\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'property assessments': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");

VJSON(us_socrata_search_public_salaries, "us-socrata-search-public-salaries", "Socrata Cross-Portal Search — Employee Salaries", "Socrata 横断検索 employee salaries",
  "us_state", "transparency",
  "https://api.us.socrata.com/api/catalog/v1?limit=100&q=employee%20salaries&order=createdAt",
  "results",
  "en", "[\"usa\",\"opendata\",\"socrata\",\"search\",\"public-salaries\"]", 86400,
  "Every Socrata-hosted asset across all US government portals matching 'employee salaries': owning domain, dataset identifier, category, column schema and last update — a cross-jurisdiction sweep in one request.");
