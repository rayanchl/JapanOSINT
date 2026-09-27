/* Verified-live ch_corporate sources (9), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(gleif_lei_filter_lapsed, "gleif-lei-filter-lapsed", "GLEIF LEIs with LAPSED registration", "GLEIF LEIs with LAPSED registration",
  "ch_corporate", "corporate",
  "https://api.gleif.org/api/v1/lei-records?filter[registration.status]=LAPSED&page[size]=10",
  "data",
  "en", "[\"ch\",\"corporate\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "1,191,703 entities that stopped renewing their LEI - full records. A lapsed LEI on an entity still doing regulated business is a due-diligence signal, and the set is a ready-made watchlist.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(gleif_lei_filter_nonconforming, "gleif-lei-filter-nonconforming", "GLEIF non-conforming LEI records", "GLEIF non-conforming LEI records",
  "ch_corporate", "corporate",
  "https://api.gleif.org/api/v1/lei-records?filter[conformityFlag]=NON_CONFORMING&page[size]=10",
  "data",
  "en", "[\"ch\",\"corporate\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "1,300,990 LEI records flagged NON_CONFORMING by GLEIF's data-quality checks (missing parent reporting, stale renewals). Full records; a systematic list of entities whose corporate-structure disclosure is incomplete.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSON(gleif_lei_ultimate_children, "gleif-lei-ultimate-children", "GLEIF ultimate children of a LEI", "GLEIF ultimate children of a LEI",
  "ch_corporate", "corporate",
  "https://api.gleif.org/api/v1/lei-records/7LTWFZYICNSX8D621K86/ultimate-children?page[size]=15",
  "data",
  "en", "[\"ch\",\"corporate\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "416 entities that name this LEI as their ULTIMATE consolidating parent (vs 330 direct) - full LEI records with legal name, addresses, legal form and registration status. The whole group, not just tier one.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");
