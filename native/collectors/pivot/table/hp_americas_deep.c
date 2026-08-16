/* collectors/sources/hp_americas_deep.c — Americas deep records.
 *
 * US federal data is unusually penetrable if you go past the search box: XBRL
 * company facts carry a filer's whole financial history as structured numbers,
 * USAspending links a recipient to every federal dollar, EPA ECHO carries
 * facility-level enforcement, the Senate LDA database carries who lobbies for
 * whom, and Brazil publishes the full partner list (QSA) behind every CNPJ.
 * These rows target exactly those records. */
#include "lib/hpengine.h"

#define SEC_UA { "User-Agent: JapanOSINT research contact@japanosint.local", NULL }

static const hp_source HP_AMERICAS[] = {
  /* ── US: SEC structured financials ─────────────────────────────────────── */
  { .id = "SEC_COMPANY_FACTS", .name = "SEC XBRL — company facts",
    .name_ja = "米国SEC XBRL — 企業財務ファクト", .category = "economy",
    .portal = "https://www.sec.gov/edgar", .record_type = "sec-xbrl-fact",
    .tags = "\"us\",\"filings\",\"finance\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://data.sec.gov/api/xbrl/companyfacts/CIK{qc}.json",
    .headers = SEC_UA, .title_keys = "entityName", .id_keys = "cik",
    .description = "Every XBRL fact an SEC filer has ever reported, keyed by CIK — "
      "revenue, assets, share counts, segment values with their filing dates. The "
      "numeric history behind the filings, not the filing index" },

  { .id = "SEC_COMPANY_CONCEPT", .name = "SEC XBRL — reported revenue series",
    .name_ja = "米国SEC XBRL — 売上高系列", .category = "economy",
    .portal = "https://www.sec.gov/edgar", .record_type = "sec-xbrl-concept",
    .tags = "\"us\",\"filings\",\"finance\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://data.sec.gov/api/xbrl/companyconcept/CIK{qc}/us-gaap/Revenues.json",
    .headers = SEC_UA, .title_keys = "entityName,label", .id_keys = "cik",
    .description = "The full reported-revenue time series for one CIK — every "
      "period, value, form and accession number, so a claimed revenue figure can "
      "be checked against what was actually filed" },

  /* ── US: federal money ─────────────────────────────────────────────────── */
  { .id = "USASPENDING_AWARDS", .name = "USAspending — federal awards by recipient",
    .name_ja = "米国連邦支出 — 受給者別契約", .category = "government",
    .portal = "https://www.usaspending.gov", .record_type = "us-federal-award",
    .tags = "\"us\",\"procurement\",\"spending\"", .free_tier = 1,
    .url = "https://api.usaspending.gov/api/v2/search/spending_by_award/",
    .post_body = "{\"filters\":{\"keywords\":[\"{Q}\"],\"award_type_codes\":"
                 "[\"A\",\"B\",\"C\",\"D\"]},\"fields\":[\"Award ID\",\"Recipient Name\","
                 "\"Awarding Agency\",\"Award Amount\",\"Start Date\",\"End Date\","
                 "\"Description\"],\"page\":1,\"limit\":40,\"sort\":\"Award Amount\","
                 "\"order\":\"desc\"}",
    .array_path = "results", .title_keys = "Recipient Name,Description",
    .id_keys = "Award ID", .date_keys = "Start Date",
    .description = "Federal contracts awarded to an entity — award id, awarding "
      "agency, obligated amount, period of performance and description. The direct "
      "answer to 'what does this company get paid by the US government'" },

  { .id = "USASPENDING_RECIPIENT_LOOKUP", .name = "USAspending — recipient resolution",
    .name_ja = "米国連邦支出 — 受給者名寄せ", .category = "government",
    .portal = "https://www.usaspending.gov", .record_type = "us-recipient",
    .tags = "\"us\",\"spending\"", .free_tier = 1,
    .url = "https://api.usaspending.gov/api/v2/autocomplete/recipient/",
    .post_body = "{\"search_text\":\"{Q}\",\"limit\":25}",
    .array_path = "results", .title_keys = "recipient_name",
    .id_keys = "recipient_id,duns,uei",
    .description = "Resolves a recipient name to USAspending's canonical recipient "
      "id, UEI and DUNS — the join key that turns a company name into its complete "
      "federal award history" },

  { .id = "SAM_GOV_ENTITY", .name = "SAM.gov — federal contractor registration",
    .name_ja = "SAM.gov — 連邦調達登録", .category = "government",
    .portal = "https://sam.gov", .record_type = "us-sam-entity",
    .tags = "\"us\",\"procurement\"", .key_env = "SAM_API_KEY", .free_tier = 1,
    .url = "https://api.sam.gov/entity-information/v3/entities?api_key={key}"
           "&legalBusinessName={q}&includeSections=entityRegistration,coreData&page=0",
    .array_path = "entityData", .title_keys = "entityRegistration.legalBusinessName",
    .id_keys = "entityRegistration.ueiSAM",
    .description = "SAM.gov registration behind a federal contractor — UEI, CAGE "
      "code, physical address, business types, registration status and expiry, plus "
      "any exclusion (debarment) records" },

  { .id = "FEC_CONTRIBUTIONS", .name = "FEC — individual campaign contributions",
    .name_ja = "米国連邦選挙委員会 — 個人献金", .category = "government",
    .portal = "https://www.fec.gov", .record_type = "us-contribution",
    .tags = "\"us\",\"political-finance\"", .key_env = "FEC_API_KEY", .free_tier = 1,
    .url = "https://api.open.fec.gov/v1/schedules/schedule_a/?api_key={key}"
           "&contributor_name={q}&per_page=40&sort=-contribution_receipt_date",
    .array_path = "results", .title_keys = "contributor_name,committee.name",
    .id_keys = "transaction_id", .date_keys = "contribution_receipt_date",
    .description = "Itemised US federal campaign contributions by a named donor — "
      "amount, date, recipient committee, and the donor's stated employer and "
      "occupation, which is often the cleanest employment record available" },

  { .id = "US_SENATE_LOBBYING", .name = "US Senate LDA — lobbying filings",
    .name_ja = "米国上院 — ロビイング届出", .category = "government",
    .portal = "https://lda.senate.gov", .record_type = "us-lobbying",
    .tags = "\"us\",\"lobbying\"", .free_tier = 1,
    .url = "https://lda.senate.gov/api/v1/filings/?client_name={q}&page_size=25",
    .array_path = "results", .title_keys = "client.name,registrant.name",
    .id_keys = "filing_uuid", .date_keys = "dt_posted",
    .next_path = "next",
    .description = "Lobbying disclosure filings naming a client — the registrant "
      "firm, income/expenses reported, the lobbyists named and the specific issues "
      "and bills lobbied on" },

  /* ── US: regulators & enforcement ──────────────────────────────────────── */
  { .id = "US_EPA_ECHO", .name = "EPA ECHO — facility compliance & enforcement",
    .name_ja = "米国EPA — 施設の法令順守/執行", .category = "government",
    .portal = "https://echo.epa.gov", .record_type = "us-epa-facility",
    .tags = "\"us\",\"environment\",\"enforcement\"", .free_tier = 1,
    .url = "https://echodata.epa.gov/echo/rest_services.get_facilities?output=JSON&p_fn={q}",
    .title_keys = "FacName,SourceID", .id_keys = "RegistryID",
    .description = "US environmental regulatory record for facilities operated by a "
      "company — permits held, inspection and violation counts, formal enforcement "
      "actions and penalties assessed" },

  { .id = "US_FDA_ENFORCEMENT", .name = "openFDA — recalls & enforcement reports",
    .name_ja = "米国FDA — リコール/執行", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-fda-enforcement",
    .tags = "\"us\",\"recalls\",\"enforcement\"", .free_tier = 1,
    .url = "https://api.fda.gov/drug/enforcement.json?search=recalling_firm:%22{q}%22&limit=40",
    .array_path = "results", .title_keys = "product_description,reason_for_recall",
    .id_keys = "recall_number", .date_keys = "recall_initiation_date",
    .description = "FDA recall and enforcement reports naming a firm — product, "
      "reason, classification, distribution pattern and quantity, with the recall "
      "number that keys the whole event" },

  { .id = "FCC_LICENSES", .name = "FCC — radio licence holdings",
    .name_ja = "米国FCC — 無線免許保有", .category = "telecom",
    .portal = "https://www.fcc.gov", .record_type = "us-fcc-licence",
    .tags = "\"us\",\"spectrum\"", .free_tier = 1,
    .url = "https://data.fcc.gov/api/license-view/basicSearch/getLicenses?searchValue={q}&format=json",
    .array_path = "Licenses.License", .title_keys = "licName,serviceDesc",
    .id_keys = "licenseID", .date_keys = "expiredDate",
    .description = "FCC licences held by an entity — call sign, radio service, "
      "status, grant and expiry dates; a licence list is a hard link between a "
      "corporate name and physical transmitting infrastructure" },

  { .id = "FAA_NNUMBER_REGISTRY", .name = "FAA — N-number aircraft registry",
    .name_ja = "米国FAA — 航空機登録(N番号)", .category = "transport",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://registry.faa.gov", .record_type = "us-aircraft",
    .tags = "\"us\",\"aviation\",\"ownership\"", .free_tier = 1,
    .url = "https://registry.faa.gov/AircraftInquiry/Search/NNumberResult?nNumberTxt={qn}",
    .base = "https://registry.faa.gov", .filter_query = 0,
    .description = "FAA aircraft registration for an N-number — registered owner, "
      "address, aircraft make/model/serial and certificate dates. Aircraft "
      "ownership is one of the few asset registries that is fully public" },

  { .id = "NPI_REGISTRY", .name = "NPPES — US healthcare provider registry",
    .name_ja = "米国 医療従事者登録(NPI)", .category = "health",
    .portal = "https://npiregistry.cms.hhs.gov", .record_type = "us-npi",
    .tags = "\"us\",\"health\",\"people\"", .free_tier = 1,
    .url = "https://npiregistry.cms.hhs.gov/api/?version=2.1&organization_name={q}&limit=40",
    .array_path = "results", .title_keys = "basic.organization_name,basic.name",
    .id_keys = "number",
    .description = "US healthcare providers and organisations — NPI number, "
      "taxonomy/specialty, licence numbers by state, practice and mailing "
      "addresses, and authorised official for organisational providers" },

  { .id = "PROPUBLICA_NONPROFIT_FILINGS", .name = "ProPublica — nonprofit 990 filing history",
    .name_ja = "米国 非営利団体990申告", .category = "economy",
    .portal = "https://projects.propublica.org/nonprofits/", .record_type = "us-nonprofit",
    .tags = "\"us\",\"nonprofit\",\"filings\"", .free_tier = 1,
    .url = "https://projects.propublica.org/nonprofits/api/v2/search.json?q={q}",
    .array_path = "organizations", .title_keys = "name", .id_keys = "ein",
    .detail_url = "https://projects.propublica.org/nonprofits/api/v2/organizations/{v}.json",
    .detail_key = "ein",
    .page_param = "page", .page_start = 0,
    .description = "US nonprofits by name, then the full IRS Form 990 history behind "
      "each EIN — revenue, expenses, assets and the filing PDFs listing officers and "
      "their compensation" },

  { .id = "NY_ACTIVE_CORPS", .name = "New York — active corporations register",
    .name_ja = "米ニューヨーク州 法人登記", .category = "government",
    .portal = "https://data.ny.gov", .record_type = "us-state-company",
    .tags = "\"us\",\"registry\",\"new-york\"", .free_tier = 1,
    .url = "https://data.ny.gov/resource/n9v6-gdp6.json?$q={q}&$limit=40",
    .title_keys = "current_entity_name,initial_dos_filing_date",
    .id_keys = "dos_id", .date_keys = "initial_dos_filing_date",
    .description = "New York State corporate register — entity name, DOS id, "
      "jurisdiction, filing date, county and the registered agent/service address, "
      "which is frequently the only US address a foreign owner provides" },

  { .id = "CA_SOS_BIZFILE", .name = "California SOS — business search",
    .name_ja = "米カリフォルニア州 法人検索", .category = "government",
    .portal = "https://bizfileonline.sos.ca.gov", .record_type = "us-state-company",
    .tags = "\"us\",\"registry\",\"california\"", .free_tier = 1,
    .url = "https://bizfileonline.sos.ca.gov/api/Records/businesssearch",
    .post_body = "{\"SEARCH_VALUE\":\"{Q}\",\"SEARCH_FILTER_TYPE_ID\":\"0\","
                 "\"SEARCH_TYPE_ID\":\"1\",\"FILING_TYPE_ID\":\"\",\"STATUS_ID\":\"\","
                 "\"FILING_DATE\":{\"start\":null,\"end\":null},\"CORPORATION_BANKRUPTCY_YN\":false,"
                 "\"OFFICER_OWNER_NAMES\":[],\"NUMBER_OF_FILINGS\":null}",
    .title_keys = "TITLE.0,TITLE", .id_keys = "ID",
    .description = "California business entities by name or agent — entity number, "
      "type, status, jurisdiction and filing date. California hosts a large share "
      "of US corporate shells, so its register is a routine pivot" },

  /* ── Canada ────────────────────────────────────────────────────────────── */
  { .id = "CA_MRAS_SEARCH", .name = "Canada Business Registries — federated search",
    .name_ja = "カナダ 企業登記 横断検索", .category = "government",
    .portal = "https://beta.canadasbusinessregistries.ca", .record_type = "ca-company",
    .tags = "\"ca\",\"registry\"", .free_tier = 1,
    .url = "https://searchapi.mrasservice.ca/Search/api/v1/search?fq=keyword:%7B{q}%7D"
           "&lang=en&queryaction=fieldquery&sortfield=score&sortorder=desc",
    .array_path = "docs", .title_keys = "Company_Name", .id_keys = "Juri_ID",
    .description = "Federated search across Canadian federal and provincial "
      "corporate registries — company name, jurisdiction, registry id and status "
      "in one query instead of thirteen separate portals" },

  /* ── Brazil ────────────────────────────────────────────────────────────── */
  { .id = "BR_MINHARECEITA_CNPJ", .name = "Minha Receita — full CNPJ record",
    .name_ja = "ブラジル CNPJ 全項目照会", .category = "government",
    .portal = "https://minhareceita.org", .record_type = "br-company",
    .tags = "\"br\",\"registry\",\"beneficial-ownership\"", .want = HP_NUMERIC,
    .free_tier = 1, .url = "https://minhareceita.org/{qd}",
    .title_keys = "razao_social,nome_fantasia", .id_keys = "cnpj",
    .date_keys = "data_inicio_atividade",
    .description = "The complete Brazilian Receita Federal record behind a CNPJ — "
      "legal and trade name, capital, CNAE activities, address, phones, and the QSA "
      "partner list naming every shareholder/administrator and their qualification" },

  { .id = "BR_BRASILAPI_CNPJ", .name = "BrasilAPI — CNPJ registration record",
    .name_ja = "ブラジル CNPJ 登記(BrasilAPI)", .category = "government",
    .portal = "https://brasilapi.com.br", .record_type = "br-company",
    .tags = "\"br\",\"registry\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://brasilapi.com.br/api/cnpj/v1/{qd}",
    .title_keys = "razao_social,nome_fantasia", .id_keys = "cnpj",
    .date_keys = "data_inicio_atividade",
    .description = "Second independent read of a Brazilian CNPJ — registration "
      "status and reason, size, legal nature, share capital and partners. Two "
      "sources for the same identifier is how you catch a stale record" },

  { .id = "BR_CEIS_SANCTIONS", .name = "Portal da Transparência — sanctioned companies (CEIS)",
    .name_ja = "ブラジル 制裁企業リスト(CEIS)", .category = "government",
    .portal = "https://portaldatransparencia.gov.br", .record_type = "br-sanction",
    .tags = "\"br\",\"sanctions\",\"debarment\"", .free_tier = 1,
    .key_env = "BR_TRANSPARENCIA_KEY",
    .url = "https://api.portaldatransparencia.gov.br/api-de-dados/ceis?nomeSancionado={q}&pagina=1",
    .headers = { "chave-api-dados: {key}", NULL },
    .title_keys = "sancionado.nome,pessoa.nome", .id_keys = "id",
    .date_keys = "dataInicioSancao",
    .page_param = "pagina", .page_start = 1,
    .description = "Brazilian register of companies barred from public contracting "
      "(CEIS) — sanctioned entity, sanctioning body, legal basis, and the start and "
      "end dates of the debarment" },

  /* ── Spanish-speaking Latin America ────────────────────────────────────── */
  { .id = "CO_SECOP_CONTRACTS", .name = "Colombia SECOP — public contracts",
    .name_ja = "コロンビア 公共契約(SECOP)", .category = "government",
    .portal = "https://www.datos.gov.co", .record_type = "co-contract",
    .tags = "\"co\",\"procurement\"", .free_tier = 1,
    .url = "https://www.datos.gov.co/resource/jbjy-vk9h.json?$q={q}&$limit=40",
    .title_keys = "descripcion_del_proceso,proveedor_adjudicado",
    .id_keys = "id_contrato", .date_keys = "fecha_de_firma",
    .description = "Colombian public contracts naming an entity — contracting "
      "entity, supplier, value, contract type and dates, from the open SECOP II "
      "dataset that covers the whole national procurement system" },

  { .id = "CL_MERCADO_PUBLICO", .name = "Chile Mercado Público — tender awards",
    .name_ja = "チリ 公共調達(Mercado Público)", .category = "government",
    .portal = "https://www.mercadopublico.cl", .record_type = "cl-tender",
    .tags = "\"cl\",\"procurement\"", .key_env = "CL_MERCADOPUBLICO_TICKET",
    .free_tier = 1,
    .url = "https://api.mercadopublico.cl/servicios/v1/publico/licitaciones.json"
           "?ticket={key}&estado=adjudicada",
    .array_path = "Listado", .filter_query = 1, .title_keys = "Nombre",
    .id_keys = "CodigoExterno", .date_keys = "FechaCierre",
    .description = "Awarded Chilean public tenders filtered to the queried entity — "
      "tender code, buying agency, title and closing date; the award records behind "
      "a Chilean supplier relationship" },

  { .id = "MX_DOF_NOTICES", .name = "Mexico DOF — official gazette search",
    .name_ja = "メキシコ 官報検索(DOF)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.dof.gob.mx", .record_type = "mx-gazette",
    .tags = "\"mx\",\"gazette\"", .free_tier = 1,
    .url = "https://www.dof.gob.mx/busqueda_detalle.php?BUSCAR_EN=TITULO&TEXTO_BUSCAR={q}",
    .base = "https://www.dof.gob.mx", .filter_query = 1,
    .description = "Mexican Diario Oficial de la Federación entries naming an "
      "entity — concessions, sanctions, appointments and decrees, which is where "
      "Mexican state action against or in favour of a company is published" },
};

HP_REGISTER_TABLE(HP_AMERICAS)
