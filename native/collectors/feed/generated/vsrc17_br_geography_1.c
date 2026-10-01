/* Verified-live br_geography sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(br_ibge_municipio_distritos, "br-ibge-municipio-distritos", "IBGE Localidades - distritos de um municipio", "IBGE Localidades - distritos de um municipio",
  "br_geography", "geography",
  "https://servicodados.ibge.gov.br/api/v1/localidades/municipios/3550308/distritos",
  "",
  "pt", "[\"br\",\"geography\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Sub-municipal districts behind a municipality - 96 for Sao Paulo - each with district id, name and the full municipio/microrregiao/mesorregiao/UF chain. Auto-labels on 'nome'.");
