/* Verified-live de_research sources (9), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(datacite_dois_by_client, "datacite-dois-by-client", "DataCite DOIs deposited by one repository", "DataCite DOIs deposited by one repository",
  "de_research", "research",
  "https://api.datacite.org/dois?client-id=gesis.gesis&page[size]=5",
  "data",
  "en", "[\"de\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Everything a named repository has minted, full DataCite records with creators, affiliations and ORCIDs. Repository->holdings hop; combined with provider->clients it enumerates a whole national consortium's data output.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(datacite_dois_by_orcid, "datacite-dois-by-orcid", "DataCite DOIs by creator ORCID", "DataCite DOIs by creator ORCID",
  "de_research", "research",
  "https://api.datacite.org/dois?query=creators.nameIdentifiers.nameIdentifier:*0000-0001-6187-6610*&page[size]=5",
  "data",
  "en", "[\"de\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Datasets and software authored by a specific ORCID, full records with affiliation. Researcher->data-output hop that Crossref cannot answer.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(datacite_dois_by_provider, "datacite-dois-by-provider", "DataCite DOIs under one provider organisation", "DataCite DOIs under one provider organisation",
  "de_research", "research",
  "https://api.datacite.org/dois?provider-id=acjp&page[size]=5",
  "data",
  "en", "[\"de\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "All DOIs across every repository belonging to one provider org, full records including contributors and affiliations. Institution-level research-output sweep.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(datacite_dois_related_identifier, "datacite-dois-related-identifier", "DataCite DOIs related to a given DOI", "DataCite DOIs related to a given DOI",
  "de_research", "research",
  "https://api.datacite.org/dois?query=relatedIdentifiers.relatedIdentifier:10.7717/peerj.4375&page[size]=5",
  "data",
  "en", "[\"de\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Finds datasets, software and theses that declare a relationship to a target DOI (IsSupplementTo, Cites, IsDerivedFrom ...). The data-behind-the-paper hop, reversed.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(datacite_dois_title_query, "datacite-dois-title-query", "DataCite DOI search with affiliation+publisher expansion", "DataCite DOI search with affiliation+publisher expansion",
  "de_research", "research",
  "https://api.datacite.org/dois?query=titles.title:graphene&page[size]=5&affiliation=true&publisher=true",
  "data",
  "en", "[\"de\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Fielded Elasticsearch query over DataCite with affiliation=true and publisher=true, which expands the creator affiliation objects and publisher entity inline instead of returning bare strings.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

/* datacite-prefixes / datacite-repositories: these two rows declared the path
 * "meta.years", which is DataCite's per-year FACET (10 {id,title,count}
 * buckets), not the registry — so each run stored ten year-counts and none of
 * the 5,755 prefixes or 4,496 repositories, which sit under "data". Measured
 * 2026-09-15. page[size] goes 5 -> 1000 (DataCite's maximum, accepted on both
 * endpoints) so the walk reaches the end in 6 and 5 pages instead of stopping
 * at the page ceiling after 100 records. */
VJSON(datacite_prefixes, "datacite-prefixes", "DataCite DOI prefix registry", "DataCite DOI prefix registry",
  "de_research", "research",
  "https://api.datacite.org/prefixes?page[size]=1000",
  "data",
  "en", "[\"de\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Every DataCite DOI prefix with creation date and relationships to its clients and providers - attribution of an arbitrary DOI prefix to the organisation behind it.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(datacite_repositories, "datacite-repositories", "DataCite repositories directory", "DataCite repositories directory",
  "de_research", "research",
  "https://api.datacite.org/repositories?page[size]=1000",
  "data",
  "en", "[\"de\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Global directory of data repositories with symbol, re3data/OpenDOAR ids, client type, languages, certificates, domains, ISSNs, url and repository software. Registry of who is holding research data worldwide.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(wikidata_backlinks, "wikidata-backlinks", "Wikidata backlinks (what references this item)", "Wikidata backlinks (what references this item)",
  "de_research", "research",
  "https://www.wikidata.org/w/api.php?action=query&list=backlinks&bltitle=Q336264&format=json&bllimit=10",
  "query.backlinks",
  "en", "[\"de\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Every Wikidata item whose statements point at a target item, with continuation token. Reverse-edge traversal - e.g. all people whose employer/alma mater is a given university - without writing SPARQL.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

/* wikidata-wbgetclaims: wbgetclaims answers {claims:{P17:[stmt…], P373:[…], …}}
 * — an OBJECT keyed by property id, one statement array per property. The row
 * declared "claims.p1352", which (path lookup being case-insensitive) selected
 * the P1352 array alone: 37 statements kept of 205 across 111 properties on
 * Q336264 (measured 2026-09-15), every other property discarded. The hook
 * lists every statement of every property into one "statements" array; each
 * statement object is copied whole and keeps its own GUID "id". */
#include "_vjson_idkeys.inc"
static void wd_claims_flatten(cJSON *doc) {
  cJSON *claims = cJSON_GetObjectItemCaseSensitive(doc, "claims");
  if (!cJSON_IsObject(claims)) return;
  cJSON *flat = cJSON_CreateArray();
  if (!flat) return;
  cJSON *prop;
  cJSON_ArrayForEach(prop, claims) {
    if (!cJSON_IsArray(prop)) continue;
    cJSON *st;
    cJSON_ArrayForEach(st, prop)
      if (cJSON_IsObject(st)) cJSON_AddItemToArray(flat, cJSON_Duplicate(st, 1));
  }
  cJSON_AddItemToObject(doc, "statements", flat);
}
VJSON_PREP(wikidata_wbgetclaims, "wikidata-wbgetclaims", "Wikidata claims only (statement-level)", "Wikidata claims only (statement-level)",
  "de_research", "research",
  "https://www.wikidata.org/w/api.php?action=wbgetclaims&entity=Q336264&format=json",
  "statements",
  "en", "[\"de\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Just the statements for an entity keyed by property (P17 country, P373 commons category, P571 inception, P1454 legal form, identifiers to ROR/GRID/ISNI/VIAF), each with snak hash, datatype, rank and statement GUID. The identifier-crosswalk workhorse.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.",
  NULL, wd_claims_flatten);
