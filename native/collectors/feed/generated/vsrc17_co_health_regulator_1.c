/* Verified-live co_health-regulator sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(co_reps_directorio_georreferenciado, "co-reps-directorio-georreferenciado", "Directorio Georreferenciado de Prestadores de Servicios de Salud (REPS)", "Directorio Georreferenciado de Prestadores de Servicios de Salud (REPS)",
  "co_health-regulator", "health-regulator",
  "https://www.datos.gov.co/resource/7r2w-27jm.json?$limit=50&$order=:id",
  "",
  "es", "[\"co\",\"health-regulator\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Geolocated directory of licensed Colombian health providers - provider name, NIT/identification, address, municipality, coordinates and enabled services.");
