/* collectors/pivot/table/hp3b31_intlbody.c — batch 31: intlbody — multilateral & treaty-body record.
 *
 * International organisations are the most under-queried public record there
 * is. They publish at record level — a World Bank project has a document set,
 * a contract award list and a supplier; an ICSID case names the investor, the
 * respondent state and the counsel; an IMO GISIS entry names the ship's
 * registered owner and its port state control detentions; a UN Panel of Experts
 * report names the companies moving the cargo — and almost none of it is
 * indexed against the company names it contains.
 *
 * These bodies also carry the debarment lists that bind across institutions:
 * a World Bank sanction is enforced by the AfDB, ADB, EBRD and IDB under the
 * cross-debarment agreement, so one lookup here disqualifies a counterparty
 * from five development banks at once.
 *
 * PROVENANCE AND VERIFICATION STATUS — read before trusting a row here.
 *
 * Hand-authored in C, not scaffolded by tools/gen_hp_batch.py. This file is
 * the maintained copy. Since 2026-10-05 a manifest RECONSTRUCTED from it exists
 * at docs/candidate-sources-batch31.<beat>.txt, so the manifest-driven gates can
 * finally be pointed at batch 31. It is a record, not a source: never
 * regenerate this file from it. Its fidelity was proven rather than assumed —
 * gen_hp_batch.py run on the manifest reproduces every field of every row here
 * exactly, and a grep of field assignments agrees on both sides.
 *
 * These rows are NOT proof-of-life verified. No row was fetched over the wire,
 * so rules 4, 4b and 4d (fetching is not emitting; emitting is not storing;
 * answering is not answering THE QUESTION) are unmeasured. What HAS been
 * checked is offline and structural: clean build, zero audit-sources findings,
 * no duplicate id or endpoint, every row reachable (rule 3), and every paged
 * endpoint declares its walk (audit_batch_pagination, first run 2026-10-05).
 *
 * With network, run the MANIFEST-driven tools:
 *     python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch31.*.txt --check-filter
 *     python3 tools/audit_batch_emit.py ../docs/candidate-sources-batch31.*.txt \
 *             --bin ./bin/japanosint --jobs 6
 * NOT `audit_registry_emit.py --match hp3b31`, which this header used to
 * suggest: --match is a regex on the source ID, these ids carry no batch
 * prefix, so it selects zero rows and reports nothing wrong.
 * Rows with key_env need their key in the environment, and rows with
 * post_body are POSTs that probe_hp_batch does not send — judge those two
 * classes with audit_batch_emit. Retire whatever comes back EMITS_NOTHING,
 * DROPS_EVERYTHING, COLLISION or FILTER_IGNORED. The engine cannot fabricate:
 * an endpoint that moved yields an honest empty, never an invented record.
 */
#include "lib/hpengine.h"

static const hp_source HP3B31_INTLBODY[] = {
  /* ── United Nations ───────────────────────────────────────────────────── */
  { .id = "UN_DIGITAL_LIBRARY", .name = "UN Digital Library — documents & resolutions",
    .name_ja = "国連デジタルライブラリ", .category = "government",
    .portal = "https://digitallibrary.un.org", .record_type = "un-document",
    .tags = "\"un\",\"documents\",\"multilateral\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://digitallibrary.un.org/search?p={q}&rg=100",
    .base = "https://digitallibrary.un.org", .filter_query = 1,
    .description = "Security Council and General Assembly resolutions, Panel of "
      "Experts reports, Secretary-General reports and voting records. Panel of "
      "Experts reports in particular name the companies, vessels and "
      "individuals behind sanctions evasion, in detail, with evidence" },

  { .id = "UN_COMTRADE_TRADE_FLOWS", .name = "UN Comtrade — official bilateral trade statistics",
    .name_ja = "国連 貿易統計", .category = "finance",
    .portal = "https://comtradeplus.un.org", .record_type = "un-trade-flow",
    .tags = "\"un\",\"trade\",\"statistics\"", .key_env = "COMTRADE_API_KEY",
    .free_tier = 1,
    .url = "https://comtradeapi.un.org/data/v1/get/C/A/HS?reporterCode={qd}"
      "&subscription-key={key}",
    .array_path = "data", .title_keys = "cmdDesc,partnerDesc",
    .id_keys = "period+reporterCode+partnerCode+cmdCode+flowCode", .date_keys = "period",
    /* id_keys composite (+ composes, , chooses): identity is the reporter x partner x commodity x flow x period tuple; `period`
     * alone collapsed a whole year of trade onto one uid. */
    .description = "Reported imports and exports by commodity and partner. "
      "Mirror-statistics gaps — where A reports exporting far more to B than B "
      "reports importing — are the standard method for locating "
      "trade-based value transfer and sanctions circumvention" },

  { .id = "ICAO_SAFETY_AUDITS", .name = "ICAO — state safety audit & aviation registry data",
    .name_ja = "ICAO 航空安全監査", .category = "transport",
    .portal = "https://www.icao.int", .record_type = "icao-safety-record",
    .tags = "\"un\",\"aviation\",\"safety\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.icao.int/search/pages/results.aspx?k={q}",
    .base = "https://www.icao.int", .filter_query = 1,
    .description = "ICAO's universal safety oversight audit results by state, "
      "the significant safety concerns raised, aircraft registration prefixes "
      "and the state letters that record regulatory failures" },

  /* ── Development banks: projects, contracts, debarment ────────────────── */
  { .id = "WORLDBANK_PROJECTS_API", .name = "World Bank — projects & operations API",
    .name_ja = "世界銀行 事業API", .category = "government",
    .portal = "https://projects.worldbank.org", .record_type = "mdb-project",
    .tags = "\"mdb\",\"funding\",\"development\"", .free_tier = 1,
    .url = "https://search.worldbank.org/api/v2/projects?qterm={q}&rows=100"
      "&format=json&fl=id,project_name,countryname,totalamt,boardapprovaldate,"
      "impagency,sector,theme,status,projectfinancialtype,url",
    .array_path = "projects", .title_keys = "project_name,countryname",
    .id_keys = "id", .date_keys = "boardapprovaldate",
    .page_param = "os", .page_zero_based = 1, .page_size = 100, .page_start = 0, .page_max = 40,
    .description = "World Bank lending operations — the implementing agency, "
      "the amount, the sectors and the status, requested with an explicit field "
      "list so the full project record is returned rather than the four-field "
      "summary the API defaults to" },

  { .id = "WORLDBANK_CONTRACT_AWARDS", .name = "World Bank — major contract awards",
    .name_ja = "世界銀行 主要契約落札", .category = "government",
    .portal = "https://finances.worldbank.org", .record_type = "mdb-contract",
    .tags = "\"mdb\",\"procurement\"", .free_tier = 1,
    .url = "https://finances.worldbank.org/api/catalog/v1?q={q}&domains=finances.worldbank.org&search_context=finances.worldbank.org&limit=100",
    .array_path = "results", .title_keys = "resource.name,resource.description",
    .id_keys = "resource.id", .date_keys = "resource.updatedAt",
    .link_keys = "permalink",
    .page_param = "offset", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "The Bank's finances catalog, which carries the major "
      "contract award dataset — the supplier, its country, the borrower "
      "country, the procurement method and the contract amount for every award "
      "above the disclosure threshold" },

  { .id = "IFC_PROJECT_DISCLOSURES", .name = "IFC — private sector investment disclosures",
    .name_ja = "IFC 投融資案件開示", .category = "finance",
    .portal = "https://disclosures.ifc.org", .record_type = "mdb-investment",
    .tags = "\"mdb\",\"investment\",\"private-sector\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://disclosures.ifc.org/results?q={q}",
    .base = "https://disclosures.ifc.org", .filter_query = 1,
    .description = "International Finance Corporation investments — the client "
      "company by name, the sponsor, the investment amount, the environmental "
      "and social category and the summary of investment information. A direct "
      "link between a private company and development finance" },

  { .id = "ISDB_PROJECT_DISCLOSURE", .name = "Islamic Development Bank — projects & procurement",
    .name_ja = "イスラム開発銀行 事業/調達", .category = "government",
    .portal = "https://www.isdb.org", .record_type = "mdb-project",
    .tags = "\"mdb\",\"islamic-finance\",\"funding\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.isdb.org/projects?search={q}",
    .base = "https://www.isdb.org", .filter_query = 1,
    .description = "IsDB financing across its 57 member countries — the "
      "executing agency, the financing mode (istisna'a, murabaha, leasing), "
      "the amount and the procurement notices. Development finance for a "
      "membership that overlaps only partly with the other banks' portfolios" },

  { .id = "IATI_AID_ACTIVITIES", .name = "IATI / d-portal — aid activity & transaction data",
    .name_ja = "国際援助透明性 事業データ", .category = "government",
    .portal = "https://d-portal.org", .record_type = "aid-activity",
    .tags = "\"aid\",\"development\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://d-portal.org/ctrack.html?search={q}",
    .base = "https://d-portal.org", .filter_query = 1,
    .description = "The International Aid Transparency Initiative's activity "
      "and transaction records — donor, implementing organisation, budget and "
      "each disbursement. Following the implementing partner chain is how aid "
      "money is traced to a local contractor" },

  /* ── Arbitration and international courts ─────────────────────────────── */
  { .id = "PCA_ARBITRATION_CASES", .name = "Permanent Court of Arbitration — cases",
    .name_ja = "常設仲裁裁判所 事件", .category = "legal",
    .portal = "https://pca-cpa.org", .record_type = "arbitration-case",
    .tags = "\"arbitration\",\"disputes\",\"treaty\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://pca-cpa.org/en/cases/?s={q}",
    .base = "https://pca-cpa.org", .filter_query = 1,
    .description = "PCA-administered inter-state, investor-state and "
      "contract-based arbitrations — the parties, the applicable rules, the "
      "tribunal and the procedural orders and awards released publicly" },

  { .id = "WTO_DISPUTE_SETTLEMENT", .name = "WTO — dispute settlement & trade measures",
    .name_ja = "WTO 紛争解決/貿易措置", .category = "government",
    .portal = "https://www.wto.org", .record_type = "wto-dispute",
    .tags = "\"trade\",\"disputes\",\"multilateral\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.wto.org/english/res_e/search_e.htm?q={q}",
    .base = "https://www.wto.org", .filter_query = 1,
    .description = "WTO disputes with the complainant, respondent, measure "
      "challenged and panel findings, plus the notified anti-dumping, "
      "countervailing and safeguard measures that name the exporters affected" },

  /* ── Standards, compliance and mutual evaluation ──────────────────────── */
  { .id = "APG_FATF_STYLE_BODIES", .name = "FATF-style regional bodies — regional AML evaluations",
    .name_ja = "FATF地域体 相互審査", .category = "government",
    .portal = "https://www.apgml.org", .record_type = "aml-evaluation",
    .tags = "\"aml\",\"compliance\",\"asia-pacific\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.apgml.org/search/default.aspx?q={q}",
    .base = "https://www.apgml.org", .filter_query = 1,
    .description = "The Asia/Pacific Group on Money Laundering and its sister "
      "regional bodies assess the countries the FATF plenary never reaches "
      "directly. Their mutual evaluation and follow-up reports carry the "
      "jurisdiction-level detail — beneficial ownership access, supervision "
      "coverage, prosecution counts — that the FATF summary omits" },

  { .id = "COE_GRECO_MONEYVAL", .name = "Council of Europe — GRECO & MONEYVAL evaluations",
    .name_ja = "欧州評議会 汚職/資金洗浄審査", .category = "government",
    .portal = "https://www.coe.int", .record_type = "compliance-evaluation",
    .tags = "\"europe\",\"anti-corruption\",\"aml\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.coe.int/en/web/portal/search?p_p_id=search&q={q}",
    .base = "https://www.coe.int", .filter_query = 1,
    .description = "GRECO's anti-corruption evaluations of judiciaries, "
      "parliaments and prosecution services, and MONEYVAL's AML assessments — "
      "the institution-level findings behind a country's corruption ranking" },

  { .id = "OECD_ANTIBRIBERY_REPORTS", .name = "OECD — anti-bribery & integrity monitoring",
    .name_ja = "OECD 贈賄防止監視", .category = "government",
    .portal = "https://www.oecd.org", .record_type = "compliance-evaluation",
    .tags = "\"oecd\",\"anti-corruption\",\"enforcement\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.oecd.org/en/search.html?q={q}",
    .base = "https://www.oecd.org", .filter_query = 1,
    .description = "Working Group on Bribery phase reports, which enumerate "
      "every foreign-bribery enforcement action each signatory has taken and "
      "the cases it has closed without charge — plus the national contact point "
      "complaints against named multinationals" },

  { .id = "IAEA_SAFEGUARDS_RECORD", .name = "IAEA — safeguards, incidents & facility reporting",
    .name_ja = "IAEA 保障措置/事象報告", .category = "government",
    .portal = "https://www.iaea.org", .record_type = "nuclear-record",
    .tags = "\"nuclear\",\"safeguards\",\"multilateral\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.iaea.org/search?search_api_fulltext={q}",
    .base = "https://www.iaea.org", .filter_query = 1,
    .description = "Safeguards implementation reports, board resolutions, "
      "incident and trafficking database summaries and the power reactor "
      "information system records for each named facility and operator" },

  { .id = "OPCW_DECLARATIONS", .name = "OPCW — chemical weapons convention record",
    .name_ja = "OPCW 化学兵器条約 記録", .category = "government",
    .portal = "https://www.opcw.org", .record_type = "chemical-record",
    .tags = "\"chemical\",\"nonproliferation\",\"multilateral\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.opcw.org/search?search_api_fulltext={q}",
    .base = "https://www.opcw.org", .filter_query = 1,
    .description = "Declared chemical industry facilities subject to "
      "inspection, verification reports, investigation of alleged use reports "
      "and the identification of attribution findings naming units and "
      "entities" },
};

HP_REGISTER_TABLE(HP3B31_INTLBODY)
