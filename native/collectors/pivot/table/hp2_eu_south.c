/* collectors/sources/hp2_eu_south.c — Italy, Iberia, Greece, Malta and Cyprus.
 *
 * Southern Europe publishes two things almost nobody harvests: complete
 * subsidy and public-contract databases (Spain's BDNS, Italy's ANAC, Portugal's
 * BASE), and the supplier "casellario" annotations that record why a contractor
 * was excluded. Malta and Cyprus matter for a different reason — they are the
 * EU's two service jurisdictions of choice, so their company registers and
 * securities regulators sit on the paperwork of structures that operate
 * everywhere else. */
#include "lib/hpengine.h"

static const hp_source HP2_EU_SOUTH[] = {
  /* ── Italy ─────────────────────────────────────────────────────────────── */

  { .id = "IT_ANAC_OPENDATA", .name = "Italy ANAC — public contracts open data",
    .name_ja = "イタリア 反汚職庁 契約オープンデータ", .category = "government",
    .portal = "https://dati.anticorruzione.it", .record_type = "it-contract-dataset",
    .tags = "\"it\",\"procurement\",\"contracts\",\"opendata\"", .free_tier = 1,
    .url = "https://dati.anticorruzione.it/opendata/api/3/action/package_search?q={q}&rows=100&sort=id%20asc",
    .array_path = "result.results", .title_keys = "title,name",
    .id_keys = "id", .date_keys = "metadata_modified",
    .link_keys = "name", .link_tmpl = "https://dati.anticorruzione.it/opendata/dataset/{v}",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "Italy's anti-corruption authority publishes every public "
      "contract as open data, keyed by CIG — contracting station, economic "
      "operator, award procedure, amount and any subcontracts. This walks the "
      "whole catalogue with the resource files behind each dataset" },

  { .id = "IT_CONSOB_WARNINGS", .name = "Italy CONSOB — abusive intermediaries & sanctions",
    .name_ja = "イタリア CONSOB 無登録業者/制裁", .category = "government",
    .portal = "https://www.consob.it", .record_type = "it-regulator-warning",
    .tags = "\"it\",\"financial\",\"enforcement\",\"warning\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.consob.it/web/consob/ricerca?q={q}",
    .base = "https://www.consob.it", .filter_query = 1,
    .description = "CONSOB orders blacking out websites offering financial "
      "services abusively, its warning list of unauthorised intermediaries and "
      "its sanction resolutions against named firms and individuals" },

  { .id = "IT_GAZZETTA_UFFICIALE", .name = "Italy — Gazzetta Ufficiale",
    .name_ja = "イタリア 官報", .category = "government",
    .portal = "https://www.gazzettaufficiale.it", .record_type = "it-gazette",
    .tags = "\"it\",\"gazette\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.gazzettaufficiale.it/ricerca/testo/pubblicazione?query={q}",
    .base = "https://www.gazzettaufficiale.it", .filter_query = 1,
    .description = "Italy's official journal — concessions, expropriations, "
      "extraordinary administration decrees, prefectural anti-mafia measures "
      "and the ministerial decrees that name individual companies" },

  /* ── Spain ─────────────────────────────────────────────────────────────── */

  { .id = "ES_BOE_GAZETTE", .name = "Spain BOE — state official gazette",
    .name_ja = "スペイン 官報", .category = "government",
    .portal = "https://www.boe.es", .record_type = "es-gazette",
    .tags = "\"es\",\"gazette\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.boe.es/buscar/?q={q}",
    .base = "https://www.boe.es", .filter_query = 1,
    .description = "Spain's official state gazette — sanctions resolutions, "
      "concessions, licence grants and revocations, subsidy awards and the "
      "administrative penalties published against named entities" },

  { .id = "ES_CNMV_ENTITIES", .name = "Spain CNMV — registered entities & warnings",
    .name_ja = "スペイン CNMV 登録先/警告", .category = "government",
    .portal = "https://www.cnmv.es", .record_type = "es-financial-entity",
    .tags = "\"es\",\"financial\",\"licence\",\"warning\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.cnmv.es/portal/consultas/busqueda?q={q}",
    .base = "https://www.cnmv.es", .filter_query = 1,
    .description = "Spanish securities regulator registers of investment "
      "firms, funds and their managers, the significant-shareholding "
      "notifications filed against listed issuers, and the public warnings "
      "about unauthorised entities" },

  { .id = "ES_PLACSP_CONTRACTS", .name = "Spain — public sector contracting platform",
    .name_ja = "スペイン 公共調達プラットフォーム", .category = "government",
    .portal = "https://contrataciondelestado.es", .record_type = "es-tender",
    .tags = "\"es\",\"procurement\",\"contracts\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://contrataciondelestado.es/wps/portal/plataforma?q={q}",
    .base = "https://contrataciondelestado.es", .filter_query = 1,
    .description = "Spanish tender notices, award decisions and formalised "
      "contracts across state, regional and local authorities, with the "
      "awarded bidder, the amount and the CPV classification" },

  { .id = "ES_BDNS_SUBSIDIES", .name = "Spain BDNS — national subsidy database",
    .name_ja = "スペイン 補助金データベース", .category = "government",
    .portal = "https://www.infosubvenciones.es", .record_type = "es-subsidy",
    .tags = "\"es\",\"subsidy\",\"grants\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.infosubvenciones.es/bdnstrans/GE/es/concesiones?beneficiario={q}",
    .base = "https://www.infosubvenciones.es", .filter_query = 1,
    .description = "Every public subsidy granted in Spain by any level of "
      "government — beneficiary NIF and name, the granting body, the amount "
      "and the call it came from. One of the most complete public-money "
      "beneficiary databases anywhere in the EU" },

  { .id = "ES_CNMC_SANCTIONS", .name = "Spain CNMC — competition & regulatory sanctions",
    .name_ja = "スペイン CNMC 制裁", .category = "government",
    .portal = "https://www.cnmc.es", .record_type = "es-enforcement",
    .tags = "\"es\",\"antitrust\",\"enforcement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.cnmc.es/buscador?search={q}",
    .base = "https://www.cnmc.es", .filter_query = 1,
    .description = "Spanish competition and markets authority decisions — "
      "cartel fines, bid-rigging cases (which also trigger procurement "
      "prohibitions), merger clearances and energy and telecom regulatory "
      "sanctions, with the undertakings named" },

  /* ── Portugal ──────────────────────────────────────────────────────────── */
  { .id = "PT_BASE_CONTRACTS", .name = "Portugal BASE — public contracts portal",
    .name_ja = "ポルトガル 公共契約ポータル", .category = "government",
    .portal = "https://www.base.gov.pt", .record_type = "pt-contract",
    .tags = "\"pt\",\"procurement\",\"contracts\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.base.gov.pt/Base4/pt/pesquisa/?type=contratos&texto={q}",
    .base = "https://www.base.gov.pt", .filter_query = 1,
    .description = "Portuguese public contracts — contracting authority, "
      "supplier NIF, object, procedure type, value and execution deadline, "
      "including the direct adjustments that make up most of the volume" },

  { .id = "PT_DRE_GAZETTE", .name = "Portugal — Diário da República",
    .name_ja = "ポルトガル 官報", .category = "government",
    .portal = "https://dre.pt", .record_type = "pt-gazette",
    .tags = "\"pt\",\"gazette\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://dre.pt/dre/pesquisa?search={q}",
    .base = "https://dre.pt", .filter_query = 1,
    .description = "Portugal's official gazette — licence grants, concessions, "
      "regulatory decisions and the insolvency and corporate notices published "
      "in the second series" },

  /* ── Greece ────────────────────────────────────────────────────────────── */

  { .id = "GR_PROMITHEUS_TENDERS", .name = "Greece — national procurement system (KIMDIS)",
    .name_ja = "ギリシャ 公共調達", .category = "government",
    .portal = "https://www.promitheus.gov.gr", .record_type = "gr-tender",
    .tags = "\"gr\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.promitheus.gov.gr/?s={q}",
    .base = "https://www.promitheus.gov.gr", .filter_query = 1,
    .description = "Greek central electronic procurement — contract notices, "
      "awards and the ADAM-numbered contracts that must also appear in "
      "Diavgeia, giving two independent records of the same award" },

  /* ── Malta & Cyprus ────────────────────────────────────────────────────── */
  { .id = "MT_MBR_COMPANIES", .name = "Malta — business registry company search",
    .name_ja = "マルタ 企業登記", .category = "government",
    .portal = "https://registry.mbr.mt", .record_type = "mt-entity",
    .tags = "\"mt\",\"registry\",\"ownership\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://registry.mbr.mt/ROC/index.jsp?searchTerm={q}",
    .base = "https://registry.mbr.mt", .filter_query = 1,
    .description = "Maltese registered companies — registration number, "
      "directors, company secretary, shareholders and the annual returns and "
      "accounts filed. Malta names shareholders on the public register, which "
      "makes it materially more useful than most EU registers" },

  { .id = "CY_REGISTRAR_COMPANIES", .name = "Cyprus — registrar of companies search",
    .name_ja = "キプロス 法人登記", .category = "government",
    .portal = "https://efiling.drcor.mcit.gov.cy", .record_type = "cy-entity",
    .tags = "\"cy\",\"registry\",\"ownership\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://efiling.drcor.mcit.gov.cy/DrcorPublic/SearchResults.aspx?name={q}",
    .base = "https://efiling.drcor.mcit.gov.cy", .filter_query = 1,
    .description = "Cypriot companies, partnerships and overseas branches — "
      "registration number, status, registered office, directors, secretary "
      "and shareholders. Cyprus sits in the ownership chain of an enormous "
      "share of post-Soviet corporate structures" },

};

HP_REGISTER_TABLE(HP2_EU_SOUTH)
