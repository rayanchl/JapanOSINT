/* Verified-live co_public-finance sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(co_dnp_proyectos_sgr, "co-dnp-proyectos-sgr", "DNP - Proyectos del Sistema General de Regalias", "DNP - Proyectos del Sistema General de Regalias",
  "co_public-finance", "public-finance",
  "https://www.datos.gov.co/resource/mzgh-shtp.json?$limit=50&$order=:id",
  "",
  "es", "[\"co\",\"public-finance\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Royalty-funded investment projects: project name and BPIN code, executing entity, sector, department/municipality, approved value and status - where extractive royalties actually land.");
