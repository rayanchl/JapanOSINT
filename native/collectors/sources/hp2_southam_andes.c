/* collectors/sources/hp2_southam_andes.c — Spanish-speaking South America.
 *
 * The critical values here are the tax-ID registries (AFIP, SUNAT, SRI, SII —
 * which say whether a counterparty legally exists and is in good standing), the
 * procurement systems that publish awards *and* the suppliers barred from
 * bidding, and the judicial portals. Peru's OSCE debarment list and Argentina's
 * padrón lookup in particular answer questions no company register can. */
#include "../../lib/hpengine.h"

static const hp_source HP2_ANDES[] = {
  /* ── Argentina ─────────────────────────────────────────────────────────── */
  { .id = "AR_AFIP_PADRON", .name = "Argentina AFIP — taxpayer padrón (CUIT)",
    .name_ja = "アルゼンチン 納税者登録(CUIT)", .category = "economy",
    .portal = "https://www.afip.gob.ar", .record_type = "ar-taxpayer",
    .tags = "\"ar\",\"tax\",\"identity\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://soa.afip.gob.ar/sr-padron/v2/persona/{qd}",
    .title_keys = "data.nombre,data.razonSocial", .id_keys = "data.idPersona",
    .date_keys = "data.fechaInscripcion",
    .description = "The AFIP taxpayer record behind an Argentine CUIT — legal "
      "name, type of person, fiscal address, activities and registration status. "
      "Keyless, authoritative, and the first check on any Argentine counterparty" },

  { .id = "AR_COMPRAR_TENDERS", .name = "Argentina COMPR.AR — national procurement",
    .name_ja = "アルゼンチン 公共調達", .category = "government",
    .portal = "https://comprar.gob.ar", .record_type = "ar-tender",
    .tags = "\"ar\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://comprar.gob.ar/BuscarAvanzado.aspx?qs={q}",
    .base = "https://comprar.gob.ar", .filter_query = 1,
    .description = "Argentine national procurement processes and awards — the "
      "contracting unit, the item, the award value and the supplier CUIT, which "
      "joins straight back to the AFIP padrón" },

  { .id = "AR_BORA_GAZETTE", .name = "Argentina — Boletín Oficial",
    .name_ja = "アルゼンチン 官報", .category = "government",
    .portal = "https://www.boletinoficial.gob.ar", .record_type = "ar-gazette",
    .tags = "\"ar\",\"gazette\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.boletinoficial.gob.ar/busquedaAvanzada/primera?descripcion={q}",
    .base = "https://www.boletinoficial.gob.ar", .filter_query = 1,
    .description = "Argentina's official gazette — company constitutions and "
      "changes, appointments, sanctions and judicial notices; where corporate "
      "acts are legally published before any register reflects them" },

  { .id = "AR_DATOS_GOB", .name = "Argentina datos.gob.ar — dataset catalogue",
    .name_ja = "アルゼンチン 政府データ", .category = "government",
    .portal = "https://datos.gob.ar", .record_type = "ar-dataset",
    .tags = "\"ar\",\"opendata\"", .free_tier = 1,
    .url = "https://datos.gob.ar/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title", .id_keys = "name",
    .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "Argentine open-data catalogue — beneficiary lists, subsidy "
      "registers, licensee inventories and contract datasets published by each "
      "ministry, with direct resource URLs" },

  /* ── Chile ─────────────────────────────────────────────────────────────── */
  { .id = "CL_CMF_ENTITIES", .name = "Chile CMF — supervised financial entities",
    .name_ja = "チリ 金融市場委員会 登録機関", .category = "economy",
    .portal = "https://www.cmfchile.cl", .record_type = "cl-licensee",
    .tags = "\"cl\",\"finance\",\"licences\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.cmfchile.cl/institucional/mercados/entidad.php?buscar={q}",
    .base = "https://www.cmfchile.cl", .filter_query = 1,
    .description = "Chilean financial market commission registers — banks, "
      "insurers, issuers and their filings, plus CMF's sanction resolutions "
      "naming the entity and the individuals responsible" },

  { .id = "CL_PJUD_CASES", .name = "Chile Poder Judicial — case search",
    .name_ja = "チリ 司法 事件検索", .category = "legal",
    .portal = "https://oficinajudicialvirtual.pjud.cl", .record_type = "cl-court-case",
    .tags = "\"cl\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://oficinajudicialvirtual.pjud.cl/indexN.php?buscar={q}",
    .base = "https://oficinajudicialvirtual.pjud.cl", .filter_query = 1,
    .description = "Chilean judiciary case lookups by party — civil, labour and "
      "criminal proceedings; Chile's virtual judicial office is the public front "
      "end to the national case system" },

  { .id = "CL_DATOS_CKAN", .name = "Chile datos.gob.cl — dataset catalogue",
    .name_ja = "チリ 政府データ", .category = "government",
    .portal = "https://datos.gob.cl", .record_type = "cl-dataset",
    .tags = "\"cl\",\"opendata\"", .free_tier = 1,
    .url = "https://datos.gob.cl/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title", .id_keys = "name",
    .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "Chilean open-data catalogue — sanitary licences, fishing and "
      "aquaculture concessions, mining property and transfer registers, each with "
      "downloadable resources" },

  /* ── Colombia ──────────────────────────────────────────────────────────── */
  { .id = "CO_RUES_COMPANY", .name = "Colombia RUES — unified business register",
    .name_ja = "コロンビア 企業登記(RUES)", .category = "government",
    .portal = "https://www.rues.org.co", .record_type = "co-company",
    .tags = "\"co\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.rues.org.co/RM?razonSocial={q}",
    .base = "https://www.rues.org.co", .filter_query = 1,
    .description = "Colombia's unified commercial register — NIT, chamber of "
      "commerce, legal representative, capital and status across all chambers in "
      "one search" },

  { .id = "CO_SECOP_SUPPLIERS", .name = "Colombia — contractors & sanctions datasets",
    .name_ja = "コロンビア 契約者/制裁データ", .category = "government",
    .portal = "https://www.datos.gov.co", .record_type = "co-contractor",
    .tags = "\"co\",\"procurement\",\"debarment\"", .free_tier = 1,
    .url = "https://www.datos.gov.co/api/views/metadata/v1?q={q}&limit=100",
    .filter_query = 1, .title_keys = "name,description", .id_keys = "id",
    .page_param = "offset", .page_size = 100, .page_start = 0,
    .description = "Colombian open-data catalogue entries for contractor, "
      "disciplinary-sanction and fiscal-liability registers — the datasets that "
      "say whether a person may hold public contracts at all" },

  { .id = "CO_PROCURADURIA_ANTECEDENTES", .name = "Colombia — disciplinary record certificate",
    .name_ja = "コロンビア 懲戒記録証明", .category = "government",
    .portal = "https://www.procuraduria.gov.co", .record_type = "co-disciplinary",
    .tags = "\"co\",\"enforcement\",\"pep\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.procuraduria.gov.co/portal/index.jsp?buscar={q}",
    .base = "https://www.procuraduria.gov.co", .filter_query = 1,
    .description = "Colombian Procuraduría disciplinary antecedents — whether a "
      "person is barred from public office or contracting. Required for public "
      "employment, and therefore a hard, checkable status" },

  /* ── Peru ──────────────────────────────────────────────────────────────── */
  { .id = "PE_SUNAT_RUC", .name = "Peru SUNAT — RUC taxpayer record",
    .name_ja = "ペルー 納税者登録(RUC)", .category = "economy",
    .portal = "https://e-consultaruc.sunat.gob.pe", .record_type = "pe-taxpayer",
    .tags = "\"pe\",\"tax\",\"identity\"", .want = HP_NUMERIC,
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://e-consultaruc.sunat.gob.pe/cl-ti-itmrconsruc/FrameCriterioBusquedaWeb.jsp?nroRuc={qd}",
    .base = "https://e-consultaruc.sunat.gob.pe", .filter_query = 0,
    .description = "Peruvian RUC record — legal name, trade name, fiscal "
      "address, activity, and the condition/status pair that says whether the "
      "taxpayer is 'habido' (locatable) or has disappeared" },

  { .id = "PE_OSCE_DEBARRED", .name = "Peru OSCE — debarred suppliers",
    .name_ja = "ペルー 契約禁止業者(OSCE)", .category = "government",
    .portal = "https://www.gob.pe/osce", .record_type = "pe-debarment",
    .tags = "\"pe\",\"debarment\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://apps.osce.gob.pe/perfilprov-ui/inhabilitados?razonSocial={q}",
    .base = "https://apps.osce.gob.pe", .filter_query = 1,
    .description = "Peruvian suppliers barred from contracting with the state — "
      "the entity, the resolution, the period and the ground. The single most "
      "decisive fact about a Peruvian state contractor" },

  { .id = "PE_SEACE_TENDERS", .name = "Peru SEACE — procurement processes",
    .name_ja = "ペルー 公共調達(SEACE)", .category = "government",
    .portal = "https://prod2.seace.gob.pe", .record_type = "pe-tender",
    .tags = "\"pe\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://prod2.seace.gob.pe/seacebus-uiwd-pub/buscadorPublico/buscadorPublico.xhtml?keyword={q}",
    .base = "https://prod2.seace.gob.pe", .filter_query = 1,
    .description = "Peruvian procurement processes — entity, object, referential "
      "value, bidders and the awarded supplier; joins to the RUC record and the "
      "debarment list" },

  { .id = "PE_DATOS_ABIERTOS", .name = "Peru — open data catalogue",
    .name_ja = "ペルー 政府データ", .category = "government",
    .portal = "https://www.datosabiertos.gob.pe", .record_type = "pe-dataset",
    .tags = "\"pe\",\"opendata\"", .free_tier = 1,
    .url = "https://www.datosabiertos.gob.pe/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title", .id_keys = "name",
    .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "Peruvian open-data catalogue — mining concessions, forestry "
      "titles, health licences and beneficiary registers, with the resources "
      "behind each dataset" },

  /* ── Ecuador / Bolivia / Paraguay / Uruguay / Venezuela ────────────────── */
  { .id = "EC_SRI_RUC", .name = "Ecuador SRI — RUC taxpayer record",
    .name_ja = "エクアドル 納税者登録(RUC)", .category = "economy",
    .portal = "https://srienlinea.sri.gob.ec", .record_type = "ec-taxpayer",
    .tags = "\"ec\",\"tax\",\"identity\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://srienlinea.sri.gob.ec/sri-catastro-sujeto-servicio-internet/rest/ConsolidadoContribuyente/obtenerPorNumerosRuc?&ruc={qd}",
    .title_keys = "razonSocial,nombreComercial", .id_keys = "numeroRuc",
    .date_keys = "fechaInicioActividades",
    .description = "Ecuadorian RUC consolidated record — legal name, taxpayer "
      "class, activity start/cessation dates and current status, straight from "
      "the SRI service with no key" },

  { .id = "EC_SUPERCIAS_COMPANY", .name = "Ecuador Supercias — company & shareholder data",
    .name_ja = "エクアドル 会社監督庁", .category = "government",
    .portal = "https://appscvsmovil.supercias.gob.ec", .record_type = "ec-company",
    .tags = "\"ec\",\"registry\",\"ownership\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://appscvsmovil.supercias.gob.ec/portalInformacion/consulta_cia_menu.zul?nombre={q}",
    .base = "https://appscvsmovil.supercias.gob.ec", .filter_query = 1,
    .description = "Ecuador's companies supervisor — legal representatives, "
      "shareholders and financial statements. Ecuador publishes shareholder "
      "lists, which most of the region does not" },

  { .id = "BO_SICOES_TENDERS", .name = "Bolivia SICOES — public procurement",
    .name_ja = "ボリビア 公共調達(SICOES)", .category = "government",
    .portal = "https://www.sicoes.gob.bo", .record_type = "bo-tender",
    .tags = "\"bo\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.sicoes.gob.bo/portal/contrataciones/busqueda/convocatorias.php?buscar={q}",
    .base = "https://www.sicoes.gob.bo", .filter_query = 1,
    .description = "Bolivian state contracting system — call notices, awards and "
      "the contracted supplier, plus the register of suppliers barred from "
      "participating" },

  { .id = "PY_DNCP_TENDERS", .name = "Paraguay DNCP — procurement (OCDS)",
    .name_ja = "パラグアイ 公共調達(DNCP)", .category = "government",
    .portal = "https://www.contrataciones.gov.py", .record_type = "py-tender",
    .tags = "\"py\",\"procurement\",\"ocds\"", .free_tier = 1,
    .url = "https://www.contrataciones.gov.py/datos/api/v3/doc/search/processes?query={q}",
    .array_path = "results", .title_keys = "nombre_licitacion,titulo",
    .id_keys = "id_llamado", .date_keys = "fecha_publicacion_convocatoria",
    .page_param = "page", .page_start = 1,
    .description = "Paraguayan procurement in OCDS form — the process, the "
      "contracting entity, awards and suppliers. Paraguay's DNCP publishes a "
      "genuinely complete open contracting API" },

  { .id = "PY_DNCP_SUPPLIERS", .name = "Paraguay DNCP — supplier & sanction record",
    .name_ja = "パラグアイ 供給者/制裁記録", .category = "government",
    .portal = "https://www.contrataciones.gov.py", .record_type = "py-supplier",
    .tags = "\"py\",\"procurement\",\"debarment\"", .free_tier = 1,
    .url = "https://www.contrataciones.gov.py/datos/api/v3/doc/search/proveedores?query={q}",
    .array_path = "results", .title_keys = "razon_social,nombre_fantasia",
    .id_keys = "ruc", .page_param = "page", .page_start = 1,
    .description = "Paraguayan state suppliers by name or RUC — registration "
      "status, contract history and any disqualification; the supplier side of "
      "the same OCDS dataset" },

  { .id = "UY_COMPRAS_ESTATALES", .name = "Uruguay — state purchases",
    .name_ja = "ウルグアイ 公共調達", .category = "government",
    .portal = "https://www.gub.uy/agencia-compras-contrataciones-estado",
    .record_type = "uy-tender", .tags = "\"uy\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.comprasestatales.gub.uy/consultas/buscar?texto={q}",
    .base = "https://www.comprasestatales.gub.uy", .filter_query = 1,
    .description = "Uruguayan state purchase calls and awards — the buying "
      "agency, the item, the award and the supplier RUT; Uruguay also publishes "
      "its supplier register through the same portal" },

  { .id = "UY_DGI_RUT", .name = "Uruguay DGI — taxpayer (RUT) status",
    .name_ja = "ウルグアイ 納税者登録(RUT)", .category = "economy",
    .portal = "https://www.dgi.gub.uy", .record_type = "uy-taxpayer",
    .tags = "\"uy\",\"tax\",\"identity\"", .want = HP_NUMERIC,
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://servicios.dgi.gub.uy/serviciosenlinea/dgi--servicios-en-linea--consulta-de-certificado-unico?rut={qd}",
    .base = "https://servicios.dgi.gub.uy", .filter_query = 0,
    .description = "Uruguayan taxpayer certificate status behind a RUT — whether "
      "the entity holds a valid single certificate, which it needs to contract "
      "with the state or import" },

  { .id = "VE_REGISTRY_SEARCH", .name = "Venezuela — RIF & mercantile register search",
    .name_ja = "ベネズエラ 納税者/商業登記", .category = "government",
    .portal = "http://contribuyente.seniat.gob.ve", .record_type = "ve-taxpayer",
    .tags = "\"ve\",\"tax\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "http://contribuyente.seniat.gob.ve/BuscaRif/BuscaRif.jsp?p_rif={qn}",
    .base = "http://contribuyente.seniat.gob.ve", .filter_query = 0,
    .description = "Venezuelan SENIAT taxpayer lookup by RIF — registered name "
      "and status. Frequently unreachable from outside the country, which is "
      "reported as an error rather than as an absent record" },

  /* ── Regional ──────────────────────────────────────────────────────────── */
  { .id = "LATAM_IDB_SANCTIONS", .name = "IDB — sanctioned firms & individuals",
    .name_ja = "米州開発銀行 制裁リスト", .category = "government",
    .portal = "https://www.iadb.org", .record_type = "debarment",
    .tags = "\"latam\",\"debarment\",\"mdb\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.iadb.org/en/transparency/sanctioned-firms-and-individuals?search={q}",
    .base = "https://www.iadb.org", .filter_query = 1,
    .description = "Inter-American Development Bank sanctions — firms and people "
      "barred from IDB-financed contracts. Cross-debarment means an IDB listing "
      "usually bars the party at the World Bank too" },

  { .id = "LATAM_CAF_PROJECTS", .name = "CAF — Andean development bank projects",
    .name_ja = "CAF アンデス開発銀行 事業", .category = "government",
    .portal = "https://www.caf.com", .record_type = "latam-project",
    .tags = "\"latam\",\"development\",\"finance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.caf.com/es/buscador/?q={q}",
    .base = "https://www.caf.com", .filter_query = 1,
    .description = "Development Bank of Latin America project approvals and "
      "procurement — the financier behind much regional infrastructure, and the "
      "counterparty list for the contractors building it" },

  { .id = "LATAM_MERCOSUR_MEASURES", .name = "Mercosur — trade measures & resolutions",
    .name_ja = "メルコスール 貿易措置", .category = "government",
    .portal = "https://www.mercosur.int", .record_type = "latam-trade-measure",
    .tags = "\"latam\",\"trade\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.mercosur.int/?s={q}",
    .base = "https://www.mercosur.int", .filter_query = 1,
    .description = "Mercosur resolutions and trade measures naming products, "
      "sectors or firms — the regional layer above Brazilian and Argentine "
      "national trade rules" },
};

HP_REGISTER_TABLE(HP2_ANDES)
