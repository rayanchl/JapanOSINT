/* Verified-live ml_health sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* 2026-11-09: DHS answered 100 of its declared RecordCount (Uganda 20,391) a
 * run, and keyed on VJSON's title fallback — the Indicator label, shared by
 * every survey, sex and region breakdown of that indicator. Measured:
 * perpage=5000 is honoured, page=2 answers the next 5,000, and DataId is
 * distinct on every record (5,000 of 5,000 per page; 150,718 = RecordCount
 * summed over eight countries). `page=1` is bound so lib/jsonlist.c advances
 * it against the declared RecordCount; keyed on DataId. */
VJSON_KEYED(dhs_data_ml, "dhs-data-ml", "DHS Program indicator data - Mali", "DHS Program indicator data - Mali",
  "ml_health", "health",
  "https://api.dhsprogram.com/rest/dhs/data?countryIds=ML&f=json&perpage=5000&page=1",
  "data",
  "en", "[\"ml\",\"health\",\"batch17\",\"high-penetrancy\"]", 43200,
  "DHS observations for Mali with indicator label, value, survey id/year and characteristic breakdowns.",
  "DataId");
