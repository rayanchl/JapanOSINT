/* Verified-live eu_research sources (13), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

/* eu-cordis-programmes-search: CORDIS publishes one record per programme node
 * PER LANGUAGE, and every language edition repeats the same "id". Live-verified
 * 2026-09-07 on a 50-record page: 25 distinct "id", 50 distinct id+language —
 * the 48% collapse the registry sweep measured. The language editions carry
 * different titles and teasers, so merging them destroys real records. Same
 * hand-rolled paged-emit-with-relabelled-id pattern as vsrc_environment_3.c
 * (geo-tidesandcurrents-currents), because jsonlist_emit_paged_keyed reads a
 * single field and this identity is a pair. */
typedef struct { const char *path, *record_type, *lang, *tags_json; } cordis_lang_opts;

static int cordis_emit_page_lang(const source_ctx *c, intel_sink *s,
                                 const char *id, cJSON *doc, void *ud,
                                 int *seen) {
  (void)c;
  cordis_lang_opts *o = (cordis_lang_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  if (arr) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (!cJSON_IsObject(rec)) continue;
      cJSON *rid = cJSON_GetObjectItemCaseSensitive(rec, "id");
      cJSON *lg  = cJSON_GetObjectItemCaseSensitive(rec, "language");
      if (!cJSON_IsString(rid) || !rid->valuestring) continue;
      if (!cJSON_IsString(lg) || !lg->valuestring) continue;
      char buf[256];
      snprintf(buf, sizeof buf, "%s_%s", rid->valuestring, lg->valuestring);
      cJSON_DeleteItemFromObjectCaseSensitive(rec, "id");
      cJSON_AddStringToObject(rec, "id", buf);
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->record_type, o->lang,
                          o->tags_json, seen);
}

static int run_eu_cordis_programmes_search(const source_ctx *c, intel_sink *s) {
  cordis_lang_opts o = { "payload.results", "research", "en",
    "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\"]" };
  int n = pw_walk(c, s, "eu-cordis-programmes-search",
                  "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27programme%27&p=1&num=10&format=json&srt=id:increasing",   /* exhaustive-ok: pw_walk advances p= (PW_PAGE_PARAMS, lib/pagewalk.c:199) and discloses the remainder */
                  pw_fetch_json, cordis_emit_page_lang, &o);
  if (n < 0) {
    fprintf(stderr, "[eu-cordis-programmes-search] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def eu_cordis_programmes_search = {
  .id = "eu-cordis-programmes-search", .collector = "eu_research",
  .name = "CORDIS (EU) — funding programme hierarchy",
  .name_ja = "CORDIS (EU) — funding programme hierarchy",
  .update_interval_sec = 86400, .run = run_eu_cordis_programmes_search,
  .category = "research", .type = "api",
  .url = "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27programme%27&p=1&num=10&format=json&srt=id:increasing",   /* exhaustive-ok: documentation copy of the walked URL above; run() pages it */
  .description = "8,678 EU funding programme/sub-programme nodes (FP7, H2020, Horizon Europe, Euratom branches) with codes and rcns — the budget-line tree that every CORDIS project attaches to.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eu_cordis_programmes_search);

VJSON(cordis_search_projects_query, "cordis-search-projects-query", "CORDIS project search with keyword clause", "CORDIS project search with keyword clause",
  "eu_research", "research",
  "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27project%27%20AND%20%27hydrogen%27&format=json&p=1&num=10&srt=id:increasing",   /* default relevance order is unstable under p= paging: 31 pages gave 226 distinct of 310; srt=id:increasing gave 310 of 310 (2026-09-15) */
  "payload.results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "1,488 EU-funded projects matching a term. Each result: reference/grant number, id, acronym, the programme array with code/id/rcn/title (H2020 pillar and topic), dates and teaser. The reference number is the key into cordis.europa.eu/project/id/{ref}.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(cordis_search_results_query, "cordis-search-results-query", "CORDIS project RESULTS (outcomes) search", "CORDIS project RESULTS (outcomes) search",
  "eu_research", "research",
  "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27result%27%20AND%20%27hydrogen%27&format=json&p=1&num=5",
  "payload.results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "7,691 reported project outcomes, each carrying relatedProjectAcronym, relatedProjectReference and relatedProjectRcn plus the outcome teaser - the deliverables-behind-a-grant hop, and it links back to the parent project id.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(eu_cordis_projects_search, "eu-cordis-projects-search", "CORDIS (EU) — research project search", "CORDIS (EU) — research project search",
  "eu_research", "research",
  "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27project%27&p=1&num=5&format=json",
  "payload.results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "58,915 EU-funded research projects. Rows give grant agreement reference/id, acronym, title, framework programme with code and rcn, start/end dates, coordinating country, objective teaser and last-update date. The tree had the cordis host but not this search API. Supports arbitrary field queries (e.g. AND /project/rcn=193159) so it doubles as a lookup.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(eu_cordis_results_search, "eu-cordis-results-search", "CORDIS (EU) — research result/deliverable search", "CORDIS (EU) — research result/deliverable search",
  "eu_research", "research",
  "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27result%27&p=1&num=3&format=json",
  "payload.results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\"]", 86400,
  "884,143 research outputs (deliverables, publications, reports) produced under EU grants, each linkable back to the funding project. Same query grammar and pagination (searchAfter cursor) as the project search.");

VJSON(global_ooni_test_names, "global-ooni-test-names", "OONI test-name registry", "OONI test-name registry",
  "eu_research", "research",
  "https://api.ooni.io/api/_/test_names",
  "test_names",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Canonical id/name pairs for every OONI test (bridge_reachability, dnscheck, psiphon, riseupvpn, whatsapp, ...). Needed to enumerate the measurements API exhaustively per test.");

VJSON(openaire_datasources_search, "openaire-datasources-search", "OpenAIRE data sources search", "OpenAIRE data sources search",
  "eu_research", "research",
  "https://api.openaire.eu/graph/v1/dataSources?search=Zenodo&pageSize=5",
  "results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Repositories and aggregators OpenAIRE harvests: official/English name, type scheme, OpenAIRE compatibility level, website, logo, original ids. Provenance registry - tells you which repository a record actually came from.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(openaire_organizations_search, "openaire-organizations-search", "OpenAIRE organizations search", "OpenAIRE organizations search",
  "eu_research", "research",
  "https://api.openaire.eu/graph/v1/organizations?search=Kyoto&pageSize=5",
  "results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "118 org records for a name query: legal name, legal short name, website, country, OpenAIRE org id, pids and originalIds. The id used to attribute research products and projects to an institution.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(openaire_products_by_project, "openaire-products-by-project", "OpenAIRE research products BY project id", "OpenAIRE research products BY project id",
  "eu_research", "research",
  "https://api.openaire.eu/graph/v1/researchProducts?relProjectId=corda__h2020::6c228bea4ee8a4ef22cd1d4fa079b63a&pageSize=5",
  "results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "25 outputs produced under one funded project, full product records with authors and ORCIDs. The grant->deliverables hop, and the reason the project id above matters. Verified with a real project id pulled from the projects search.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(openaire_project_by_id, "openaire-project-by-id", "OpenAIRE project by id (detail)", "OpenAIRE project by id (detail)",
  "eu_research", "research",
  "https://api.openaire.eu/graph/v1/projects?id=corda__h2020::6c228bea4ee8a4ef22cd1d4fa079b63a",
  "results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "One project resolved from its OpenAIRE id: grant code, acronym, title, dates, call id, open-access mandates, subjects and funding stream. The stable id that the research-products-by-project hop consumes.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(openaire_projects_search, "openaire-projects-search", "OpenAIRE Graph funded projects search", "OpenAIRE Graph funded projects search",
  "eu_research", "research",
  "https://api.openaire.eu/graph/v1/projects?search=hydrogen&pageSize=10",
  "results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "5,202 funded projects: OpenAIRE project id, national/EU grant code, acronym, title, start/end dates, call identifier, keywords, and the fundings array (funder short name, jurisdiction, funding stream). Covers national funders (Czech TA, DFG, ANR...) as well as EU.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(openaire_research_product_by_pid, "openaire-research-product-by-pid", "OpenAIRE research product by DOI/PID (detail)", "OpenAIRE research product by DOI/PID (detail)",
  "eu_research", "research",
  "https://api.openaire.eu/graph/v1/researchProducts?pid=10.7717/peerj.4375",
  "results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Resolves any persistent identifier to OpenAIRE's deduplicated record: full author list each with ORCID and its provenance (orcid vs orcid_pending), instances across every repository holding it, licence, and links to funding projects.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(openaire_research_products, "openaire-research-products", "OpenAIRE Graph research products search", "OpenAIRE Graph research products search",
  "eu_research", "research",
  "https://api.openaire.eu/graph/v1/researchProducts?search=climate&pageSize=10",
  "results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "1.93M hits; each result is a merged research product with authors and their ORCIDs, type, language, descriptions, subjects with provenance, pids, publication venue, and the OpenAIRE id used for project/organisation joins.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");
