/* Verified-live cd_microdata sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(ihsn_microdata_cd, "ihsn-microdata-cd", "IHSN survey catalog - Congo (DRC region)", "IHSN survey catalog - Congo (DRC region)",
  "cd_microdata", "microdata",
  /* `country=Congo` was IGNORED upstream (params.countries:[-1], found 0), so the
   * row stored nothing on every run. The catalogue honours ISO3: measured
   * 2026-09-15, country_iso3=COD found 97 DRC studies and country_iso3=ZZZ found
   * 0 (the filter is real). ps=100 holds all 97; page=1 lets the walk continue
   * when the DRC set grows past one page. */
  "https://catalog.ihsn.org/index.php/api/catalog/search?format=json&country_iso3=COD&ps=100&page=1",
  "result.rows",
  "en", "[\"cd\",\"microdata\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Congo/DRC study records including the 15th General Population and Housing Census, with idno, title, type and year; full metadata behind the idno detail hop. NOTE: the 'Congo' filter also matches Republic of the Congo studies.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
