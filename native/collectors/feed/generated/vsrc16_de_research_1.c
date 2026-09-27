/* Verified-live de_research sources (9), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* datacite-prefixes / datacite-repositories: these two rows declared the path
 * "meta.years", which is DataCite's per-year FACET (10 {id,title,count}
 * buckets), not the registry — so each run stored ten year-counts and none of
 * the 5,755 prefixes or 4,496 repositories, which sit under "data". Measured
 * 2026-09-15. page[size] goes 5 -> 1000 (DataCite's maximum, accepted on both
 * endpoints) so the walk reaches the end in 6 and 5 pages instead of stopping
 * at the page ceiling after 100 records. */

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
