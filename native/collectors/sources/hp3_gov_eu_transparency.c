/* collectors/sources/hp3_gov_eu_transparency.c — EU institutional transparency.
 *
 * `hp2_eu_institutions.c` covers the EU as a *regulator* — the registers,
 * sanctions and enforcement decisions that bind a firm. This file covers the EU
 * as an *institution*: who met whom, who sits on the expert group that drafted
 * the text, which document the Council refused to release, which project the
 * cohesion funds paid for, and which company's VAT and EORI numbers are live.
 *
 * The influence layer is the point. The Commission publishes high-level
 * meetings, expert-group membership, comitology committees and feedback on
 * initiatives, and each of those names a specific organisation against a
 * specific file. That is a lobbying record the Transparency Register alone does
 * not give you. */
#include "../../lib/hpengine.h"

static const hp_source HP3_GOV_EU_TRANSPARENCY[] = {
  /* ── The influence layer ──────────────────────────────────────────────── */
  { .id = "EU_EXPERT_GROUPS_REGISTER", .name = "EU — Commission expert groups register",
    .name_ja = "EU 専門家グループ登録", .category = "government",
    .portal = "https://ec.europa.eu", .record_type = "eu-expert-group",
    .tags = "\"eu\",\"influence\",\"advisory\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ec.europa.eu/transparency/expert-groups-register/screen/"
      "expert-groups?keywords={q}&lang=en",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "The formal advisory bodies that shape Commission proposals "
      "before they are published — the group's lead department, its mandate, "
      "and the individual companies, industry associations and academics "
      "appointed to it. Being in the room is measurable here" },

  { .id = "EU_COMITOLOGY_REGISTER", .name = "EU — comitology register",
    .name_ja = "EU コミトロジー登録簿", .category = "government",
    .portal = "https://ec.europa.eu", .record_type = "eu-comitology",
    .tags = "\"eu\",\"regulation\",\"process\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ec.europa.eu/transparency/comitology-register/screen/search"
      "?fulltext={q}&lang=en",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "Implementing and delegated acts as they pass through member "
      "state committees — the draft text, the committee, the vote, and the "
      "minutes. Most of the EU's technical rulemaking happens here, out of "
      "sight of the Parliament" },

  { .id = "EU_COMMISSION_MEETINGS", .name = "EU — Commission high-level lobby meetings",
    .name_ja = "EU 委員会幹部面談記録", .category = "government",
    .portal = "https://ec.europa.eu", .record_type = "eu-lobby-meeting",
    .tags = "\"eu\",\"lobbying\",\"influence\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ec.europa.eu/transparencyinitiative/meetings/meeting.do"
      "?host=&d-6679426-p=1&searchString={q}",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "Meetings between Commissioners, their cabinets and "
      "directors-general and registered interest representatives — date, "
      "portfolio, the organisation met and the subject discussed" },

  { .id = "EU_HAVE_YOUR_SAY_FEEDBACK", .name = "EU — Have Your Say initiative feedback",
    .name_ja = "EU 政策意見募集", .category = "government",
    .portal = "https://ec.europa.eu", .record_type = "eu-consultation-feedback",
    .tags = "\"eu\",\"consultation\",\"influence\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://ec.europa.eu/info/law/better-regulation/have-your-say/"
      "initiatives?text={q}",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "Feedback submitted on Commission initiatives, signed by the "
      "submitting organisation with its Transparency Register number. A "
      "company's own written position on a regulation it is trying to change" },

  { .id = "EU_DOCUMENTS_REGISTER", .name = "EU — Commission register of documents",
    .name_ja = "EU 委員会文書登録簿", .category = "government",
    .portal = "https://ec.europa.eu", .record_type = "eu-document",
    .tags = "\"eu\",\"documents\",\"foi\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ec.europa.eu/transparency/documents-register/search"
      "?fulltext={q}&lang=en",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "The register of Commission documents — the index that makes "
      "an access-to-documents request possible, because it names the document "
      "that exists even when the text itself is withheld" },

  { .id = "EU_COUNCIL_PUBLIC_REGISTER", .name = "EU Council — public register of documents",
    .name_ja = "EU理事会 公開文書登録簿", .category = "government",
    .portal = "https://www.consilium.europa.eu", .record_type = "eu-council-document",
    .tags = "\"eu\",\"council\",\"documents\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.consilium.europa.eu/en/documents-publications/"
      "public-register/public-register-search/?SearchTerm={q}",
    .base = "https://www.consilium.europa.eu", .filter_query = 1,
    .description = "Council working-party documents, member state position "
      "papers and negotiating mandates — where national governments' actual "
      "positions on a file are recorded, as opposed to their public statements" },

  { .id = "EU_INTEGRITY_WATCH", .name = "EU Integrity Watch — MEP side jobs & meetings",
    .name_ja = "EU 議員兼職/面談監視", .category = "government",
    .portal = "https://www.integritywatch.eu", .record_type = "eu-conflict-of-interest",
    .tags = "\"eu\",\"conflict-of-interest\",\"lobbying\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.integritywatch.eu/mepsincomes?search={q}",
    .base = "https://www.integritywatch.eu", .filter_query = 1,
    .description = "MEPs' declared outside income and the lobby meetings held "
      "by rapporteurs, assembled from the Parliament's own declarations. The "
      "conflict-of-interest view the Parliament publishes only as scanned PDFs" },

  /* ── Parliament as data ───────────────────────────────────────────────── */
  { .id = "EU_EP_OPENDATA_DOCUMENTS", .name = "European Parliament — open data documents",
    .name_ja = "欧州議会 オープンデータ文書", .category = "government",
    .portal = "https://data.europarl.europa.eu", .record_type = "eu-parliament-document",
    .tags = "\"eu\",\"parliament\",\"legislation\"", .free_tier = 1,
    .url = "https://data.europarl.europa.eu/api/v2/documents?format=application%2Fld%2Bjson"
      "&limit=100",
    .array_path = "data", .filter_query = 1,
    .title_keys = "title,work_type", .id_keys = "identifier",
    .date_keys = "activity_date",
    .page_param = "offset", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Parliament's own linked-data API — reports, resolutions, "
      "amendments and questions with their document identifiers, so a file can "
      "be followed from committee draft to plenary text" },

  { .id = "EU_EP_CORPORATE_BODIES", .name = "European Parliament — committees & delegations",
    .name_ja = "欧州議会 委員会/代表団", .category = "government",
    .portal = "https://data.europarl.europa.eu", .record_type = "eu-parliament-body",
    .tags = "\"eu\",\"parliament\"", .free_tier = 1,
    .url = "https://data.europarl.europa.eu/api/v2/corporate-bodies"
      "?format=application%2Fld%2Bjson&limit=200",
    .array_path = "data", .filter_query = 1,
    .title_keys = "label,classification", .id_keys = "identifier",
    .page_param = "offset", .page_size = 200, .page_start = 0, .page_max = 20,
    .description = "Every Parliament committee, subcommittee, political group "
      "and interparliamentary delegation, with the identifiers that join a body "
      "to its members and its documents" },

  { .id = "EU_EP_MEETINGS", .name = "European Parliament — meetings & agendas",
    .name_ja = "欧州議会 会議日程", .category = "government",
    .portal = "https://data.europarl.europa.eu", .record_type = "eu-parliament-meeting",
    .tags = "\"eu\",\"parliament\",\"schedule\"", .free_tier = 1,
    .url = "https://data.europarl.europa.eu/api/v2/meetings"
      "?format=application%2Fld%2Bjson&limit=100",
    .array_path = "data", .filter_query = 1,
    .title_keys = "activity_label,had_activity_type", .id_keys = "identifier",
    .date_keys = "activity_date",
    .page_param = "offset", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Plenary and committee sittings with their agendas and the "
      "activities scheduled — the calendar layer that says when a file will "
      "actually be voted" },

  /* ── Where the money goes ─────────────────────────────────────────────── */
  { .id = "EU_COHESION_DATA_CATALOG", .name = "EU Cohesion — open data catalog",
    .name_ja = "EU 結束政策データ", .category = "government",
    .portal = "https://cohesiondata.ec.europa.eu", .record_type = "eu-cohesion-dataset",
    .tags = "\"eu\",\"funding\",\"open-data\"", .free_tier = 1,
    .url = "https://cohesiondata.ec.europa.eu/api/catalog/v1?q={q}&limit=100",
    .array_path = "results", .title_keys = "resource.name,resource.description",
    .id_keys = "resource.id", .date_keys = "resource.updatedAt",
    .link_keys = "permalink",
    .page_param = "offset", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "The Cohesion Open Data Platform's catalog — the "
      "beneficiary lists, payment series and programme allocations behind the "
      "EU's largest spending instrument, per member state and fund" },

  { .id = "EU_KOHESIO_PROJECTS", .name = "Kohesio — EU-funded project beneficiaries",
    .name_ja = "EU 助成事業受益者", .category = "government",
    .portal = "https://kohesio.ec.europa.eu", .record_type = "eu-funded-project",
    .tags = "\"eu\",\"funding\",\"beneficiary\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://kohesio.ec.europa.eu/en/projects/?keywords={q}",
    .base = "https://kohesio.ec.europa.eu", .filter_query = 1,
    .description = "Named beneficiaries of ERDF, Cohesion Fund and ESF+ money "
      "with the project, the EU co-financing amount, the total cost and the "
      "location. Roughly 1.5 million projects, keyed to identifiable companies "
      "and municipalities" },

  { .id = "EU_FUNDING_TENDERS_PORTAL", .name = "EU Funding & Tenders portal — calls and awards",
    .name_ja = "EU 資金・入札ポータル", .category = "government",
    .portal = "https://ec.europa.eu", .record_type = "eu-funding-call",
    .tags = "\"eu\",\"funding\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ec.europa.eu/info/funding-tenders/opportunities/portal/screen/"
      "opportunities/topic-search?keywords={q}",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "Horizon, Digital Europe, EDF and CEF calls with their topic "
      "identifiers, budgets and deadlines — and, for closed calls, the "
      "consortia selected" },

  { .id = "EU_OLAF_INVESTIGATION_REPORTS", .name = "EU OLAF — anti-fraud investigation reporting",
    .name_ja = "EU 不正対策局 調査報告", .category = "government",
    .portal = "https://anti-fraud.ec.europa.eu", .record_type = "eu-fraud-case",
    .tags = "\"eu\",\"fraud\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://anti-fraud.ec.europa.eu/search_en?keys={q}",
    .base = "https://anti-fraud.ec.europa.eu", .filter_query = 1,
    .description = "The European Anti-Fraud Office's published case reporting "
      "and recommendations — customs fraud, misused structural funds and "
      "staff-integrity cases, with the recovery amounts recommended" },

  /* ── Identity and compliance checks ───────────────────────────────────── */
  { .id = "EU_VIES_VAT_VALIDATION", .name = "EU VIES — VAT number validation",
    .name_ja = "EU VAT番号照会", .category = "finance",
    .portal = "https://ec.europa.eu", .record_type = "eu-vat-registration",
    .tags = "\"eu\",\"vat\",\"identity\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://ec.europa.eu/taxation_customs/vies/rest-api/check-vat-number",
    .post_body = "{\"countryCode\":\"{Q}\",\"vatNumber\":\"{qd}\"}",
    .content_type = "application/json",
    .title_keys = "name,address", .id_keys = "vatNumber",
    .description = "The authoritative check of whether an intra-EU VAT number "
      "is valid and, where the member state permits it, the registered name "
      "and address behind it. A VAT number is often the only identifier on an "
      "invoice from a shell" },

  { .id = "EU_EUDAMED_DEVICES", .name = "EUDAMED — EU medical device database",
    .name_ja = "EU 医療機器データベース", .category = "health",
    .portal = "https://ec.europa.eu", .record_type = "eu-medical-device",
    .tags = "\"eu\",\"health\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ec.europa.eu/tools/eudamed/#/screen/search-device?"
      "submitted=true&deviceName={q}",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "Medical devices placed on the EU market — the manufacturer "
      "and its Single Registration Number, the authorised representative for "
      "non-EU makers, the notified body and the device risk class" },

  { .id = "EU_EASA_AIRWORTHINESS_DIRECTIVES", .name = "EASA — airworthiness directives & approvals",
    .name_ja = "EU航空安全機関 耐空性改善命令", .category = "transport",
    .portal = "https://ad.easa.europa.eu", .record_type = "eu-airworthiness-directive",
    .tags = "\"eu\",\"aviation\",\"safety\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ad.easa.europa.eu/ad/search?searchTerm={q}",
    .base = "https://ad.easa.europa.eu", .filter_query = 1,
    .description = "Mandatory airworthiness directives naming a manufacturer, "
      "type certificate or component — the defect found, the affected serial "
      "range and the compliance deadline" },

  { .id = "EU_ERA_RAIL_ERADIS", .name = "ERADIS — EU rail safety certificates & licences",
    .name_ja = "EU鉄道 安全証明データベース", .category = "transport",
    .portal = "https://eradis.era.europa.eu", .record_type = "eu-rail-certificate",
    .tags = "\"eu\",\"rail\",\"licence\",\"safety\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://eradis.era.europa.eu/safety_docs/scert/search.aspx?keyword={q}",
    .base = "https://eradis.era.europa.eu", .filter_query = 1,
    .description = "Railway undertakings' safety certificates, infrastructure "
      "managers' authorisations, entities in charge of maintenance and the "
      "national accident investigation reports — the EU rail sector's full "
      "licensing record in one database" },

  { .id = "EU_ECDC_SURVEILLANCE_ATLAS", .name = "ECDC — communicable disease surveillance",
    .name_ja = "EU疾病予防管理センター 感染症監視", .category = "health",
    .portal = "https://www.ecdc.europa.eu", .record_type = "eu-disease-surveillance",
    .tags = "\"eu\",\"health\",\"surveillance\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.ecdc.europa.eu/en/search?s={q}",
    .base = "https://www.ecdc.europa.eu", .filter_query = 1,
    .description = "The European Centre for Disease Prevention and Control's "
      "surveillance reports, rapid risk assessments and outbreak notifications "
      "by country and pathogen" },

  { .id = "EU_EMSA_MARITIME_SAFETY", .name = "EMSA — maritime safety & pollution reporting",
    .name_ja = "EU海事安全機関 報告", .category = "maritime",
    .portal = "https://www.emsa.europa.eu", .record_type = "eu-maritime-safety",
    .tags = "\"eu\",\"maritime\",\"safety\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.emsa.europa.eu/search.html?searchword={q}",
    .base = "https://www.emsa.europa.eu", .filter_query = 1,
    .description = "European Maritime Safety Agency reporting — port state "
      "control statistics, casualty analyses, pollution detections and the "
      "flag-state performance data behind the Paris MoU lists" },

  { .id = "EU_ACER_ENERGY_REMIT", .name = "ACER — energy market transparency & REMIT",
    .name_ja = "EU エネルギー規制協力機関", .category = "energy",
    .portal = "https://www.acer.europa.eu", .record_type = "eu-energy-market",
    .tags = "\"eu\",\"energy\",\"regulation\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.acer.europa.eu/search?search_api_fulltext={q}",
    .base = "https://www.acer.europa.eu", .filter_query = 1,
    .description = "The EU energy regulator's market monitoring — REMIT "
      "registered market participants, market-abuse decisions taken by national "
      "regulators and the cross-border capacity allocation record" },

  { .id = "EU_CHAFEA_HEALTH_PROGRAMME", .name = "EU HaDEA — health & digital programme beneficiaries",
    .name_ja = "EU 保健デジタル事業受益者", .category = "government",
    .portal = "https://hadea.ec.europa.eu", .record_type = "eu-funded-project",
    .tags = "\"eu\",\"funding\",\"health\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://hadea.ec.europa.eu/search_en?keys={q}",
    .base = "https://hadea.ec.europa.eu", .filter_query = 1,
    .description = "The executive agency running EU4Health, Digital Europe and "
      "part of Horizon — grant beneficiaries, consortium partners and the "
      "procurement it runs on the Commission's behalf" },

  { .id = "EU_EIOPA_PENSION_REGISTER", .name = "EIOPA — cross-border pension & insurance registers",
    .name_ja = "EU 年金/保険登録簿", .category = "finance",
    .portal = "https://www.eiopa.europa.eu", .record_type = "eu-pension-institution",
    .tags = "\"eu\",\"pensions\",\"licence\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.eiopa.europa.eu/search_en?keys={q}",
    .base = "https://www.eiopa.europa.eu", .filter_query = 1,
    .description = "Institutions for occupational retirement provision "
      "operating cross-border, the pan-European personal pension register and "
      "the insurance undertakings list — pension money is a shareholder class "
      "that rarely appears in a company's own filings" },

  { .id = "EU_ECB_SUPERVISED_ENTITIES", .name = "ECB Banking Supervision — supervised entities",
    .name_ja = "ECB 監督対象銀行", .category = "finance",
    .portal = "https://www.bankingsupervision.europa.eu",
    .record_type = "eu-supervised-bank",
    .tags = "\"eu\",\"banking\",\"supervision\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.bankingsupervision.europa.eu/search/html/index.en.html?"
      "searchTerm={q}",
    .base = "https://www.bankingsupervision.europa.eu", .filter_query = 1,
    .description = "The banks the ECB supervises directly, the significance "
      "criteria applied to each, the sanctions imposed and the SREP outcomes "
      "published — euro-area banking supervision at institution level" },

  { .id = "EU_EFTA_SURVEILLANCE_CASES", .name = "EFTA Surveillance Authority — cases & decisions",
    .name_ja = "EFTA監視機構 事件/決定", .category = "government",
    .portal = "https://www.eftasurv.int", .record_type = "eu-eea-case",
    .tags = "\"eea\",\"norway\",\"iceland\",\"enforcement\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.eftasurv.int/search?query={q}",
    .base = "https://www.eftasurv.int", .filter_query = 1,
    .description = "State aid, competition and internal-market decisions for "
      "Norway, Iceland and Liechtenstein — the EEA states that follow EU rules "
      "but are absent from every Commission database" },
};

HP_REGISTER_TABLE(HP3_GOV_EU_TRANSPARENCY)
