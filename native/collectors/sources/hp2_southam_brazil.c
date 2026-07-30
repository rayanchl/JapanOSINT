/* collectors/sources/hp2_southam_brazil.c — Brazil critical-value records.
 *
 * Brazil publishes more consequential data than almost any other country, and
 * the consequential parts are not the company register: they are the blacklists.
 * The "lista suja" of employers caught using slave labour, IBAMA's environmental
 * embargoes, CEIS/CNEP/CEPIM debarment, the CEAF register of dismissed public
 * servants, and the electoral court's candidate and donation records. All of it
 * is public, keyed by CNPJ/CPF, and mostly free. */
#include "../../lib/hpengine.h"

#define BR_KEY  "BR_TRANSPARENCIA_KEY"
#define BR_HDR  { "chave-api-dados: {key}", NULL }
#define BR_BASE "https://api.portaldatransparencia.gov.br/api-de-dados"

static const hp_source HP2_BRAZIL[] = {
  /* ── Debarment & conduct blacklists ────────────────────────────────────── */
  { .id = "BR_CNEP_SANCTIONS", .name = "Brazil CNEP — anti-corruption sanctions",
    .name_ja = "ブラジル 腐敗防止制裁(CNEP)", .category = "government",
    .portal = "https://portaldatransparencia.gov.br", .record_type = "br-sanction",
    .tags = "\"br\",\"sanctions\",\"corruption\"", .key_env = BR_KEY,
    .headers = BR_HDR, .free_tier = 1,
    .url = BR_BASE "/cnep?nomeSancionado={q}&pagina=1",
    .page_param = "pagina", .page_start = 1,
    .title_keys = "pessoa.nome,sancionado.nome", .id_keys = "id",
    .date_keys = "dataInicioSancao",
    .description = "Companies sanctioned under Brazil's Clean Company Act — the "
      "leniency agreements and administrative penalties that follow a corruption "
      "finding, with the sanctioning body and the period" },

  { .id = "BR_CEPIM_NGO_BAN", .name = "Brazil CEPIM — barred nonprofit partners",
    .name_ja = "ブラジル NPO契約禁止リスト(CEPIM)", .category = "government",
    .portal = "https://portaldatransparencia.gov.br", .record_type = "br-sanction",
    .tags = "\"br\",\"nonprofit\",\"debarment\"", .key_env = BR_KEY,
    .headers = BR_HDR, .free_tier = 1,
    .url = BR_BASE "/cepim?nomeEntidade={q}&pagina=1",
    .page_param = "pagina", .page_start = 1,
    .title_keys = "pessoaJuridica.nome,entidade.nome", .id_keys = "id",
    .description = "Nonprofits barred from receiving Brazilian federal transfers "
      "— the entity, the impeding body and the reason. NGO fronts for public "
      "money show up here before anywhere else" },

  { .id = "BR_CEAF_DISMISSALS", .name = "Brazil CEAF — dismissed public servants",
    .name_ja = "ブラジル 公務員懲戒免職記録(CEAF)", .category = "government",
    .portal = "https://portaldatransparencia.gov.br", .record_type = "br-dismissal",
    .tags = "\"br\",\"pep\",\"enforcement\"", .key_env = BR_KEY,
    .headers = BR_HDR, .free_tier = 1,
    .url = BR_BASE "/ceaf?nome={q}&pagina=1",
    .page_param = "pagina", .page_start = 1,
    .title_keys = "pessoa.nome,nome", .id_keys = "id",
    .date_keys = "dataPublicacao",
    .description = "Public servants dismissed for cause in Brazil — the name, "
      "the agency, the legal ground and the publication date. The individual-level "
      "counterpart to the corporate debarment lists" },

  { .id = "BR_SLAVE_LABOUR_LIST", .name = "Brazil — slave-labour employers list (lista suja)",
    .name_ja = "ブラジル 強制労働企業リスト", .category = "government",
    .portal = "https://www.gov.br/trabalho-e-emprego", .record_type = "br-labour-violation",
    .tags = "\"br\",\"labour\",\"blacklist\",\"human-rights\"", .free_tier = 1,
    .type = "scraped", .mode = HP_HTML,
    .url = "https://www.gov.br/trabalho-e-emprego/pt-br/@@busca?SearchableText={q}",
    .base = "https://www.gov.br", .filter_query = 1,
    .description = "Brazil's register of employers caught using labour analogous "
      "to slavery — the single most consequential supply-chain finding available "
      "about a Brazilian producer, and grounds for exclusion by most buyers" },

  { .id = "BR_IBAMA_EMBARGOES", .name = "Brazil IBAMA — environmental embargoes",
    .name_ja = "ブラジル 環境違反差止(IBAMA)", .category = "government",
    .portal = "https://www.ibama.gov.br", .record_type = "br-environmental-sanction",
    .tags = "\"br\",\"environment\",\"enforcement\",\"deforestation\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://servicos.ibama.gov.br/ctf/publico/areasembargadas/ConsultaPublicaAreasEmbargadas.php?nome={q}",
    .base = "https://servicos.ibama.gov.br", .filter_query = 1,
    .description = "IBAMA embargoed areas and environmental fines — the property, "
      "the responsible party and the infraction. The evidence base for "
      "deforestation-linked commodity due diligence" },

  /* ── Money: contracts, transfers, benefits ─────────────────────────────── */
  { .id = "BR_FEDERAL_CONTRACTS", .name = "Brazil — federal contracts by supplier",
    .name_ja = "ブラジル 連邦契約(供給者別)", .category = "government",
    .portal = "https://portaldatransparencia.gov.br", .record_type = "br-contract",
    .tags = "\"br\",\"procurement\",\"contracts\"", .key_env = BR_KEY,
    .headers = BR_HDR, .free_tier = 1,
    .url = BR_BASE "/contratos?cnpjContratada={qd}&pagina=1",
    .page_param = "pagina", .page_start = 1,
    .title_keys = "objeto,fornecedor.nome", .id_keys = "id",
    .date_keys = "dataAssinatura",
    .description = "Federal contracts held by a Brazilian CNPJ — object, value, "
      "signing and end dates and the contracting organ; the direct answer to "
      "what a company is paid by the federal government" },

  { .id = "BR_COMPRASNET_TENDERS", .name = "Brazil Comprasnet — bids & awards",
    .name_ja = "ブラジル 入札(Comprasnet)", .category = "government",
    .portal = "https://compras.dados.gov.br", .record_type = "br-tender",
    .tags = "\"br\",\"procurement\"", .free_tier = 1,
    .url = "https://compras.dados.gov.br/licitacoes/v1/licitacoes.json?nome={q}&offset=0",
    .array_path = "_embedded.licitacoes", .title_keys = "objeto,nome",
    .id_keys = "identificador", .date_keys = "data_abertura",
    .page_param = "offset", .page_size = 500, .page_start = 0,
    .description = "Brazilian federal bidding processes — modality, object, "
      "estimated and awarded values, and the participating suppliers; the stage "
      "before the contract record" },

  { .id = "BR_SIAFI_PAYMENTS", .name = "Brazil — federal payments to a supplier",
    .name_ja = "ブラジル 連邦支払記録", .category = "government",
    .portal = "https://portaldatransparencia.gov.br", .record_type = "br-payment",
    .tags = "\"br\",\"spending\"", .key_env = BR_KEY, .headers = BR_HDR,
    .free_tier = 1,
    .url = BR_BASE "/despesas/documentos-por-favorecido?codigoPessoa={qd}&pagina=1",
    .page_param = "pagina", .page_start = 1,
    .title_keys = "favorecido.nome,documento", .id_keys = "documento",
    .date_keys = "data",
    .description = "Actual federal disbursements to a CNPJ/CPF — document number, "
      "phase (commitment, settlement, payment) and amount. Contracts say what was "
      "promised; this says what was paid" },

  { .id = "BR_SERVIDORES", .name = "Brazil — federal public servants register",
    .name_ja = "ブラジル 連邦公務員名簿", .category = "government",
    .portal = "https://portaldatransparencia.gov.br", .record_type = "br-public-servant",
    .tags = "\"br\",\"pep\",\"employment\"", .key_env = BR_KEY, .headers = BR_HDR,
    .free_tier = 1,
    .url = BR_BASE "/servidores?nome={q}&pagina=1",
    .page_param = "pagina", .page_start = 1,
    .title_keys = "servidor.pessoa.nome,nome", .id_keys = "id",
    .description = "Named federal public servants with their organ, post, "
      "function and remuneration band — Brazil publishes the individual, which "
      "makes conflict-of-interest checks against supplier owners possible" },

  /* ── Corporate, markets & IP ───────────────────────────────────────────── */
  { .id = "BR_CVM_FILINGS", .name = "Brazil CVM — securities filings & registrants",
    .name_ja = "ブラジル 証券委員会 開示", .category = "economy",
    .portal = "https://dados.cvm.gov.br", .record_type = "br-filing",
    .tags = "\"br\",\"filings\",\"finance\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://dados.cvm.gov.br/dataset?q={q}",
    .base = "https://dados.cvm.gov.br", .filter_query = 1,
    .description = "Brazilian securities commission open data — registered "
      "issuers, funds and intermediaries, their periodic filings and CVM's "
      "sanctioning decisions" },

  { .id = "BR_INPI_TRADEMARKS", .name = "Brazil INPI — trademarks & patents",
    .name_ja = "ブラジル 商標/特許(INPI)", .category = "research",
    .portal = "https://www.gov.br/inpi", .record_type = "br-trademark",
    .tags = "\"br\",\"trademarks\",\"patents\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://busca.inpi.gov.br/pePI/servlet/MarcasServletController?Action=searchMarca&marca={q}",
    .base = "https://busca.inpi.gov.br", .filter_query = 1,
    .description = "Brazilian trademark and patent filings by holder — brands a "
      "group owns under names that do not match its corporate registration" },

  { .id = "BR_JUCESP_COMPANY", .name = "Brazil — state commercial registries (juntas)",
    .name_ja = "ブラジル 州商業登記", .category = "government",
    .portal = "https://www.gov.br/empresas-e-negocios", .record_type = "br-company",
    .tags = "\"br\",\"registry\",\"officers\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.gov.br/empresas-e-negocios/pt-br/@@busca?SearchableText={q}",
    .base = "https://www.gov.br", .filter_query = 1,
    .description = "State-level commercial registry (junta comercial) surface — "
      "the articles of association and amendments that name partners and capital "
      "changes, which the federal CNPJ record summarises but does not evidence" },

  /* ── Politics, elections, justice ──────────────────────────────────────── */
  { .id = "BR_TSE_CANDIDATES", .name = "Brazil TSE — candidates & asset declarations",
    .name_ja = "ブラジル 選挙 候補者/資産申告", .category = "government",
    .portal = "https://divulgacandcontas.tse.jus.br", .record_type = "br-candidate",
    .tags = "\"br\",\"elections\",\"pep\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://divulgacandcontas.tse.jus.br/divulga/#/candidato?nome={q}",
    .base = "https://divulgacandcontas.tse.jus.br", .filter_query = 1,
    .description = "Brazilian election candidates with their declared assets, "
      "party, and campaign finance — the asset declaration is a legally required "
      "public statement, which makes unexplained wealth checkable" },

  { .id = "BR_TSE_DONATIONS", .name = "Brazil TSE — campaign donations",
    .name_ja = "ブラジル 選挙献金", .category = "government",
    .portal = "https://divulgacandcontas.tse.jus.br", .record_type = "br-donation",
    .tags = "\"br\",\"political-finance\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://divulgacandcontas.tse.jus.br/divulga/#/prestador?nome={q}",
    .base = "https://divulgacandcontas.tse.jus.br", .filter_query = 1,
    .description = "Donations and campaign spending reported to Brazil's "
      "electoral court — donor CPF/CNPJ, amount and recipient; the link between "
      "a state supplier and the politicians who award its contracts" },

  { .id = "BR_JUSBRASIL_CASES", .name = "Brazil — court cases by party",
    .name_ja = "ブラジル 訴訟検索", .category = "legal",
    .portal = "https://www.jusbrasil.com.br", .record_type = "br-court-case",
    .tags = "\"br\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.jusbrasil.com.br/busca?q={q}",
    .base = "https://www.jusbrasil.com.br", .filter_query = 1,
    .description = "Brazilian court proceedings naming a party across federal, "
      "state and labour courts — labour claims in particular are a reliable early "
      "indicator of a company in trouble" },

  { .id = "BR_CADE_ANTITRUST", .name = "Brazil CADE — antitrust cases",
    .name_ja = "ブラジル 競争当局(CADE)", .category = "government",
    .portal = "https://www.gov.br/cade", .record_type = "br-antitrust",
    .tags = "\"br\",\"antitrust\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://sei.cade.gov.br/sei/institucional/pesquisa/documento_consulta_externa.php?txtPesquisa={q}",
    .base = "https://sei.cade.gov.br", .filter_query = 1,
    .description = "Brazilian competition authority proceedings — cartel "
      "investigations, leniency agreements and merger reviews naming the parties; "
      "cartel findings often precede the corruption cases" },

  /* ── Sector registers ──────────────────────────────────────────────────── */
  { .id = "BR_ANVISA_REGISTRATIONS", .name = "Brazil ANVISA — health product registrations",
    .name_ja = "ブラジル 医薬品/医療機器登録(ANVISA)", .category = "health",
    .portal = "https://consultas.anvisa.gov.br", .record_type = "br-health-registration",
    .tags = "\"br\",\"health\",\"licences\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://consultas.anvisa.gov.br/#/medicamentos/?nomeProduto={q}",
    .base = "https://consultas.anvisa.gov.br", .filter_query = 1,
    .description = "Brazilian health-surveillance registrations and their holders "
      "— medicines, devices and food products, plus ANVISA's interdictions and "
      "recalls" },

  { .id = "BR_ANEEL_GENERATION", .name = "Brazil ANEEL — power generation licences",
    .name_ja = "ブラジル 発電事業許可(ANEEL)", .category = "infrastructure",
    .portal = "https://dadosabertos.aneel.gov.br", .record_type = "br-power-plant",
    .tags = "\"br\",\"energy\",\"critical-infrastructure\"", .free_tier = 1,
    .url = "https://dadosabertos.aneel.gov.br/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title", .id_keys = "name",
    .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "Brazilian electricity regulator open data — generation plants "
      "with owner, capacity and coordinates, transmission concessions and "
      "distributor performance; the critical-infrastructure inventory" },

  { .id = "BR_ANP_FUEL_LICENCES", .name = "Brazil ANP — oil, gas & fuel authorisations",
    .name_ja = "ブラジル 石油・ガス認可(ANP)", .category = "industry",
    .portal = "https://www.gov.br/anp", .record_type = "br-energy-licence",
    .tags = "\"br\",\"energy\",\"licences\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.gov.br/anp/pt-br/@@busca?SearchableText={q}",
    .base = "https://www.gov.br", .filter_query = 1,
    .description = "Authorisations to produce, import, distribute or resell fuels "
      "in Brazil, and ANP's penalties — the licence layer over a sector with "
      "persistent fuel-fraud and money-laundering exposure" },

  { .id = "BR_MINING_CADASTRE", .name = "Brazil ANM — mining rights cadastre",
    .name_ja = "ブラジル 鉱業権登録(ANM)", .category = "industry",
    .portal = "https://sistemas.anm.gov.br", .record_type = "br-mining-right",
    .tags = "\"br\",\"mining\",\"licences\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://sistemas.anm.gov.br/SCM/site/admin/pesquisaProcesso.aspx?nome={q}",
    .base = "https://sistemas.anm.gov.br", .filter_query = 1,
    .description = "Brazilian mining process records — the title holder, the "
      "substance, the municipality and the stage; illegal-gold work starts by "
      "checking whether a claimed title exists at all" },

  { .id = "BR_IBAMA_CTF", .name = "Brazil IBAMA — federal environmental register",
    .name_ja = "ブラジル 環境登録(CTF)", .category = "government",
    .portal = "https://servicos.ibama.gov.br", .record_type = "br-environmental-registration",
    .tags = "\"br\",\"environment\",\"licences\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://servicos.ibama.gov.br/ctf/publico/certificadoRegularidade/ConsultaCertificadoRegularidade.php?nome={q}",
    .base = "https://servicos.ibama.gov.br", .filter_query = 1,
    .description = "Whether an operator holds a valid federal environmental "
      "regularity certificate — required to transport timber, wildlife and "
      "chemicals, so its absence is itself the finding" },

  { .id = "BR_SISCOMEX_TRADE", .name = "Brazil — foreign trade statistics by firm sector",
    .name_ja = "ブラジル 貿易統計", .category = "economy",
    .portal = "https://comexstat.mdic.gov.br", .record_type = "br-trade-flow",
    .tags = "\"br\",\"trade\",\"customs\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://comexstat.mdic.gov.br/pt/geral?q={q}",
    .base = "https://comexstat.mdic.gov.br", .filter_query = 1,
    .description = "Brazilian import/export statistics by product, country and "
      "municipality — the trade-flow context around an exporter, including the "
      "municipality-level detail that identifies producing regions" },
};

HP_REGISTER_TABLE(HP2_BRAZIL)
