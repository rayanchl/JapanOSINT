/* Verified-live bf_microdata sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(ihsn_microdata_bf, "ihsn-microdata-bf", "IHSN survey catalog - Burkina Faso", "IHSN survey catalog - Burkina Faso",
  "bf_microdata", "microdata",
  "http://catalog.ihsn.org/index.php/api/catalog/search?format=json&country=Burkina%20Faso&ps=50",
  "result.rows",
  "en", "[\"bf\",\"microdata\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Burkinabe studies with idno, title, authoring entity and year; detail hop yields producers, sampling and file lists.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
