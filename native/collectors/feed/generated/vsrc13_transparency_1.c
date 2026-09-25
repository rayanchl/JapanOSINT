/* Verified-live transparency sources (60), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

/* 2026-09-08 EMITS_NOTHING fixes for cyb-oireachtas-constituencies,
 * cyb-oireachtas-houses and cyb-eduskunta-tables. All three endpoints are
 * alive and their declared array paths are correct; the records are the wrong
 * SHAPE for jsonlist, so it fetched real data and emitted zero.
 *
 * Fetched live 2026-09-08:
 *
 *   api.oireachtas.ie/v1/houses?limit=20 ->
 *     {"head":{"counts":{"housesCount":68,...}},
 *      "results":[{"house":{"chamberType":"house","houseNo":"27",
 *                           "houseCode":"dail & seanad","seats":234,
 *                           "dateRange":{...},"showAs":"34th Dáil & 27th Seanad",
 *                           "uri":"https://data.oireachtas.ie/..."}}, ...]}
 *   api.oireachtas.ie/v1/constituencies?limit=50 -> the same, with each result
 *     holding TWO envelopes, "constituencyOrPanel" and "house".
 *
 * Every human-readable label sits one level down, under `showAs`, which is not
 * one of jsonlist's K_TITLE names — so each record has no derivable title at
 * the top level and emit_record drops it (SOURCE_AUTHORING_CONTRACT R1).
 *
 * The fix COPIES the label up rather than unwrapping: a constituency result
 * carries two envelopes whose members would collide on `showAs`/`uri` if
 * hoisted, and hoisting one of them would silently drop the other (which names
 * the Seanad/Dáil term the constituency belongs to). So `name` and `uri` are
 * added at the top level from the first envelope that has them, the envelopes
 * themselves are left untouched, and jsonlist keeps every original field
 * verbatim in properties_json. Nothing is invented — both values are copies of
 * strings the upstream sent — and nothing is dropped.
 *
 *   avoindata.eduskunta.fi/api/v1/tables/ ->
 *     ["Attachment","AttachmentGroup","HetekaData","MemberOfParliament",
 *      "PrimaryKeys","SaliDBAanestys", ...]
 *
 * a bare array of STRINGS. jsonlist needs objects, so this row emitted nothing
 * too. Each string IS the whole record here (the table's name is the only
 * datum this index endpoint publishes), so each is wrapped as {"name": <it>}.
 * That adds no information; it only gives the string the field name jsonlist
 * looks for. */
typedef struct { const char *path, *cat, *tags; } tr_shape_opts;

static int tr_emit_showas(const source_ctx *c, intel_sink *s, const char *id,
                          cJSON *doc, void *ud, int *seen) {
  (void)c;
  tr_shape_opts *o = (tr_shape_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  cJSON *el;
  cJSON_ArrayForEach(el, arr) {
    if (!cJSON_IsObject(el)) continue;
    if (cJSON_GetObjectItemCaseSensitive(el, "name")) continue;
    for (cJSON *sub = el->child; sub; sub = sub->next) {
      if (!cJSON_IsObject(sub)) continue;
      cJSON *sa = cJSON_GetObjectItemCaseSensitive(sub, "showAs");
      if (!sa || !cJSON_IsString(sa) || !sa->valuestring || !sa->valuestring[0])
        continue;
      cJSON_AddStringToObject(el, "name", sa->valuestring);
      cJSON *uri = cJSON_GetObjectItemCaseSensitive(sub, "uri");
      if (uri && cJSON_IsString(uri) && uri->valuestring && uri->valuestring[0]
          && !cJSON_GetObjectItemCaseSensitive(el, "uri"))
        cJSON_AddStringToObject(el, "uri", uri->valuestring);
      break;
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->cat, "en", o->tags, seen);
}

static int tr_emit_strings(const source_ctx *c, intel_sink *s, const char *id,
                           cJSON *doc, void *ud, int *seen) {
  (void)c;
  tr_shape_opts *o = (tr_shape_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  if (arr) {
    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n; i++) {
      cJSON *el = cJSON_GetArrayItem(arr, i);
      if (!el || !cJSON_IsString(el) || !el->valuestring || !el->valuestring[0])
        continue;
      cJSON *obj = cJSON_CreateObject();
      if (!obj) continue;
      cJSON_AddStringToObject(obj, "name", el->valuestring);
      cJSON_ReplaceItemInArray(arr, i, obj);
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->cat, "en", o->tags, seen);
}

VJSON(cyb_annuaire_entreprises_near, "cyb-annuaire-entreprises-near", "France — company search by geography, Paris centre", "仏 企業地理検索 パリ中心部",
  "transparency", "corporate",
  "https://recherche-entreprises.api.gouv.fr/near_point?lat=48.8566&long=2.3522&radius=5&per_page=25",
  "results",
  "fr", "[\"fra\",\"corporate\",\"registry\",\"geo\"]", 86400,
  "Registered French establishments within a radius of a coordinate, giving company presence on the ground.");

VJSON(cyb_brreg_enheter, "cyb-brreg-enheter", "Brønnøysund Register Centre — Norwegian entity register", "ノルウェー企業登記所 法人一覧",
  "transparency", "corporate",
  "https://data.brreg.no/enhetsregisteret/api/enheter?size=100",
  "_embedded.enheter",
  "no", "[\"nor\",\"corporate\",\"registry\",\"transparency\"]", 86400,
  "Norwegian main company register with organisation number, legal form, industry code and registered address.");

VJSON(cyb_camara_br_proposicoes2, "cyb-camara-br-proposicoes2", "Chamber of Deputies of Brazil — legislative propositions", "ブラジル下院 議案",
  "transparency", "politics",
  "https://dadosabertos.camara.leg.br/api/v2/proposicoes?itens=100&formato=json",
  "dados",
  "pt", "[\"bra\",\"parliament\",\"legislation\",\"transparency\"]", 21600,
  "Bills and other propositions before the Brazilian Chamber with type, number and year.");

VJSON(cyb_camara_br_votacoes2, "cyb-camara-br-votacoes2", "Chamber of Deputies of Brazil — votes", "ブラジル下院 採決",
  "transparency", "politics",
  "https://dadosabertos.camara.leg.br/api/v2/votacoes?itens=100&formato=json",
  "dados",
  "pt", "[\"bra\",\"parliament\",\"voting\",\"transparency\"]", 21600,
  "Recorded votes in the Brazilian Chamber with date, body and summary of the outcome.");

VJSON(cyb_contractsfinder_ocds, "cyb-contractsfinder-ocds", "UK Contracts Finder — OCDS notice releases", "英国 Contracts Finder OCDS通知",
  "transparency", "procurement",
  "https://www.contractsfinder.service.gov.uk/Published/Notices/OCDS/Search",
  "releases",
  "en", "[\"gbr\",\"procurement\",\"ocds\",\"contracts\"]", 21600,
  "UK central and local government procurement notices in Open Contracting Data Standard form.");

VJSON(cyb_contractsfinder_ocds_award, "cyb-contractsfinder-ocds-award", "UK Contracts Finder — OCDS award notices", "英国 Contracts Finder 落札通知",
  "transparency", "procurement",
  "https://www.contractsfinder.service.gov.uk/Published/Notices/OCDS/Search?stages=award",
  "releases",
  "en", "[\"gbr\",\"procurement\",\"ocds\",\"contracts\"]", 43200,
  "UK award-stage procurement notices naming winning suppliers and contract values.");

VJSON(cyb_datagouv_fr_marches, "cyb-datagouv-fr-marches", "data.gouv.fr — French public procurement datasets", "data.gouv.fr 公共調達データセット",
  "transparency", "procurement",
  "https://www.data.gouv.fr/api/1/datasets/?q=marches%20publics&page_size=50&sort=created",
  "data",
  "fr", "[\"fra\",\"procurement\",\"opendata\",\"contracts\"]", 86400,
  "French open-data catalogue entries for public procurement, the index to national and local DECP publications.");

VJSON(cyb_eduskunta_aanestys, "cyb-eduskunta-aanestys", "Parliament of Finland — plenary vote records", "フィンランド議会 本会議採決",
  "transparency", "politics",
  "https://avoindata.eduskunta.fi/api/v1/tables/SaliDBAanestys/rows?perPage=50&page=0",
  "rowdata",
  "fi", "[\"fin\",\"parliament\",\"voting\",\"transparency\"]", 21600,
  "Plenary session votes from the Eduskunta open data service.");

VJSON(cyb_eduskunta_istunto, "cyb-eduskunta-istunto", "Parliament of Finland — plenary sessions", "フィンランド議会 本会議",
  "transparency", "politics",
  "https://avoindata.eduskunta.fi/api/v1/tables/SaliDBIstunto/rows?perPage=50&page=0",
  "rowdata",
  "fi", "[\"fin\",\"parliament\",\"politics\",\"transparency\"]", 21600,
  "Plenary sessions with dates, numbering and status.");

VJSON(cyb_eduskunta_mp, "cyb-eduskunta-mp", "Parliament of Finland — members", "フィンランド議会 議員",
  "transparency", "politics",
  "https://avoindata.eduskunta.fi/api/v1/tables/MemberOfParliament/rows?perPage=50&page=0",
  "rowdata",
  "fi", "[\"fin\",\"parliament\",\"politics\",\"transparency\"]", 86400,
  "Members of the Eduskunta with party, constituency and term.");

/* See the note at the top of this file: the response is a bare array of table
   NAME strings, which jsonlist cannot label; each is wrapped as {"name": …}. */
static int run_cyb_eduskunta_tables(const source_ctx *c, intel_sink *s) {
  tr_shape_opts o = { "", "politics",
                      "[\"fin\",\"parliament\",\"opendata\",\"reference\"]" };
  int n = pw_walk(c, s, "cyb-eduskunta-tables",
                  "https://avoindata.eduskunta.fi/api/v1/tables/",
                  pw_fetch_json, tr_emit_strings, &o);
  if (n < 0) { fprintf(stderr, "[cyb-eduskunta-tables] fetch failed\n"); return -1; }
  return 0; }
static const source_def cyb_eduskunta_tables = {
  .id = "cyb-eduskunta-tables", .collector = "transparency",
  .name = "Parliament of Finland — open data table index",
  .name_ja = "フィンランド議会 データ表索引",
  .update_interval_sec = 86400, .run = run_cyb_eduskunta_tables,
  .category = "politics", .type = "api",
  .url = "https://avoindata.eduskunta.fi/api/v1/tables/",
  .description = "Index of every table exposed by the Finnish Parliament open data API.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(cyb_eduskunta_tables);

#include "lib/jocore.h"

/* cyb-europarl-adopted-texts — walked YEAR BY YEAR, one request per year.
 *
 * History. 2026-09-08 declared `offset` so the pagewalk could advance, after
 * verifying that offset=0 twice returned the same 50 ids. That no longer holds:
 * re-measured 2026-09-14, offset=0 fetched twice returns a DIFFERENT SET, with
 * or without sort=id, so no offset walk over this API can be exhaustive — the
 * best walk reached 5,048 distinct of meta.total 5,461, and the sweep measured
 * 1,000 emitted / 706 stored.
 *
 * The API does filter by `year`, and no year comes near its 1,000-record page:
 * 2014 … 2025 answer 115 … 551 texts each, every one a single complete page
 * whose distinct ids equal its own meta.total. Asking per year makes the result
 * independent of the server's ordering. A year with no texts answers an empty
 * body, which is counted as zero, not as a failure; a year that fails is
 * disclosed as a notice, and a year that ever fills the 1,000-record page is
 * disclosed too, because this API offers no stable order to page it further. */
static int run_cyb_europarl_adopted_texts(const source_ctx *c, intel_sink *s) {
  const char *id = "cyb-europarl-adopted-texts";
  const char *tags = "[\"eur\",\"parliament\",\"legislation\",\"transparency\"]";
  const char *hdrs[] = { "Accept: application/ld+json", NULL };
  time_t now = time(NULL);
  struct tm tmv;
  int last = gmtime_r(&now, &tmv) ? tmv.tm_year + 1900 : 2026;
  int n = 0, failed = 0, years_with = 0;
  for (int y = 1999; y <= last; y++) {
    char url[256];
    snprintf(url, sizeof url,
             "https://data.europarl.europa.eu/api/v2/adopted-texts"
             "?format=application%%2Fld%%2Bjson&year=%d&limit=1000&offset=0", y); /* exhaustive-ok: one request per year, every year < 1,000 texts; a year that fills the page is disclosed as a notice */
    http_response r = {0};
    int rc = http_request(c->http, "GET", url, hdrs, NULL, 0, 120000, 2, &r);
    if (rc != 0 || r.status < 200 || r.status >= 300) {
      failed++;
      fprintf(stderr, "[%s] year %d failed (transport rc=%d, HTTP %ld)\n",
              id, y, rc, r.status);
      http_response_free(&r);
      continue;
    }
    if (!r.body || r.body_len == 0) {      /* no texts adopted that year */
      http_response_free(&r);
      continue;
    }
    cJSON *doc = cJSON_Parse(r.body);
    http_response_free(&r);
    if (!doc) {
      failed++;
      fprintf(stderr, "[%s] year %d answered a body that is not JSON\n", id, y);
      continue;
    }
    cJSON *data = cJSON_GetObjectItemCaseSensitive(doc, "data");
    if (!cJSON_IsArray(data)) {
      /* A JSON body with no "data" array is not a year of texts. Emitting it
       * stored the envelope itself as a record under a fallback label (seen
       * 2026-09-15: 1 such row beside 5,455 real texts), so it is a failed
       * year, disclosed below, and nothing from it is stored. */
      failed++;
      fprintf(stderr, "[%s] year %d answered JSON without a data array\n", id, y);
      cJSON_Delete(doc);
      continue;
    }
    int got = cJSON_GetArraySize(data);
    if (got > 0) years_with++;
    if (got >= 1000) {
      char q[32], why[160];
      snprintf(q, sizeof q, "year=%d", y);
      snprintf(why, sizeof why, "year %d filled the API's 1,000-record page", y);
      jo_truncation_notice_ex(s, id, q, got, -1, why,
                              "partition the year further (the API has no stable order to page)",
                              NULL);
    }
    int e = jsonlist_emit_ex(s, id, doc, "data", "politics", "en", tags, NULL);
    if (e > 0) n += e;
    cJSON_Delete(doc);
  }
  if (failed) {
    char why[160];
    snprintf(why, sizeof why, "%d year listing(s) failed to fetch or parse", failed);
    jo_truncation_notice_ex(s, id, NULL, n, -1, why,
                            "re-run; each failed year is in the run log", NULL);
  }
  fprintf(stderr, "[%s] emitted %d text(s) across %d year(s) with data\n",
          id, n, years_with);
  return (n == 0 && failed) ? -1 : 0;
}
static const source_def cyb_europarl_adopted_texts = {
  .id = "cyb-europarl-adopted-texts", .collector = "transparency",
  .name = "European Parliament — adopted texts",
  .name_ja = "欧州議会 採択文書",
  .update_interval_sec = 43200, .run = run_cyb_europarl_adopted_texts,
  .category = "politics", .type = "api",
  .url = "https://data.europarl.europa.eu/api/v2/adopted-texts?format=application%2Fld%2Bjson",
  .description = "Texts adopted in plenary, including resolutions on sanctions and foreign policy.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(cyb_europarl_adopted_texts);

VJSON(cyb_europarl_corporate_bodies, "cyb-europarl-corporate-bodies", "European Parliament — corporate bodies", "欧州議会 組織体一覧",
  "transparency", "politics",
  "https://data.europarl.europa.eu/api/v2/corporate-bodies?format=application%2Fld%2Bjson&limit=50",
  "data",
  "en", "[\"eur\",\"parliament\",\"politics\",\"transparency\"]", 86400,
  "Committees, delegations and political groups of the European Parliament.");

VJSON(cyb_europarl_meetings, "cyb-europarl-meetings", "European Parliament — meetings open data", "欧州議会 会議データ",
  "transparency", "politics",
  "https://data.europarl.europa.eu/api/v2/meetings?format=application%2Fld%2Bjson&limit=50",
  "data",
  "en", "[\"eur\",\"parliament\",\"politics\",\"transparency\"]", 21600,
  "European Parliament plenary and body meetings with dates and identifiers.");

VJSON(cyb_findtender_ocds_tender, "cyb-findtender-ocds-tender", "UK Find a Tender — OCDS tender-stage releases", "英国 Find a Tender 入札段階通知",
  "transparency", "procurement",
  "https://www.find-tender.service.gov.uk/api/1.0/ocdsReleasePackages?stages=tender",
  "releases",
  "en", "[\"gbr\",\"procurement\",\"ocds\",\"contracts\"]", 21600,
  "High-value UK tender opportunities in OCDS form, the post-Brexit replacement for TED coverage of the UK.");

VJSON(cyb_folketing_afstemning, "cyb-folketing-afstemning", "Danish Parliament ODA — votes", "デンマーク議会 採決",
  "transparency", "politics",
  "https://oda.ft.dk/api/Afstemning?%24top=50",
  "value",
  "da", "[\"dnk\",\"parliament\",\"voting\",\"transparency\"]", 43200,
  "Folketing votes with conclusion and vote type.");

VJSON(cyb_folketing_dokument, "cyb-folketing-dokument", "Danish Parliament ODA — documents", "デンマーク議会 文書",
  "transparency", "politics",
  "https://oda.ft.dk/api/Dokument?%24top=50",
  "value",
  "da", "[\"dnk\",\"parliament\",\"legislation\",\"transparency\"]", 28800,
  "Documents tied to Folketing cases with type, date and title.");

VJSON(cyb_folketing_oda_aktor, "cyb-folketing-oda-aktor", "Danish Parliament ODA — actors", "デンマーク議会 関係者",
  "transparency", "politics",
  "https://oda.ft.dk/api/Akt%C3%B8r?%24top=50",
  "value",
  "da", "[\"dnk\",\"parliament\",\"politics\",\"transparency\"]", 86400,
  "Actors in the Folketing system: members, ministries, committees and external parties.");

VJSON(cyb_folketing_oda_sag, "cyb-folketing-oda-sag", "Danish Parliament ODA — cases", "デンマーク議会 案件",
  "transparency", "politics",
  "https://oda.ft.dk/api/Sag?%24top=50",
  "value",
  "da", "[\"dnk\",\"parliament\",\"legislation\",\"transparency\"]", 21600,
  "Folketing cases including bills, motions and inquiries with status and type.");

VJSON(cyb_folketing_sagstrin, "cyb-folketing-sagstrin", "Danish Parliament ODA — case stages", "デンマーク議会 審議段階",
  "transparency", "politics",
  "https://oda.ft.dk/api/Sagstrin?%24top=50",
  "value",
  "da", "[\"dnk\",\"parliament\",\"legislation\",\"transparency\"]", 43200,
  "Stage-by-stage progress of Folketing cases through readings and committee.");

VJSON(cyb_folketing_stemme, "cyb-folketing-stemme", "Danish Parliament ODA — individual votes", "デンマーク議会 個別投票",
  "transparency", "politics",
  "https://oda.ft.dk/api/Stemme?%24top=50",
  "value",
  "da", "[\"dnk\",\"parliament\",\"voting\",\"transparency\"]", 57600,
  "Individual member votes attached to each Folketing division.");

/* See the note at the top of this file: each result wraps its label in a
   "constituencyOrPanel"/"house" envelope under `showAs`. */
static int run_cyb_oireachtas_constituencies(const source_ctx *c, intel_sink *s) {
  tr_shape_opts o = { "results", "politics",
                      "[\"irl\",\"parliament\",\"politics\",\"geo\"]" };
  int n = pw_walk(c, s, "cyb-oireachtas-constituencies",
                  "https://api.oireachtas.ie/v1/constituencies?limit=50",
                  pw_fetch_json, tr_emit_showas, &o);
  if (n < 0) { fprintf(stderr, "[cyb-oireachtas-constituencies] fetch failed\n"); return -1; }
  return 0; }
static const source_def cyb_oireachtas_constituencies = {
  .id = "cyb-oireachtas-constituencies", .collector = "transparency",
  .name = "Houses of the Oireachtas — constituencies and panels",
  .name_ja = "アイルランド議会 選挙区",
  .update_interval_sec = 86400, .run = run_cyb_oireachtas_constituencies,
  .category = "politics", .type = "api",
  .url = "https://api.oireachtas.ie/v1/constituencies?limit=50",
  .description = "Irish constituencies and Seanad panels used in member records.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(cyb_oireachtas_constituencies);

VJSON(cyb_oireachtas_divisions, "cyb-oireachtas-divisions", "Houses of the Oireachtas — division results", "アイルランド議会 採決結果",
  "transparency", "politics",
  "https://api.oireachtas.ie/v1/divisions?limit=50",
  "results",
  "en", "[\"irl\",\"parliament\",\"voting\",\"transparency\"]", 21600,
  "Recorded votes in the Dáil and Seanad with per-member tallies.");

/* See the note at the top of this file: each result wraps its label in a
   "house" envelope under `showAs`. */
static int run_cyb_oireachtas_houses(const source_ctx *c, intel_sink *s) {
  tr_shape_opts o = { "results", "politics",
                      "[\"irl\",\"parliament\",\"politics\",\"reference\"]" };
  int n = pw_walk(c, s, "cyb-oireachtas-houses",
                  "https://api.oireachtas.ie/v1/houses?limit=20",
                  pw_fetch_json, tr_emit_showas, &o);
  if (n < 0) { fprintf(stderr, "[cyb-oireachtas-houses] fetch failed\n"); return -1; }
  return 0; }
static const source_def cyb_oireachtas_houses = {
  .id = "cyb-oireachtas-houses", .collector = "transparency",
  .name = "Houses of the Oireachtas — house terms",
  .name_ja = "アイルランド議会 会期",
  .update_interval_sec = 86400, .run = run_cyb_oireachtas_houses,
  .category = "politics", .type = "api",
  .url = "https://api.oireachtas.ie/v1/houses?limit=20",
  .description = "Dáil and Seanad terms with start and end dates, needed to scope other Oireachtas queries.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(cyb_oireachtas_houses);

VJSON(cyb_oireachtas_questions, "cyb-oireachtas-questions", "Houses of the Oireachtas — parliamentary questions", "アイルランド議会 質問",
  "transparency", "politics",
  "https://api.oireachtas.ie/v1/questions?limit=50",
  "results",
  "en", "[\"irl\",\"parliament\",\"questions\",\"transparency\"]", 21600,
  "Parliamentary questions and answers by member and department.");

VJSON(cyb_prh_fi_companies, "cyb-prh-fi-companies", "Finnish Patent and Registration Office — company register", "フィンランド特許登記庁 企業登記",
  "transparency", "corporate",
  "https://avoindata.prh.fi/opendata-ytj-api/v3/companies?totalResults=false&maxResults=100&resultsFrom=0",
  "companies",
  "fi", "[\"fin\",\"corporate\",\"registry\",\"transparency\"]", 86400,
  "Finnish business register records with business ID, company form, registration date and addresses.");

VJSON(cyb_prh_fi_companies_p2, "cyb-prh-fi-companies-p2", "Finnish Patent and Registration Office — company register page 2", "フィンランド特許登記庁 企業登記 2ページ目",
  "transparency", "corporate",
  "https://avoindata.prh.fi/opendata-ytj-api/v3/companies?totalResults=false&maxResults=100&resultsFrom=100",
  "companies",
  "fi", "[\"fin\",\"corporate\",\"registry\",\"transparency\"]", 86400,
  "Second page of the Finnish business register feed.");

VJSON(cyb_recherche_entreprises_fr, "cyb-recherche-entreprises-fr", "France — company search API, banking sector slice", "仏 企業検索API 銀行分野",
  "transparency", "corporate",
  "https://recherche-entreprises.api.gouv.fr/search?q=banque&per_page=25",
  "results",
  "fr", "[\"fra\",\"corporate\",\"registry\",\"transparency\"]", 86400,
  "French national company search returning SIREN, legal form, activity code and officers for banking-sector matches.");

VJSON(cyb_recherche_entreprises_fr_assoc, "cyb-recherche-entreprises-fr-assoc", "France — company search API, associations slice", "仏 企業検索API 団体分野",
  "transparency", "corporate",
  "https://recherche-entreprises.api.gouv.fr/search?q=association&per_page=25",
  "results",
  "fr", "[\"fra\",\"corporate\",\"registry\",\"transparency\"]", 86400,
  "French entity search slice covering associations, which sit outside the commercial register.");

VRSS(cyb_rechtspraak_nl_atom, "cyb-rechtspraak-nl-atom", "Rechtspraak.nl — Dutch court judgments feed", "オランダ司法 判決フィード",
  "transparency", "legal",
  "https://data.rechtspraak.nl/uitspraken/zoeken?max=100&sort=DESC",
  "nl", "[\"nld\",\"court\",\"judgments\",\"legal\"]", 10800,
  "Most recently published Dutch court decisions with ECLI, court and date.");

VRSS(cyb_rechtspraak_nl_recent, "cyb-rechtspraak-nl-recent", "Rechtspraak.nl — Dutch judgments with document metadata", "オランダ司法 判決 (文書付)",
  "transparency", "legal",
  "https://data.rechtspraak.nl/uitspraken/zoeken?max=50&sort=DESC&return=DOC",
  "nl", "[\"nld\",\"court\",\"judgments\",\"legal\"]", 21600,
  "Dutch court decisions restricted to entries with full document text available.");

VJSON(cyb_riksdagen_dokumentlista2, "cyb-riksdagen-dokumentlista2", "Riksdag of Sweden — private members' motions", "スウェーデン議会 動議一覧",
  "transparency", "politics",
  "https://data.riksdagen.se/dokumentlista/?doktyp=mot&utformat=json&sz=50",
  "dokumentlista.dokument",
  "sv", "[\"swe\",\"parliament\",\"legislation\",\"transparency\"]", 21600,
  "Swedish parliamentary motions with submitting member, party and subject.");

VJSON(cyb_rpo_sk_search, "cyb-rpo-sk-search", "Statistical Office of Slovakia — register of legal entities search", "スロバキア統計局 法人登記検索",
  "transparency", "corporate",
  "https://api.statistics.sk/rpo/v1/search?fullName=banka&limit=50",
  "results",
  "sk", "[\"svk\",\"corporate\",\"registry\",\"transparency\"]", 86400,
  "Slovak register of legal persons, entrepreneurs and public bodies, searchable by name with identifiers and addresses.");

VJSON(cyb_rpvs_sk_plain, "cyb-rpvs-sk-plain", "Slovakia — register of public sector partners", "スロバキア 公共部門パートナー登記",
  "transparency", "corporate",
  "https://rpvs.gov.sk/opendatav2/Partneri",
  "value",
  "sk", "[\"svk\",\"beneficial-ownership\",\"corporate\",\"transparency\"]", 86400,
  "Slovak beneficial-ownership register for companies contracting with the state, the strongest open BO dataset in the EU.");

VJSON(cyb_scotparl_committees, "cyb-scotparl-committees", "Scottish Parliament — committees", "スコットランド議会 委員会",
  "transparency", "politics",
  "https://data.parliament.scot/api/committees",
  "",
  "en", "[\"gbr\",\"scotland\",\"parliament\",\"committee\"]", 86400,
  "Current and historical Scottish Parliament committees with sessions.");

VJSON(cyb_scotparl_constituencies, "cyb-scotparl-constituencies", "Scottish Parliament — constituencies", "スコットランド議会 選挙区",
  "transparency", "politics",
  "https://data.parliament.scot/api/constituencies",
  "",
  "en", "[\"gbr\",\"scotland\",\"parliament\",\"geo\"]", 86400,
  "Scottish Parliament constituencies across sessions.");

VJSON(cyb_scotparl_memberparties, "cyb-scotparl-memberparties", "Scottish Parliament — member party affiliations", "スコットランド議会 議員会派履歴",
  "transparency", "politics",
  "https://data.parliament.scot/api/memberparties",
  "",
  "en", "[\"gbr\",\"scotland\",\"parliament\",\"politics\"]", 86400,
  "Party affiliation history per member, capturing defections and group changes.");

VJSON(cyb_scotparl_parties, "cyb-scotparl-parties", "Scottish Parliament — parties", "スコットランド議会 政党",
  "transparency", "politics",
  "https://data.parliament.scot/api/parties",
  "",
  "en", "[\"gbr\",\"scotland\",\"parliament\",\"politics\"]", 86400,
  "Political parties recorded in the Scottish Parliament data service.");

VJSON(cyb_scotparl_regions, "cyb-scotparl-regions", "Scottish Parliament — regions", "スコットランド議会 地域区",
  "transparency", "politics",
  "https://data.parliament.scot/api/regions",
  "",
  "en", "[\"gbr\",\"scotland\",\"parliament\",\"geo\"]", 86400,
  "Scottish Parliament electoral regions used for list seats.");

VJSON(cyb_sejm_pl_interpellations, "cyb-sejm-pl-interpellations", "Sejm of Poland — interpellations", "ポーランド下院 質問書",
  "transparency", "politics",
  "https://api.sejm.gov.pl/sejm/term10/interpellations",
  "",
  "pl", "[\"pol\",\"parliament\",\"questions\",\"transparency\"]", 21600,
  "Written interpellations to Polish ministers with sender, recipient and reply status.");

VJSON(cyb_sejm_pl_prints, "cyb-sejm-pl-prints", "Sejm of Poland — parliamentary prints", "ポーランド下院 議案文書",
  "transparency", "politics",
  "https://api.sejm.gov.pl/sejm/term10/prints",
  "",
  "pl", "[\"pol\",\"parliament\",\"legislation\",\"transparency\"]", 43200,
  "Sejm prints (bills and formal submissions) with title, delivery date and attachments.");

VJSON(cyb_sejm_pl_processes, "cyb-sejm-pl-processes", "Sejm of Poland — legislative processes", "ポーランド下院 立法過程",
  "transparency", "politics",
  "https://api.sejm.gov.pl/sejm/term10/processes",
  "",
  "pl", "[\"pol\",\"parliament\",\"legislation\",\"transparency\"]", 21600,
  "Legislative processes in the current Sejm term with stages and document links.");

VJSON(cyb_tweedekamer_activiteit, "cyb-tweedekamer-activiteit", "Dutch House of Representatives — activities", "オランダ下院 議事活動",
  "transparency", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Activiteit?%24top=50",
  "value",
  "nl", "[\"nld\",\"parliament\",\"politics\",\"transparency\"]", 21600,
  "Scheduled and held parliamentary activities including debates and committee sessions.");

VJSON(cyb_tweedekamer_besluit, "cyb-tweedekamer-besluit", "Dutch House of Representatives — decisions", "オランダ下院 決定",
  "transparency", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Besluit?%24top=50",
  "value",
  "nl", "[\"nld\",\"parliament\",\"voting\",\"transparency\"]", 43200,
  "Decisions taken on parliamentary cases, with outcome and decision text.");

VJSON(cyb_tweedekamer_document, "cyb-tweedekamer-document", "Dutch House of Representatives — documents", "オランダ下院 文書",
  "transparency", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Document?%24top=50",
  "value",
  "nl", "[\"nld\",\"parliament\",\"legislation\",\"transparency\"]", 21600,
  "Documents submitted to the Tweede Kamer with type, date and case linkage.");

VJSON(cyb_tweedekamer_fractie, "cyb-tweedekamer-fractie", "Dutch House of Representatives — parliamentary groups", "オランダ下院 会派",
  "transparency", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Fractie?%24top=50",
  "value",
  "nl", "[\"nld\",\"parliament\",\"politics\",\"transparency\"]", 86400,
  "Parliamentary groups with seat counts and active periods.");

VJSON(cyb_tweedekamer_stemming, "cyb-tweedekamer-stemming", "Dutch House of Representatives — votes", "オランダ下院 採決",
  "transparency", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Stemming?%24top=50",
  "value",
  "nl", "[\"nld\",\"parliament\",\"voting\",\"transparency\"]", 43200,
  "Individual and party votes recorded on Tweede Kamer decisions.");

VJSON(cyb_tweedekamer_zaak, "cyb-tweedekamer-zaak", "Dutch House of Representatives — cases (Zaak)", "オランダ下院 案件",
  "transparency", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Zaak?%24top=50",
  "value",
  "nl", "[\"nld\",\"parliament\",\"legislation\",\"transparency\"]", 21600,
  "Parliamentary cases in the Tweede Kamer data warehouse, the spine linking documents, decisions and votes.");

VJSON(cyb_ukparl_committee_publications, "cyb-ukparl-committee-publications", "UK Parliament — committee publications", "英国議会 委員会刊行物",
  "transparency", "politics",
  "https://committees-api.parliament.uk/api/Publications?take=50",
  "items",
  "en", "[\"gbr\",\"parliament\",\"committee\",\"transparency\"]", 21600,
  "Committee reports, written and oral evidence as published.");

VJSON(cyb_ukparl_committees, "cyb-ukparl-committees", "UK Parliament — select committees", "英国議会 特別委員会",
  "transparency", "politics",
  "https://committees-api.parliament.uk/api/Committees?take=50",
  "items",
  "en", "[\"gbr\",\"parliament\",\"committee\",\"transparency\"]", 86400,
  "Select committees of both Houses with remit, membership and current status.");

VJSON(cyb_ukparl_commons_divisions, "cyb-ukparl-commons-divisions", "UK Parliament — Commons division results", "英国議会 下院採決結果",
  "transparency", "politics",
  "https://commonsvotes-api.parliament.uk/data/divisions.json/search?queryParameters.take=50",
  "",
  "en", "[\"gbr\",\"parliament\",\"voting\",\"transparency\"]", 21600,
  "Recorded House of Commons votes with aye and no counts per division.");

VJSON(cyb_ukparl_constituencies, "cyb-ukparl-constituencies", "UK Parliament — constituency search", "英国議会 選挙区検索",
  "transparency", "politics",
  "https://members-api.parliament.uk/api/Location/Constituency/Search?take=50",
  "items",
  "en", "[\"gbr\",\"parliament\",\"politics\",\"geo\"]", 86400,
  "Westminster constituencies with current sitting member and boundary period.");

VJSON(cyb_ukparl_edm_list, "cyb-ukparl-edm-list", "UK Parliament — early day motions", "英国議会 早期動議",
  "transparency", "politics",
  "https://oralquestionsandmotions-api.parliament.uk/EarlyDayMotions/list?parameters.take=50",
  "response",
  "en", "[\"gbr\",\"parliament\",\"motions\",\"transparency\"]", 21600,
  "Early day motions with sponsors and signature counts, a read on backbench political attention.");

VJSON(cyb_ukparl_interests, "cyb-ukparl-interests", "UK Parliament — registered financial interests", "英国議会 議員利害関係登録",
  "transparency", "politics",
  "https://interests-api.parliament.uk/api/v1/Interests/?take=50",
  "items",
  "en", "[\"gbr\",\"parliament\",\"lobbying\",\"transparency\"]", 43200,
  "Members' registered financial interests, gifts, donations and outside employment.");

VJSON(cyb_ukparl_lords_divisions, "cyb-ukparl-lords-divisions", "UK Parliament — Lords division results", "英国議会 上院採決結果",
  "transparency", "politics",
  "https://lordsvotes-api.parliament.uk/data/Divisions/search?take=50",
  "",
  "en", "[\"gbr\",\"parliament\",\"voting\",\"transparency\"]", 21600,
  "Recorded House of Lords votes with content and not-content tallies.");

VJSON(cyb_ukparl_members_search, "cyb-ukparl-members-search", "UK Parliament — members search API", "英国議会 議員検索API",
  "transparency", "politics",
  "https://members-api.parliament.uk/api/Members/Search?take=20",
  "items",
  "en", "[\"gbr\",\"parliament\",\"politics\",\"transparency\"]", 86400,
  "Current and former members of both Houses with party, constituency and service dates.");

VJSON(cyb_ukparl_parties_commons, "cyb-ukparl-parties-commons", "UK Parliament — active parties in the Commons", "英国議会 下院現行会派",
  "transparency", "politics",
  "https://members-api.parliament.uk/api/Parties/GetActive/Commons",
  "items",
  "en", "[\"gbr\",\"parliament\",\"politics\",\"transparency\"]", 86400,
  "Parties currently represented in the House of Commons with seat counts.");

VJSON(cyb_ukparl_statutory_instruments, "cyb-ukparl-statutory-instruments", "UK Parliament — statutory instruments", "英国議会 委任立法",
  "transparency", "politics",
  "https://statutoryinstruments-api.parliament.uk/api/v2/StatutoryInstrument?take=50",
  "items",
  "en", "[\"gbr\",\"parliament\",\"legislation\",\"transparency\"]", 43200,
  "Statutory instruments before Parliament, the delegated-legislation pipeline including sanctions regulations.");

VJSON(cyb_ukparl_treaties, "cyb-ukparl-treaties", "UK Parliament — treaties laid before Parliament", "英国議会 提出条約",
  "transparency", "politics",
  "https://treaties-api.parliament.uk/api/Treaty?take=50",
  "items",
  "en", "[\"gbr\",\"parliament\",\"treaty\",\"transparency\"]", 86400,
  "Treaties laid before the UK Parliament with scrutiny status and department.");

VJSON(cyb_usaspending_glossary, "cyb-usaspending-glossary", "USAspending — federal spending glossary", "USAspending 連邦支出用語集",
  "transparency", "procurement",
  "https://api.usaspending.gov/api/v2/references/glossary/?limit=50",
  "results",
  "en", "[\"usa\",\"procurement\",\"spending\",\"reference\"]", 86400,
  "Definitions of US federal award and spending terms, needed to interpret contract and grant records.");
