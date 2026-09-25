/* Verified-live dz_microdata sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(ihsn_microdata_dz, "ihsn-microdata-dz", "IHSN survey catalog - Algeria", "IHSN survey catalog - Algeria",
  "dz_microdata", "microdata",
  "http://catalog.ihsn.org/index.php/api/catalog/search?format=json&country=Algeria&ps=50",
  "result.rows",
  "fr", "[\"dz\",\"microdata\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Algerian statistical studies including the 5eme Recensement General de la Population et de l'Habitat, with idno, title, type and year; full study metadata behind the idno detail hop.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
