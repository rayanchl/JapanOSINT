/* collectors/sources/hp3_gov_latam_public.c — Latin American public record.
 *
 * Latin America has some of the strongest transparency law in the world and
 * some of the weakest indexing of what that law produces. Chile's lobbying act
 * forces every minister to publish who they met and what was discussed, as an
 * API. Brazil's Chamber of Deputies publishes every deputy's individual
 * expense claim, as an API. Querido Diário has normalised the daily gazettes of
 * thousands of Brazilian municipalities, where local contracts, expropriations
 * and appointments actually appear. Panama's public registry is the other end
 * of half the world's offshore structures.
 *
 * `hp2_southam_*.c` and `hp2_northam_ca_mx.c` wired the sanction and registry
 * layer. This file wires the legislative, gazette, lobbying and Central
 * American procurement layer sitting alongside it. */
#include "../../lib/hpengine.h"

static const hp_source HP3_GOV_LATAM[] = {
  /* ── Brazil: the legislature and the gazettes ─────────────────────────── */
  { .id = "BR_CAMARA_DEPUTADOS", .name = "Brazil — Chamber of Deputies open data API",
    .name_ja = "ブラジル下院 オープンデータ", .category = "government",
    .portal = "https://dadosabertos.camara.leg.br", .record_type = "br-legislator",
    .tags = "\"br\",\"parliament\",\"politics\"", .free_tier = 1,
    .url = "https://dadosabertos.camara.leg.br/api/v2/deputados?nome={q}&itens=100",
    .array_path = "dados", .title_keys = "nome,siglaPartido", .id_keys = "id",
    .page_param = "pagina", .page_size = 100, .page_max = 30,
    .detail_url = "https://dadosabertos.camara.leg.br/api/v2/deputados/{v}/despesas?itens=100",
    .detail_key = "id", .detail_path = "dados",
    .description = "Brazilian federal deputies with the second hop into their "
      "parliamentary quota expenses — every supplier paid, its CNPJ, the "
      "amount and the document number. Deputy-to-company payments at invoice "
      "level, which is where ghost-supplier schemes surface" },

  { .id = "BR_SENADO_ABERTO", .name = "Brazil — Federal Senate open data",
    .name_ja = "ブラジル上院 オープンデータ", .category = "government",
    .portal = "https://legis.senado.leg.br", .record_type = "br-legislator",
    .tags = "\"br\",\"parliament\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www6g.senado.leg.br/busca/?q={q}",
    .base = "https://www6g.senado.leg.br", .filter_query = 1,
    .description = "Senators, their mandates, party history, committee seats, "
      "bills authored and the votes cast — plus the Senate's own procurement "
      "and staff appointment publications" },

  { .id = "BR_QUERIDO_DIARIO", .name = "Querido Diário — Brazilian municipal gazettes",
    .name_ja = "ブラジル 市町村官報検索", .category = "government",
    .portal = "https://queridodiario.ok.org.br", .record_type = "br-municipal-gazette",
    .tags = "\"br\",\"gazette\",\"local-government\"", .free_tier = 1,
    .url = "https://queridodiario.ok.org.br/api/gazettes?querystring={q}&size=100",
    .array_path = "gazettes", .title_keys = "territory_name,state_code",
    .id_keys = "file_checksum", .date_keys = "date", .link_keys = "url",
    .page_param = "offset", .page_size = 100, .page_start = 0, .page_max = 40,
    .description = "Open Knowledge Brasil's normalised archive of daily "
      "gazettes from thousands of Brazilian municipalities — local contract "
      "awards, emergency purchases, expropriations, appointments and licences "
      "that exist nowhere else in machine-readable form" },

  { .id = "BR_DOU_GAZETTE", .name = "Brazil — Diário Oficial da União",
    .name_ja = "ブラジル 連邦官報", .category = "government",
    .portal = "https://www.in.gov.br", .record_type = "br-gazette-notice",
    .tags = "\"br\",\"gazette\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.in.gov.br/consulta/-/buscar/dou?q={q}",
    .base = "https://www.in.gov.br", .filter_query = 1,
    .description = "The federal official gazette — appointments, licence "
      "grants and revocations, sanctions, contract extracts and the "
      "administrative decisions that only take effect on publication here" },

  { .id = "BR_TCU_AUDIT_DECISIONS", .name = "Brazil TCU — federal audit court decisions",
    .name_ja = "ブラジル 会計検査院 裁定", .category = "government",
    .portal = "https://pesquisa.apps.tcu.gov.br", .record_type = "br-audit-decision",
    .tags = "\"br\",\"audit\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://pesquisa.apps.tcu.gov.br/pesquisa/acordao?termo={q}",
    .base = "https://pesquisa.apps.tcu.gov.br", .filter_query = 1,
    .description = "Tribunal de Contas da União rulings — irregular accounts, "
      "surcharge orders against named officials and contractors, and the "
      "declarations of ineligibility that bar a firm from federal contracting" },

  { .id = "BR_CGU_LENIENCY_AGREEMENTS", .name = "Brazil CGU — leniency agreements & accountability",
    .name_ja = "ブラジル CGU 司法取引協定", .category = "government",
    .portal = "https://www.gov.br", .record_type = "br-leniency-agreement",
    .tags = "\"br\",\"corruption\",\"enforcement\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.gov.br/cgu/pt-br/busca?SearchableText={q}",
    .base = "https://www.gov.br", .filter_query = 1,
    .description = "Comptroller General leniency agreements under the Clean "
      "Company Act — the company, the conduct admitted, the fine and the "
      "compliance obligations imposed. The Brazilian corporate equivalent of a "
      "deferred prosecution agreement" },

  /* ── Chile: the lobbying act as an API ────────────────────────────────── */
  { .id = "CL_LEY_LOBBY_AUDIENCIAS", .name = "Chile Ley del Lobby — official meetings register",
    .name_ja = "チリ ロビー法 面談登録", .category = "government",
    .portal = "https://www.leylobby.gob.cl", .record_type = "cl-lobby-meeting",
    .tags = "\"cl\",\"lobbying\",\"influence\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.leylobby.gob.cl/instituciones/busqueda?q={q}",
    .base = "https://www.leylobby.gob.cl", .filter_query = 1,
    .description = "Chile requires every minister, undersecretary, mayor and "
      "regulator to publish each meeting with an outside party — who attended, "
      "for which company, what was discussed and what was sought. Alongside it "
      "the gifts and paid travel registers. There is no comparable dataset in "
      "the hemisphere" },

  { .id = "CL_INFOPROBIDAD_DECLARATIONS", .name = "Chile — public officials' interest declarations",
    .name_ja = "チリ 公職者利益申告", .category = "government",
    .portal = "https://www.infoprobidad.cl", .record_type = "cl-asset-declaration",
    .tags = "\"cl\",\"conflict-of-interest\",\"assets\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.infoprobidad.cl/Consultas/Declaraciones?nombre={q}",
    .base = "https://www.infoprobidad.cl", .filter_query = 1,
    .description = "Declarations of interest and assets filed by Chilean public "
      "officials — company shareholdings, directorships, real property and "
      "the interests of a spouse, published in full rather than summarised" },

  { .id = "CL_TRANSPARENCIA_ACTIVA", .name = "Chile — active transparency disclosures",
    .name_ja = "チリ 能動的情報公開", .category = "government",
    .portal = "https://www.portaltransparencia.cl", .record_type = "cl-transparency",
    .tags = "\"cl\",\"transparency\",\"public-money\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.portaltransparencia.cl/PortalPdT/buscador?q={q}",
    .base = "https://www.portaltransparencia.cl", .filter_query = 1,
    .description = "Every Chilean public body must publish staff and their "
      "salaries, contracts, transfers to third parties and consultancy "
      "agreements on a fixed schedule. This is the search across all of them" },

  /* ── Colombia, Argentina, Peru ────────────────────────────────────────── */
  { .id = "CO_DATOS_ABIERTOS_CATALOG", .name = "Colombia datos.gov.co — open data catalog",
    .name_ja = "コロンビア オープンデータ目録", .category = "government",
    .portal = "https://www.datos.gov.co", .record_type = "co-dataset",
    .tags = "\"co\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://www.datos.gov.co/api/catalog/v1?q={q}&limit=100",
    .array_path = "results", .title_keys = "resource.name,resource.description",
    .id_keys = "resource.id", .date_keys = "resource.updatedAt",
    .link_keys = "permalink",
    .page_param = "offset", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Colombia's Socrata catalog — the SECOP contract extracts, "
      "sanction registers, subsidy rolls and the victim and land-restitution "
      "datasets, each with a queryable API endpoint" },

  { .id = "CO_SIGEP_PUBLIC_SERVANTS", .name = "Colombia SIGEP — public employment & declarations",
    .name_ja = "コロンビア 公務員情報", .category = "government",
    .portal = "https://www.funcionpublica.gov.co", .record_type = "co-public-servant",
    .tags = "\"co\",\"public-sector\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.funcionpublica.gov.co/busqueda?q={q}",
    .base = "https://www.funcionpublica.gov.co", .filter_query = 1,
    .description = "Colombia's public employment information system — the "
      "named officeholder, the post, the entity and the CV and asset "
      "declaration filed on appointment" },

  { .id = "AR_HCDN_DIPUTADOS", .name = "Argentina — Chamber of Deputies open data",
    .name_ja = "アルゼンチン下院 オープンデータ", .category = "government",
    .portal = "https://datos.hcdn.gob.ar", .record_type = "ar-legislator",
    .tags = "\"ar\",\"parliament\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://datos.hcdn.gob.ar/dataset?q={q}",
    .base = "https://datos.hcdn.gob.ar", .filter_query = 1,
    .description = "Argentine deputies, their bills, votes, committee "
      "membership and sworn asset declarations, published as bulk datasets by "
      "the Chamber itself" },

  { .id = "AR_OFICINA_ANTICORRUPCION", .name = "Argentina — anti-corruption office declarations",
    .name_ja = "アルゼンチン 反汚職局 資産申告", .category = "government",
    .portal = "https://www.argentina.gob.ar", .record_type = "ar-asset-declaration",
    .tags = "\"ar\",\"conflict-of-interest\",\"assets\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.argentina.gob.ar/buscar/{q}",
    .base = "https://www.argentina.gob.ar", .filter_query = 1,
    .description = "The Oficina Anticorrupción's sworn asset declarations for "
      "national officials, its conflict-of-interest rulings and the register of "
      "gifts received in office" },

  { .id = "PE_INFOBRAS_PUBLIC_WORKS", .name = "Peru INFOBRAS — public works project tracking",
    .name_ja = "ペルー 公共事業追跡", .category = "government",
    .portal = "https://apps.contraloria.gob.pe", .record_type = "pe-public-works",
    .tags = "\"pe\",\"infrastructure\",\"public-money\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://apps.contraloria.gob.pe/ciudadano/wfm_obras_resultado.aspx?nom={q}",
    .base = "https://apps.contraloria.gob.pe", .filter_query = 1,
    .description = "Peru's Comptroller General tracks every public works "
      "project — the executing entity, the contractor, budget versus actual "
      "spend, physical progress and whether the works were abandoned. "
      "Unfinished public works are a durable corruption signal" },

  /* ── Mexico ───────────────────────────────────────────────────────────── */
  { .id = "MX_PLATAFORMA_TRANSPARENCIA", .name = "Mexico — national transparency platform",
    .name_ja = "メキシコ 全国透明性プラットフォーム", .category = "government",
    .portal = "https://www.plataformadetransparencia.org.mx",
    .record_type = "mx-transparency-obligation",
    .tags = "\"mx\",\"transparency\",\"foi\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.plataformadetransparencia.org.mx/buscador?texto={q}",
    .base = "https://www.plataformadetransparencia.org.mx", .filter_query = 1,
    .description = "Every Mexican public body's mandatory disclosures in one "
      "search — payroll, contracts, concessions, permits, travel expenses and "
      "the answered access-to-information requests, federal and state" },

  { .id = "MX_DECLARANET_ASSETS", .name = "Mexico DeclaraNet — officials' asset declarations",
    .name_ja = "メキシコ 公職者資産申告", .category = "government",
    .portal = "https://declaranet.gob.mx", .record_type = "mx-asset-declaration",
    .tags = "\"mx\",\"conflict-of-interest\",\"assets\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://declaranet.gob.mx/consulta?nombre={q}",
    .base = "https://declaranet.gob.mx", .filter_query = 1,
    .description = "Asset, interest and tax declarations of Mexican federal "
      "public servants — property, vehicles, investments, debts and the "
      "positions held by family members in the private sector" },

  { .id = "MX_INE_FISCALIZACION", .name = "Mexico INE — political party finance audits",
    .name_ja = "メキシコ選管 政党会計監査", .category = "government",
    .portal = "https://www.ine.mx", .record_type = "mx-political-finance",
    .tags = "\"mx\",\"political-finance\",\"elections\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.ine.mx/buscador/?q={q}",
    .base = "https://www.ine.mx", .filter_query = 1,
    .description = "The National Electoral Institute's audits of party and "
      "campaign finance — reported income and spending, the suppliers paid, "
      "the sanctions imposed and the undeclared contributions found" },

  /* ── Central America, Panama and the Caribbean ────────────────────────── */
  { .id = "PA_REGISTRO_PUBLICO", .name = "Panama — public registry of companies",
    .name_ja = "パナマ 公的登記所", .category = "government",
    .portal = "https://www.registro-publico.gob.pa", .record_type = "pa-entity",
    .tags = "\"pa\",\"registry\",\"offshore\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.registro-publico.gob.pa/es/consultas?busqueda={q}",
    .base = "https://www.registro-publico.gob.pa", .filter_query = 1,
    .description = "Panama's public registry — the corporation, its folio, "
      "directors and dignitaries, the resident agent law firm and the "
      "registered capital. The other end of a very large share of the world's "
      "offshore structures, and searchable at no cost" },

  { .id = "PA_PANAMACOMPRA", .name = "Panama — PanamaCompra procurement portal",
    .name_ja = "パナマ 公共調達", .category = "government",
    .portal = "https://www.panamacompra.gob.pa", .record_type = "pa-procurement",
    .tags = "\"pa\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.panamacompra.gob.pa/Inicio/#!/busquedaAvanzada?keyword={q}",
    .base = "https://www.panamacompra.gob.pa", .filter_query = 1,
    .description = "Panamanian state purchasing — tender notices, awards, the "
      "supplier register and the list of contractors disqualified from public "
      "contracting" },

  { .id = "CR_SICOP_PROCUREMENT", .name = "Costa Rica SICOP — integrated procurement system",
    .name_ja = "コスタリカ 統合調達システム", .category = "government",
    .portal = "https://www.sicop.go.cr", .record_type = "cr-procurement",
    .tags = "\"cr\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.sicop.go.cr/index.jsp?keyword={q}",
    .base = "https://www.sicop.go.cr", .filter_query = 1,
    .description = "Costa Rica runs all public purchasing through one system — "
      "the tender, every bid received with its price, the evaluation and the "
      "award. Losing bids are published, which most systems do not do" },

  { .id = "GT_GUATECOMPRAS", .name = "Guatemala — Guatecompras procurement",
    .name_ja = "グアテマラ 公共調達", .category = "government",
    .portal = "https://www.guatecompras.gt", .record_type = "gt-procurement",
    .tags = "\"gt\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.guatecompras.gt/concursos/consultaConcurso.aspx?busqueda={q}",
    .base = "https://www.guatecompras.gt", .filter_query = 1,
    .description = "Guatemalan public purchasing with the supplier register, "
      "the awards and the sanctioned-supplier list, plus the direct-purchase "
      "records that account for most of the spend" },

  { .id = "DO_COMPRAS_DOMINICANA", .name = "Dominican Republic — public procurement portal",
    .name_ja = "ドミニカ共和国 公共調達", .category = "government",
    .portal = "https://comunidad.comprasdominicana.gob.do",
    .record_type = "do-procurement",
    .tags = "\"do\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://comunidad.comprasdominicana.gob.do/Public/Tendering/"
      "ContractNoticeManagement/Index?keyword={q}",
    .base = "https://comunidad.comprasdominicana.gob.do", .filter_query = 1,
    .description = "Dominican tender notices and awards with the supplier RNC, "
      "the contracting institution and the amount — the Caribbean's most "
      "complete procurement disclosure" },

  { .id = "SV_COMPRASAL", .name = "El Salvador — COMPRASAL procurement",
    .name_ja = "エルサルバドル 公共調達", .category = "government",
    .portal = "https://www.comprasal.gob.sv", .record_type = "sv-procurement",
    .tags = "\"sv\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.comprasal.gob.sv/comprasalpublico/busqueda?texto={q}",
    .base = "https://www.comprasal.gob.sv", .filter_query = 1,
    .description = "Salvadoran public procurement notices, contracts and "
      "supplier records, including the emergency purchases made outside the "
      "normal competitive process" },

  { .id = "CARIBBEAN_COMPANY_REGISTRIES", .name = "Caribbean — company registry search points",
    .name_ja = "カリブ諸国 法人登記", .category = "government",
    .portal = "https://www.opencorporates.com", .record_type = "caribbean-entity",
    .tags = "\"caribbean\",\"registry\",\"offshore\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://opencorporates.com/companies?jurisdiction_code=&q={q}"
      "&utf8=%E2%9C%93",
    .base = "https://opencorporates.com", .filter_query = 1,
    .description = "The consolidated search across Caribbean company registers "
      "— Jamaica, Trinidad and Tobago, Barbados, the Bahamas, Belize, the BVI "
      "and Cayman where published. Small jurisdictions with outsized roles in "
      "holding structures and captive insurance" },

  { .id = "LATAM_IDB_PROJECT_DOCUMENTS", .name = "IDB — project documents & procurement",
    .name_ja = "米州開発銀行 事業文書", .category = "government",
    .portal = "https://www.iadb.org", .record_type = "mdb-project",
    .tags = "\"latam\",\"mdb\",\"funding\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.iadb.org/en/search?keyword={q}",
    .base = "https://www.iadb.org", .filter_query = 1,
    .description = "Inter-American Development Bank project documents — the "
      "executing agency, the loan amount, the procurement plan and the "
      "supervision reports that record implementation failures by name" },
};

HP_REGISTER_TABLE(HP3_GOV_LATAM)
