/* collectors/sources/hp_exposure_geo.c — credential exposure + place/actor depth.
 *
 * Two small groups that belong to the same batch.
 *
 * Exposure: the repo checked whether an email appears in a breach. The deeper
 * records are the infostealer logs (which name the victim machine, the installed
 * software and every credential URL captured on it), the paste history, and the
 * per-domain employee exposure view. Hudson Rock publishes the infostealer side
 * free; HIBP's stealer-log endpoints need the usual key.
 *
 * Geo/actor: an OSM contributor's changeset history, a place's extra tags
 * (operator, website, phone), a gazetteer hierarchy and NASA's granule index —
 * all keyed off a name rather than a bounding box. */
#include "../../lib/hpengine.h"

static const hp_source HP_EXPOSURE_GEO[] = {
  /* ── Credential & infostealer exposure ─────────────────────────────────── */
  { .id = "HIBP_STEALER_LOGS", .name = "HIBP — stealer logs by email",
    .name_ja = "HIBP — スティーラーログ(メール)", .category = "cyber",
    .portal = "https://haveibeenpwned.com", .record_type = "stealer-log",
    .tags = "\"breach\",\"infostealer\"", .want = HP_EMAIL,
    .key_env = "HIBP_API_KEY", .free_tier = 0,
    .url = "https://haveibeenpwned.com/api/v3/stealerlogsbyemail/{q}",
    .headers = { "hibp-api-key: {key}", "User-Agent: JapanOSINT", NULL },
    .max_items = 60, .title_keys = "value",
    .description = "Domains where an email address appeared in infostealer malware "
      "logs — i.e. sites this person entered credentials into on an infected machine. "
      "A different and usually more current signal than a breach listing" },

  { .id = "HIBP_PASTE_ACCOUNT", .name = "HIBP — pastes containing an account",
    .name_ja = "HIBP — ペースト出現履歴", .category = "cyber",
    .portal = "https://haveibeenpwned.com", .record_type = "paste-exposure",
    .tags = "\"breach\",\"pastes\"", .want = HP_EMAIL,
    .key_env = "HIBP_API_KEY", .free_tier = 0,
    .url = "https://haveibeenpwned.com/api/v3/pasteaccount/{q}",
    .headers = { "hibp-api-key: {key}", "User-Agent: JapanOSINT", NULL },
    .title_keys = "Title,Source", .id_keys = "Id", .date_keys = "Date",
    .max_items = 40,
    .description = "Public pastes in which an address appears — source site, paste "
      "id, title, date and how many accounts the paste held. Pastes surface leaks "
      "that never get catalogued as named breaches" },

  { .id = "HIBP_BREACH_CATALOG", .name = "HIBP — breach catalogue metadata",
    .name_ja = "HIBP — 侵害カタログ", .category = "cyber",
    .portal = "https://haveibeenpwned.com", .record_type = "breach-catalog-entry",
    .tags = "\"breach\",\"catalog\"", .free_tier = 1,
    .url = "https://haveibeenpwned.com/api/v3/breaches",
    .headers = { "User-Agent: JapanOSINT", NULL },
    .filter_query = 1, .max_items = 20, .title_keys = "Name,Title",
    .id_keys = "Name", .date_keys = "BreachDate",
    .description = "The keyless HIBP breach catalogue filtered to the query — breach "
      "date, added date, record count, the exact data classes exposed and whether the "
      "breach is verified, sensitive or retired" },

  { .id = "HUDSON_ROCK_EMAIL", .name = "Hudson Rock — infostealer exposure by email",
    .name_ja = "Hudson Rock — メール感染照会", .category = "cyber",
    .portal = "https://www.hudsonrock.com", .record_type = "stealer-log",
    .tags = "\"breach\",\"infostealer\"", .want = HP_EMAIL, .free_tier = 1,
    .url = "https://cavalier.hudsonrock.com/api/json/v2/osint-tools/search-by-email?email={q}",
    .title_keys = "message,stealer.stealer_family",
    .description = "Whether an email appears in infostealer logs, with the malware "
      "family, infection date, computer name, operating system, installed antivirus "
      "and the URLs whose credentials were captured. Free, no key" },

  { .id = "HUDSON_ROCK_DOMAIN", .name = "Hudson Rock — infostealer exposure by domain",
    .name_ja = "Hudson Rock — ドメイン感染統計", .category = "cyber",
    .portal = "https://www.hudsonrock.com", .record_type = "stealer-log",
    .tags = "\"breach\",\"infostealer\",\"domain\"", .want = HP_DOMAIN, .free_tier = 1,
    .url = "https://cavalier.hudsonrock.com/api/json/v2/osint-tools/search-by-domain?domain={qh}",
    .title_keys = "message,total",
    .description = "Organisation-level infostealer exposure for a domain — how many "
      "employees and how many users of the domain's services were compromised, and "
      "the third-party applications those credentials unlock" },

  { .id = "HUDSON_ROCK_USERNAME", .name = "Hudson Rock — infostealer exposure by username",
    .name_ja = "Hudson Rock — ユーザー名感染照会", .category = "cyber",
    .portal = "https://www.hudsonrock.com", .record_type = "stealer-log",
    .tags = "\"breach\",\"infostealer\",\"username\"", .free_tier = 1,
    .url = "https://cavalier.hudsonrock.com/api/json/v2/osint-tools/search-by-username?username={qn}",
    .title_keys = "message,stealer.stealer_family",
    .description = "Infostealer records tied to a username rather than an email — "
      "catches the case where the person's mailbox is clean but a login handle they "
      "reuse everywhere was captured on an infected host" },

  { .id = "LEAKCHECK_PUBLIC", .name = "LeakCheck — public breach index",
    .name_ja = "LeakCheck — 公開侵害索引", .category = "cyber",
    .portal = "https://leakcheck.io", .record_type = "breach-exposure",
    .tags = "\"breach\"", .free_tier = 1,
    .url = "https://leakcheck.io/api/public?check={q}",
    .array_path = "sources", .title_keys = "name", .date_keys = "date",
    .max_items = 40,
    .description = "Keyless breach lookup for an email, username or phone — which "
      "named sources contain the identifier and the breach date of each, without "
      "returning passwords" },

  { .id = "PROXYNOVA_COMB", .name = "ProxyNova — combolist search",
    .name_ja = "ProxyNova — コンボリスト検索", .category = "cyber",
    .portal = "https://www.proxynova.com/tools/comb", .record_type = "credential-exposure",
    .tags = "\"breach\",\"combolist\"", .free_tier = 1,
    .url = "https://api.proxynova.com/comb?query={q}&start=0&limit=40",
    .array_path = "lines", .max_items = 40, .title_keys = "0",
    .description = "Search across the aggregated COMB credential compilation for an "
      "email, username or domain fragment — shows whether an identifier circulates in "
      "public combolists, which is the corpus credential-stuffing actually uses" },

  { .id = "GREP_APP_CODE", .name = "grep.app — cross-repository code search",
    .name_ja = "grep.app — 横断コード検索", .category = "cyber",
    .portal = "https://grep.app", .record_type = "code-hit",
    .tags = "\"code\",\"leaks\"", .free_tier = 1,
    .url = "https://grep.app/api/search?q={q}&page_size=40",
    .array_path = "hits.hits", .title_keys = "repo.raw,path.raw",
    .id_keys = "id", .max_items = 40,
    .description = "Literal search across hundreds of thousands of public "
      "repositories — internal hostnames, API endpoints, employee emails and keys "
      "committed by mistake, found by exact string rather than by repo name" },

  /* ── Place & contributor depth ─────────────────────────────────────────── */
  { .id = "OSM_USER_CHANGESETS", .name = "OpenStreetMap — contributor changesets",
    .name_ja = "OSM — 編集者の変更履歴", .category = "geospatial",
    .portal = "https://www.openstreetmap.org", .record_type = "osm-changeset",
    .tags = "\"osm\",\"people\",\"timeline\"", .free_tier = 1,
    .url = "https://api.openstreetmap.org/api/0.6/changesets.json?display_name={qn}&limit=50",
    .array_path = "changesets", .title_keys = "tags.comment,id", .id_keys = "id",
    .date_keys = "created_at", .max_items = 50,
    .description = "Every edit session by an OSM contributor — comment, editor "
      "software, bounding box and timestamp. Edit geography and edit hours localise a "
      "contributor as reliably as any social profile" },

  { .id = "OSM_NOTES_SEARCH", .name = "OpenStreetMap — map notes search",
    .name_ja = "OSM — メモ検索", .category = "geospatial",
    .portal = "https://www.openstreetmap.org", .record_type = "osm-note",
    .tags = "\"osm\",\"crowdsource\"", .free_tier = 1,
    .url = "https://api.openstreetmap.org/api/0.6/notes/search.json?q={q}&limit=50",
    .array_path = "features", .title_keys = "properties.comments.0.text",
    .id_keys = "properties.id", .date_keys = "properties.date_created",
    .lat_key = "geometry.coordinates.1", .lon_key = "geometry.coordinates.0",
    .max_items = 50,
    .description = "Free-text notes left on the map mentioning a term — local "
      "observations about businesses, closures, access restrictions and construction, "
      "each geolocated and attributed to a contributor" },

  { .id = "NOMINATIM_EXTRATAGS", .name = "Nominatim — place record with extra tags",
    .name_ja = "Nominatim — 施設詳細タグ付き", .category = "geospatial",
    .portal = "https://nominatim.openstreetmap.org", .record_type = "place-record",
    .tags = "\"osm\",\"places\"", .free_tier = 1,
    .url = "https://nominatim.openstreetmap.org/search?q={q}&format=jsonv2"
           "&addressdetails=1&extratags=1&namedetails=1&limit=25",
    .headers = { "User-Agent: JapanOSINT (research)", NULL },
    .title_keys = "display_name,name", .id_keys = "osm_id",
    .lat_key = "lat", .lon_key = "lon", .max_items = 25,
    .description = "Place search returning the extra OSM tags most geocoders drop — "
      "operator, brand, website, phone, opening hours and official reference numbers, "
      "which is what links a physical site to a corporate operator" },

  { .id = "GEONAMES_HIERARCHY", .name = "GeoNames — administrative hierarchy",
    .name_ja = "GeoNames — 行政階層", .category = "geospatial",
    .portal = "https://www.geonames.org", .record_type = "gazetteer-place",
    .tags = "\"gazetteer\",\"admin\"", .want = HP_NUMERIC,
    .key_env = "GEONAMES_USER", .free_tier = 1,
    .url = "http://api.geonames.org/hierarchyJSON?geonameId={qd}&username={key}",
    .array_path = "geonames", .title_keys = "name,toponymName", .id_keys = "geonameId",
    .lat_key = "lat", .lon_key = "lng", .max_items = 25,
    .description = "The full administrative chain above a GeoNames place — continent, "
      "country, state, county, municipality — which resolves ambiguous place names "
      "into a single unambiguous jurisdiction" },

  { .id = "NASA_CMR_GRANULES", .name = "NASA CMR — earth observation granules",
    .name_ja = "NASA CMR — 観測データ検索", .category = "satellite",
    .portal = "https://cmr.earthdata.nasa.gov", .record_type = "eo-granule",
    .tags = "\"satellite\",\"imagery\"", .free_tier = 1,
    .url = "https://cmr.earthdata.nasa.gov/search/granules.json?keyword={q}&page_size=40",
    .array_path = "feed.entry", .title_keys = "title,dataset_id", .id_keys = "id",
    .date_keys = "time_start", .max_items = 40,
    .description = "NASA's common metadata repository — individual satellite data "
      "granules matching a place or instrument, with acquisition time, spatial extent "
      "and direct download links across every NASA DAAC" },
};

HP_REGISTER_TABLE(HP_EXPOSURE_GEO)
