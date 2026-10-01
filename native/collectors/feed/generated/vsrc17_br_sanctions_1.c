/* Verified-live br_sanctions sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(br_tcu_inabilitados, "br-tcu-inabilitados", "TCU - responsaveis inabilitados para funcao publica", "TCU - responsaveis inabilitados para funcao publica",
  "br_sanctions", "sanctions",
  "https://contas.tcu.gov.br/ords/condenacao/consulta/inabilitados?limit=500&offset=0", /* ORDS pages 25 by default and announces the rest only as links[rel=next] inside an ARRAY, which the page walk does not read: the bare URL stopped at 25 with hasMore=true. ORDS caps limit at 500 (asked 1,000, served 500); limit+offset lets the walk continue */
  "items",
  "pt", "[\"br\",\"sanctions\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Named individuals barred by the federal audit court from holding public office: full name, CPF, TCU process number, the deliberation (acordao) that imposed it, date of final judgment, expiry date of the bar, and UF/municipality. Auto-labels on 'nome'.");

VJSON(br_tcu_inidoneos, "br-tcu-inidoneos", "TCU - licitantes declarados inidoneos", "TCU - licitantes declarados inidoneos",
  "br_sanctions", "sanctions",
  "https://contas.tcu.gov.br/ords/condenacao/consulta/inidoneos",
  "items",
  "pt", "[\"br\",\"sanctions\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Companies and people declared unfit to bid for federal contracts: legal name, CPF or CNPJ, TCU process number, acordao reference, date of final judgment, debarment expiry date, UF. The keyless counterpart to CEIS/CNEP. Auto-labels on 'nome'.");
