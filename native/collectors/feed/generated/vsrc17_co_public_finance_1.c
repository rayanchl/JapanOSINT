/* Verified-live co_public-finance sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "_vjson_idkeys.inc"

/* 2026-11-09: keyed on Socrata's own row id `:id` (asked for with
 * `$select=*,:id`, since Socrata serves it only when asked). VJSON's id list
 * found nothing on these records and fell back to title+link+date, which
 * merged real rows: measured 35,010 rows, :id 35,010 distinct, the fallback losing 1 a run; $limit 50 -> 2000 so the whole set arrives in 18 pages. `:id` is distinct on every row. */
VJSON_IDKEYS(co_dnp_proyectos_sgr, "co-dnp-proyectos-sgr", "DNP - Proyectos del Sistema General de Regalias", "DNP - Proyectos del Sistema General de Regalias",
  "co_public-finance", "public-finance",
  "https://www.datos.gov.co/resource/mzgh-shtp.json?$select=*,:id&$limit=2000&$order=:id",
  "",
  "es", "[\"co\",\"public-finance\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Royalty-funded investment projects: project name and BPIN code, executing entity, sector, department/municipality, approved value and status - where extractive royalties actually land.",
  ":id");
