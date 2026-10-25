/* collectors/sources/hp2_northam_ca_mx.c — Canada & Mexico critical values.
 *
 * Canada runs one of the best open-government stacks in the world and almost
 * none of it is wired into OSINT tooling: a federated business-registry search
 * across every province, the autonomous sanctions list as a plain CSV, the
 * procurement ineligibility list, and proactive contract disclosure down to
 * the vendor line item.
 *
 * Mexico's highest-value record is not a company register at all — it is the
 * SAT 69-B list of firms that issued invoices for simulated operations. Being
 * on that list is the single strongest public indicator that a Mexican
 * counterparty is an invoicing shell. */
#include "lib/hpengine.h"

static const hp_source HP2_NORTHAM_CA_MX[] = {
  /* ── Canada ────────────────────────────────────────────────────────────── */
  { .id = "CA_BUSINESS_REGISTRIES", .name = "Canada — federated business registries search",
    .name_ja = "カナダ 企業登記 横断検索", .category = "government",
    .portal = "https://beta.canadasbusinessregistries.ca", .record_type = "ca-entity",
    .tags = "\"ca\",\"registry\"", .free_tier = 1,
    .url = "https://searchapi.mrasservice.ca/Search/api/v1/search?"
      "fq=keyword:%7B{q}%7D&lang=en&queryaction=fieldquery&sortfield=score&sortorder=desc",
    .array_path = "docs", .title_keys = "Company_Name,MRAS_Jurisdiction",
    .id_keys = "Juri_ID,Company_Number", .date_keys = "Date_Established",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "One query across every Canadian federal and provincial "
      "corporate register — jurisdiction, registry number, status and "
      "establishment date. Canada has no single company register, so this "
      "federation is the only way to see an entity's whole national footprint" },

  { .id = "CA_SEMA_SANCTIONS_CSV", .name = "Canada — consolidated autonomous sanctions (SEMA, structured)",
    .name_ja = "カナダ 独自制裁リスト", .category = "government",
    .portal = "https://www.international.gc.ca", .record_type = "ca-sanction",
    .tags = "\"ca\",\"sanctions\"", .mode = HP_CSV, .free_tier = 1,
    .url = "https://www.international.gc.ca/world-monde/assets/office_docs/"
      "international_relations-relations_internationales/sanctions/sema-lmes.csv",
    .filter_query = 1,
    .title_keys = "Entity,LastName,Item",
    .id_keys = "Item", .date_keys = "DateOfBirth",
    .description = "Canada's consolidated Special Economic Measures Act listing "
      "as raw structured rows — every listed person and entity with aliases, "
      "date of birth, the schedule and the country regulation that listed them. "
      "Canada lists names the EU and US do not, so this is not a duplicate" },

  { .id = "CA_LOBBYING_REGISTRY", .name = "Canada — commissioner of lobbying registry",
    .name_ja = "カナダ ロビー活動登録", .category = "government",
    .portal = "https://lobbycanada.gc.ca", .record_type = "ca-lobbying",
    .tags = "\"ca\",\"lobbying\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://lobbycanada.gc.ca/en/search/?search={q}",
    .base = "https://lobbycanada.gc.ca", .filter_query = 1,
    .description = "Canadian lobbying registrations and monthly communication "
      "reports — the client, the in-house or consultant lobbyist, the subject "
      "matter and the named designated public office holder contacted" },

  { .id = "CA_SIMA_TRADE_MEASURES", .name = "Canada CBSA — anti-dumping & countervailing measures",
    .name_ja = "カナダ 反ダンピング措置", .category = "government",
    .portal = "https://www.cbsa-asfc.gc.ca", .record_type = "ca-trade-measure",
    .tags = "\"ca\",\"trade\",\"customs\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.cbsa-asfc.gc.ca/sima-lmsi/menu-eng.html?q={q}",
    .base = "https://www.cbsa-asfc.gc.ca", .filter_query = 1,
    .description = "Special Import Measures Act findings — the named exporters "
      "subject to duty, their company-specific rates and the goods covered. "
      "Exporter-specific duty rates identify producers by name where trade "
      "statistics only show a country" },

  { .id = "CA_RECALLS_SAFETY", .name = "Canada — recalls & safety alerts",
    .name_ja = "カナダ リコール/安全警告", .category = "safety",
    .portal = "https://recalls-rappels.canada.ca", .record_type = "ca-recall",
    .tags = "\"ca\",\"recall\",\"safety\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://recalls-rappels.canada.ca/en/search/site?search_api_fulltext={q}",
    .base = "https://recalls-rappels.canada.ca", .filter_query = 1,
    .description = "Health Canada, CFIA and Transport Canada recalls in one "
      "feed — food, medical devices, vehicles, consumer products and cannabis, "
      "with the affected lot numbers and the responsible firm" },

  { .id = "CA_DRUG_PRODUCT_DB", .name = "Health Canada — drug product database",
    .name_ja = "カナダ 医薬品データベース", .category = "government",
    .portal = "https://health-products.canada.ca", .record_type = "ca-drug-product",
    .tags = "\"ca\",\"pharma\",\"licence\"", .free_tier = 1,
    .url = "https://health-products.canada.ca/api/drug/drugproduct/?lang=en&company_name={q}",
    .title_keys = "brand_name,company_name", .id_keys = "drug_code",
    .date_keys = "last_update_date",
    .description = "Every drug product authorised in Canada for a company — "
      "DIN, brand and generic name, class, schedule and marketing status. "
      "Structured records straight from Health Canada's own API" },

  { .id = "CA_CONTRACT_DISCLOSURE", .name = "Canada — proactive contract disclosure search",
    .name_ja = "カナダ 政府契約 個別開示", .category = "government",
    .portal = "https://search.open.canada.ca", .record_type = "ca-contract",
    .tags = "\"ca\",\"procurement\",\"contracts\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://search.open.canada.ca/contracts/?search_text={q}",
    .base = "https://search.open.canada.ca", .filter_query = 1,
    .description = "Individual Canadian federal contracts over CAD 10,000 — "
      "vendor name, department, value, amendment history, whether it was "
      "competed and the procurement instrument used. Contract-level records, "
      "not the dataset catalogue that indexes them" },

  { .id = "CA_QC_REGISTRAIRE", .name = "Quebec — registraire des entreprises",
    .name_ja = "ケベック州 企業登記", .category = "government",
    .portal = "https://www.registreentreprises.gouv.qc.ca", .record_type = "ca-qc-entity",
    .tags = "\"ca\",\"quebec\",\"registry\",\"ownership\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.registreentreprises.gouv.qc.ca/RQAnonymeGR/GR/GR03/"
      "GR03A2_19A_PIU_RechEnt_PC/PageRechSimple.aspx?T1.CodeService=S00436&nom={q}",
    .base = "https://www.registreentreprises.gouv.qc.ca", .filter_query = 1,
    .description = "Quebec's enterprise register — the NEQ number, directors, "
      "shareholders holding the majority of votes and the declared number of "
      "employees. Quebec discloses more ownership detail than any other "
      "Canadian province" },

  { .id = "CA_ON_BUSINESS_REGISTRY", .name = "Ontario — business registry search",
    .name_ja = "オンタリオ州 企業登記", .category = "government",
    .portal = "https://www.appmybizaccount.gov.on.ca", .record_type = "ca-on-entity",
    .tags = "\"ca\",\"ontario\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.appmybizaccount.gov.on.ca/onbis/businessnames/"
      "public/search?name={q}",
    .base = "https://www.appmybizaccount.gov.on.ca", .filter_query = 1,
    .description = "Ontario corporations and registered business names — BIN, "
      "status, registered office and the officers on file. Ontario holds the "
      "largest share of Canadian corporate activity" },

  /* ── Mexico ────────────────────────────────────────────────────────────── */
  { .id = "MX_SAT_EFOS_69B", .name = "Mexico SAT — article 69-B simulated-operations list",
    .name_ja = "メキシコ SAT 69-B 架空取引リスト", .category = "government",
    .portal = "http://omawww.sat.gob.mx", .record_type = "mx-efos",
    .tags = "\"mx\",\"tax\",\"blacklist\",\"fraud\"",
    .mode = HP_CSV, .free_tier = 1,
    .url = "http://omawww.sat.gob.mx/cifras_sat/Documents/Listado_Completo_69-B.csv",
    .filter_query = 1,
    .title_keys = "Nombre del Contribuyente,col1",
    .id_keys = "RFC,col0",
    .description = "Mexico's list of taxpayers that issued invoices covering "
      "operations that never happened — RFC, name, the stage of the procedure "
      "(presumed, definitive, cleared or overturned) and the publication dates. "
      "A definitive 69-B listing invalidates every invoice the firm ever "
      "issued, which is why this is the single most consequential Mexican "
      "record for counterparty risk" },

  { .id = "MX_SAT_INCUMPLIDOS", .name = "Mexico SAT — non-compliant taxpayers (69)",
    .name_ja = "メキシコ SAT 不履行納税者リスト", .category = "government",
    .portal = "http://omawww.sat.gob.mx", .record_type = "mx-tax-noncompliance",
    .tags = "\"mx\",\"tax\",\"blacklist\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "http://omawww.sat.gob.mx/cifras_sat/Paginas/datos/vinculo.html?page=ListCompleta69.html&q={q}",
    .base = "http://omawww.sat.gob.mx", .filter_query = 1,
    .description = "The article 69 register of taxpayers with firm tax debts, "
      "cancelled or forgiven liabilities, and those judged non-locatable — the "
      "companion list to 69-B and the one that shows whether a firm has simply "
      "vanished from its registered address" },

  { .id = "MX_SIGER_RPC", .name = "Mexico SIGER — public register of commerce",
    .name_ja = "メキシコ 商業登記", .category = "government",
    .portal = "https://rpc.economia.gob.mx", .record_type = "mx-entity",
    .tags = "\"mx\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://rpc.economia.gob.mx/siger2/xhtml/consulta/consulta.jsf?q={q}",
    .base = "https://rpc.economia.gob.mx", .filter_query = 1,
    .description = "Mexican commercial register entries — folio mercantil, "
      "incorporation instrument, the notary who executed it, capital and the "
      "powers granted to legal representatives" },

  { .id = "MX_IMPI_MARCAS", .name = "Mexico IMPI — trademark & patent register",
    .name_ja = "メキシコ 商標/特許登録", .category = "government",
    .portal = "https://acervomarcas.impi.gob.mx", .record_type = "mx-trademark",
    .tags = "\"mx\",\"ip\",\"trademark\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://acervomarcas.impi.gob.mx:8181/marcanet/vistas/common/"
      "datos/bsqDenominacionCompleta.pgi?denominacion={q}",
    .base = "https://acervomarcas.impi.gob.mx:8181", .filter_query = 1,
    .description = "Mexican trademark and patent filings — holder, agent, "
      "class, status and the assignment history that reveals corporate "
      "restructurings inside a Mexican group" },

  { .id = "MX_DOF_GAZETTE", .name = "Mexico — Diario Oficial de la Federación",
    .name_ja = "メキシコ 官報", .category = "government",
    .portal = "https://www.dof.gob.mx", .record_type = "mx-gazette",
    .tags = "\"mx\",\"gazette\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.dof.gob.mx/busqueda_detalle.php?busqueda={q}",
    .base = "https://www.dof.gob.mx", .filter_query = 1,
    .description = "Mexico's official gazette — concessions, permits, "
      "expropriations, tariff decrees and the anti-dumping resolutions that "
      "name specific foreign exporters. The legal record of every federal act "
      "that touches a named company" },

  { .id = "MX_PJF_SENTENCIAS", .name = "Mexico — federal judiciary rulings search",
    .name_ja = "メキシコ 連邦司法 判決検索", .category = "government",
    .portal = "https://www.dof.gob.mx", .record_type = "mx-judgment",
    .tags = "\"mx\",\"court\",\"judgment\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://sjf2.scjn.gob.mx/busqueda-principal-tesis?q={q}",
    .base = "https://sjf2.scjn.gob.mx", .filter_query = 1,
    .description = "Mexican federal court rulings and published theses — "
      "amparo proceedings, tax litigation and the constitutional challenges "
      "companies bring against regulators, with the parties named" },

  { .id = "MX_DATOS_ABIERTOS", .name = "Mexico datos.gob.mx — open data catalogue",
    .name_ja = "メキシコ オープンデータ", .category = "government",
    .portal = "https://datos.gob.mx", .record_type = "mx-dataset",
    .tags = "\"mx\",\"opendata\"", .free_tier = 1,
    .url = "https://datos.gob.mx/busca/api/3/action/package_search?q={q}&rows=100&sort=id%20asc",
    .array_path = "result.results", .title_keys = "title,name",
    .id_keys = "id", .date_keys = "metadata_modified",
    .link_keys = "name", .link_tmpl = "https://datos.gob.mx/busca/dataset/{v}",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "Mexico's federal open-data catalogue — the beneficiary "
      "lists, subsidy payments, permit registers and inspection records that "
      "individual agencies publish and nobody indexes, with every matching "
      "dataset and its resource files" },
};

HP_REGISTER_TABLE(HP2_NORTHAM_CA_MX)
