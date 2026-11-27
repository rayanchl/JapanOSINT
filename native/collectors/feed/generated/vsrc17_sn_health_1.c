/* Verified-live sn_health sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "_vjson_idkeys.inc"

/* id_keys "DataId", MEASURED 2026-11-14: no name on VJSON's uid list matches a
   DHS record, so it keyed on hash(title, link, date) — 568 distinct of the
   first 1,000 Morocco records, and 838 of 4,092 collapsed in the engine the
   moment the row paged. DataId is the API's own observation id: 4,092 of
   4,092 (MA), 13,057 of 13,057 (EG), 38,088 of 38,088 (SN), each read whole. */
VJSON_IDKEYS(dhs_data_sn, "dhs-data-sn", "DHS Program indicator data - Senegal", "DHS Program indicator data - Senegal",
  "sn_health", "health",
  /* perpage=1000, MEASURED 2026-11-14: this row read ONE page of 100 until
     `perpage` joined lib/jsonlist.c's PAGERS. The API honours 1,000 a page
     (RecordsReturned 1000, TotalPages 20 for Ghana's 19,342), so the default
     20-page ceiling reads up to 20,000; the remainder, if any, is disclosed
     against the envelope's own RecordCount.
     perpage=5000, MEASURED 2026-11-16: this country's 38,088 exceed 20,000,
     so at 1,000 a page the rest was disclosed and never read. The API honours
     5,000 (Senegal: RecordsReturned 5000, TotalPages 8, 5,000 distinct
     DataId), so the collection is 8 pages and reads whole in one run. */
  "https://api.dhsprogram.com/rest/dhs/data?countryIds=SN&f=json&perpage=5000",
  "data",
  "en", "[\"sn\",\"health\",\"batch17\",\"high-penetrancy\"]", 43200,
  "DHS observations for Senegal: indicator label, value, survey id and year, characteristic category. Senegal has a long continuous-DHS series.",
  "DataId");
