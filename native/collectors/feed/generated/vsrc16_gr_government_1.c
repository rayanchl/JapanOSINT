/* Verified-live gr_government sources (3), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on ada, the ADA — Diavgeia's own unique decision id, which every
 * published act carries and which the record shape puts at the top level. A
 * decision record has no `id` field, so the jsonlist precedence list found
 * nothing and the row hashed (title, link, date); Greek subject lines repeat
 * verbatim across acts of one body, and the sweep measured 100 emitted / 90
 * stored. Measured live 2026-09-16 over three pages: 300 records, 300 distinct
 * ada, 300 distinct serialisations; protocolNumber gives only 294 and is NOT
 * the key.
 *
 * size 5 -> 100 in the same measurement: the walk was spending 20 requests to
 * reach 100 records. The upstream accepts size=100 and answers a full page, so
 * those same 100 records now arrive in ONE request — measured 859ms against
 * 16.8s. The walk still stops at 100 and files a collector-truncation-notice
 * saying so; pushing the reach past 100 is a separate paging question and is
 * NOT settled here, so do not read this change as making the row deeper. */
VJSON_KEYED(gr_diavgeia_decision_search, "gr-diavgeia-decision-search", "Diavgeia (Greece) — government decision search", "Diavgeia (Greece) — government decision search",
  "gr_government", "government",
  "https://diavgeia.gov.gr/opendata/search.json?q=organizationUid:%2215168%22&size=100",
  "decisions",
  "el", "[\"gr\",\"government\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Every published act of every Greek public body, including expenditure approvals, procurement awards and payment orders. Each row gives the ADA (unique decision id), protocol number, full Greek subject line, decision type, issuing organisation and unit, signer, and thematic categories. Queryable by organizationUid, decision type, date and free text — this is the single deepest 'who was paid by whom' feed in Greece.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.",
  "ada");

VJSON(gr_diavgeia_dictionaries, "gr-diavgeia-dictionaries", "Diavgeia — controlled-vocabulary index", "Diavgeia — controlled-vocabulary index",
  "gr_government", "government",
  "https://diavgeia.gov.gr/opendata/dictionaries.json",
  "dictionaries",
  "el", "[\"gr\",\"government\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Index of the 19 reference dictionaries Diavgeia records reference (CPV, CURRENCY, ADMIN_STRUCTURE_KALLIKRATIS administrative geography, and 16 more), each with uid and label. Each uid dereferences to the full vocabulary.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

/* emitted 25,165 / stored 233 is CORRECT dedupe, and the 25,134 in the old
 * description was a count of repeats, not of posts. Measured 2026-09-11: the
 * endpoint returns 25,188 records carrying exactly two fields each, uid and
 * label; there are 233 distinct uids, 231 distinct labels, 233 distinct
 * (uid,label) pairs, and no uid ever appears with two different labels —
 * POS_10007 alone is repeated 2,989 times byte for byte. Every collapsed record
 * is an exact duplicate of one that was kept, so nothing is lost and there is
 * no other field that could key them apart. Do not re-key this row. */
VJSON(gr_diavgeia_positions, "gr-diavgeia-positions", "Diavgeia — official position register", "Diavgeia — official position register",
  "gr_government", "government",
  "https://diavgeia.gov.gr/opendata/positions.json",
  "positions",
  "el", "[\"gr\",\"government\",\"batch16\",\"high-penetrancy\"]", 86400,
  "233 distinct official post titles (uid + Greek label: Deputy Minister, Director-General, Special Secretary, ...) used to resolve the signer of an expenditure or award decision to a role. The 1.6 MB file repeats them 25,188 times; the repeats are byte-identical.");
