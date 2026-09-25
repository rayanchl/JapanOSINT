/* Verified-live np_organizations sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(np_bipad_organization, "np-bipad-organization", "Nepal BIPAD - registered disaster-response organizations", "Nepal BIPAD - registered disaster-response organizations",
  "np_organizations", "organizations",
  "https://bipadportal.gov.np/api/v1/organization/?format=json&limit=50",
  "results",
  "en", "[\"np\",\"organizations\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Organization register: title (e.g. DAO Sindhupalchok), shortName, longName, description, level, startingDateAd, point geometry, and the province/district/municipality/ward it is bound to plus responsibleFor scope. Detail endpoint verified with id 2.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
