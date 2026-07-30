/* collectors/sources/hp2_northam_us_reg.c — US federal regulator & enforcement depth.
 *
 * The US publishes enforcement at record level, and enforcement is where the
 * critical values live: OSHA inspections with penalties, wage-theft findings,
 * FMCSA carrier safety ratings and crash counts, FDA inspection classifications
 * and device recalls, EPA toxic-release quantities, CMS open-payments transfers
 * from industry to named physicians. Each of these is an API, and each answers
 * a question a corporate registry cannot. */
#include "../../lib/hpengine.h"

static const hp_source HP2_US_REG[] = {
  /* ── Labour & workplace ────────────────────────────────────────────────── */
  { .id = "US_OSHA_INSPECTIONS", .name = "OSHA — workplace inspections & penalties",
    .name_ja = "米国OSHA — 労働安全検査/罰金", .category = "government",
    .portal = "https://enforcedata.dol.gov", .record_type = "us-osha-inspection",
    .tags = "\"us\",\"labour\",\"enforcement\"", .free_tier = 1,
    .url = "https://data.dol.gov/get/inspection/limit/100/filter_object/estab_name/filter_operator/ct/filter_value/{q}",
    .title_keys = "estab_name,site_city", .id_keys = "activity_nr",
    .date_keys = "open_date", .page_param = "offset", .page_size = 100, .page_start = 0,
    .description = "OSHA inspections of an employer — violation counts by "
      "severity, initial and current penalties, whether the inspection followed "
      "an accident or a complaint, and the establishment address" },

  { .id = "US_WHD_VIOLATIONS", .name = "US DOL Wage & Hour — back-wage findings",
    .name_ja = "米国労働省 — 未払賃金是正", .category = "government",
    .portal = "https://enforcedata.dol.gov", .record_type = "us-wage-violation",
    .tags = "\"us\",\"labour\",\"enforcement\"", .free_tier = 1,
    .url = "https://data.dol.gov/get/whd_whisard/limit/100/filter_object/trade_nm/filter_operator/ct/filter_value/{q}",
    .title_keys = "trade_nm,legal_name", .id_keys = "case_id",
    .date_keys = "findings_start_date", .page_param = "offset", .page_size = 100,
    .page_start = 0,
    .description = "Wage and Hour Division cases — back wages assessed and paid, "
      "employees affected, civil money penalties and the specific statute "
      "violated. Wage theft is a durable signal about how a firm operates" },

  { .id = "US_NLRB_CASES", .name = "NLRB — labour relations cases",
    .name_ja = "米国NLRB — 労使関係事件", .category = "government",
    .portal = "https://www.nlrb.gov", .record_type = "us-labour-case",
    .tags = "\"us\",\"labour\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.nlrb.gov/search/case?search_term={q}",
    .base = "https://www.nlrb.gov", .filter_query = 1,
    .description = "National Labor Relations Board cases naming an employer — "
      "unfair labour practice charges, election petitions, and the disposition; "
      "the union-side counterpart to OSHA's safety record" },

  /* ── Transport safety ──────────────────────────────────────────────────── */
  { .id = "US_FMCSA_CARRIER", .name = "FMCSA — motor carrier safety record",
    .name_ja = "米国FMCSA — 運送事業者安全記録", .category = "transport",
    .portal = "https://safer.fmcsa.dot.gov", .record_type = "us-carrier",
    .tags = "\"us\",\"transport\",\"safety\"", .key_env = "FMCSA_WEBKEY",
    .free_tier = 1,
    .url = "https://mobile.fmcsa.dot.gov/qc/services/carriers/name/{q}?webKey={key}",
    .array_path = "content", .title_keys = "carrier.legalName,carrier.dbaName",
    .id_keys = "carrier.dotNumber",
    .description = "US motor carriers by name — DOT number, operating status, "
      "safety rating, driver and vehicle out-of-service rates, crash counts and "
      "insurance status. The complete safety profile of a trucking counterparty" },

  { .id = "US_NHTSA_INVESTIGATIONS", .name = "NHTSA — defect investigations & recalls",
    .name_ja = "米国NHTSA — 欠陥調査/リコール", .category = "safety",
    .portal = "https://www.nhtsa.gov", .record_type = "us-vehicle-recall",
    .tags = "\"us\",\"recalls\",\"safety\"", .free_tier = 1,
    .url = "https://api.nhtsa.gov/recalls/recallsByVehicle?make={q}&modelYear=2024",
    .array_path = "results", .title_keys = "Component,Summary",
    .id_keys = "NHTSACampaignNumber", .date_keys = "ReportReceivedDate",
    .description = "Vehicle recalls and defect investigations by manufacturer — "
      "the component, the risk described, the remedy and the campaign number; "
      "the public safety record of an automaker" },

  { .id = "US_NTSB_ACCIDENTS", .name = "NTSB — transport accident investigations",
    .name_ja = "米国NTSB — 事故調査", .category = "safety",
    .portal = "https://data.ntsb.gov", .record_type = "us-accident",
    .tags = "\"us\",\"aviation\",\"safety\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.ntsb.gov/Pages/ResultsV2.aspx?queryText={q}",
    .base = "https://www.ntsb.gov", .filter_query = 1,
    .description = "NTSB aviation, rail, marine and pipeline accident dockets — "
      "operator, aircraft or vessel, probable cause and the safety "
      "recommendations issued afterwards" },

  { .id = "US_PHMSA_PIPELINE", .name = "PHMSA — pipeline incidents & operators",
    .name_ja = "米国PHMSA — パイプライン事故", .category = "infrastructure",
    .portal = "https://www.phmsa.dot.gov", .record_type = "us-pipeline-incident",
    .tags = "\"us\",\"energy\",\"critical-infrastructure\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.phmsa.dot.gov/search/site/{q}",
    .base = "https://www.phmsa.dot.gov", .filter_query = 1,
    .description = "Pipeline and hazardous-materials incidents by operator — "
      "release volume, cause, casualties and cost, plus enforcement actions "
      "against the operator" },

  /* ── Health & food ─────────────────────────────────────────────────────── */
  { .id = "US_FDA_INSPECTIONS", .name = "openFDA — inspection classifications",
    .name_ja = "米国FDA — 査察結果分類", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-fda-inspection",
    .tags = "\"us\",\"health\",\"enforcement\"", .free_tier = 1,
    .url = "https://api.fda.gov/food/enforcement.json?search=recalling_firm:%22{q}%22&limit=100",
    .array_path = "results", .title_keys = "product_description,reason_for_recall",
    .id_keys = "recall_number", .date_keys = "recall_initiation_date",
    .page_param = "skip", .page_size = 100, .page_start = 0,
    .description = "FDA food enforcement reports for a firm — the product, the "
      "hazard, the classification and the distribution pattern; the food-safety "
      "counterpart to the drug recall feed" },

  { .id = "US_FDA_DEVICE_RECALLS", .name = "openFDA — medical device recalls",
    .name_ja = "米国FDA — 医療機器リコール", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-device-recall",
    .tags = "\"us\",\"health\",\"recalls\"", .free_tier = 1,
    .url = "https://api.fda.gov/device/recall.json?search=recalling_firm:%22{q}%22&limit=100",
    .array_path = "results", .title_keys = "product_description,reason_for_recall",
    .id_keys = "cfres_id", .date_keys = "event_date_initiated",
    .page_param = "skip", .page_size = 100, .page_start = 0,
    .description = "Medical device recalls by manufacturer — device, root cause, "
      "the population affected and the recall class, which is the regulator's "
      "judgement of how dangerous the failure is" },

  { .id = "US_FDA_ADVERSE_DRUG", .name = "openFDA — drug adverse event reports",
    .name_ja = "米国FDA — 医薬品有害事象", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-adverse-event",
    .tags = "\"us\",\"health\",\"pharmacovigilance\"", .free_tier = 1,
    .url = "https://api.fda.gov/drug/event.json?search=patient.drug.openfda.manufacturer_name:%22{q}%22&limit=100",
    .array_path = "results", .title_keys = "safetyreportid,serious",
    .id_keys = "safetyreportid", .date_keys = "receiptdate",
    .page_param = "skip", .page_size = 100, .page_start = 0,
    .description = "Adverse event reports naming a manufacturer's drugs — "
      "outcome, seriousness, reporter country and the reaction terms; the "
      "signal base regulators act on" },

  { .id = "US_CMS_OPEN_PAYMENTS", .name = "CMS Open Payments — industry payments to clinicians",
    .name_ja = "米国CMS — 医療者への支払開示", .category = "health",
    .portal = "https://openpaymentsdata.cms.gov", .record_type = "us-industry-payment",
    .tags = "\"us\",\"health\",\"conflicts\"", .free_tier = 1,
    .url = "https://openpaymentsdata.cms.gov/api/1/datastore/query/"
           "a08c46b1-1200-5f6d-b4b0-b8de3e0d0c07/0?limit=100&conditions[0][property]"
           "=applicable_manufacturer_or_applicable_gpo_making_payment_name"
           "&conditions[0][value]={q}&conditions[0][operator]=like",
    .array_path = "results", .title_keys = "covered_recipient_last_name,teaching_hospital_name",
    .id_keys = "record_id", .date_keys = "date_of_payment",
    .page_param = "offset", .page_size = 100, .page_start = 0,
    .description = "Every payment or transfer of value from a drug/device maker "
      "to a named US physician or teaching hospital — amount, nature, product and "
      "date. The conflict-of-interest ledger for American medicine" },

  { .id = "US_CMS_PROVIDER_ENROLMENT", .name = "CMS — provider enrolment & ownership",
    .name_ja = "米国CMS — 医療機関所有者情報", .category = "health",
    .portal = "https://data.cms.gov", .record_type = "us-provider-ownership",
    .tags = "\"us\",\"health\",\"ownership\"", .free_tier = 1,
    .url = "https://data.cms.gov/data-api/v1/dataset/f7c4a1a3-9b0e-4bcb-9d2b-1c7f0c1f7c5c/data?size=100&keyword={q}",
    .filter_query = 1, .title_keys = "ORGANIZATION_NAME,ASSOCIATE_ID",
    .id_keys = "ASSOCIATE_ID", .page_param = "offset", .page_size = 100, .page_start = 0,
    .description = "Medicare provider enrolment data including ownership — the "
      "companies and individuals with a stake in a hospital, nursing home or "
      "hospice; private-equity roll-ups in US healthcare are visible here" },

  /* ── Environment ───────────────────────────────────────────────────────── */
  { .id = "US_EPA_FRS_FACILITIES", .name = "EPA FRS — facility registry",
    .name_ja = "米国EPA — 施設登録", .category = "government",
    .portal = "https://www.epa.gov/frs", .record_type = "us-epa-facility",
    .tags = "\"us\",\"environment\"", .free_tier = 1,
    .url = "https://data.epa.gov/efservice/FRS_FACILITY_SITE/FACILITY_NAME/CONTAINING/{q}/JSON",
    .title_keys = "facility_name,location_address", .id_keys = "registry_id",
    .lat_key = "latitude83", .lon_key = "longitude83",
    .description = "EPA's master facility registry — every regulated site "
      "operated under a company name, with coordinates and the programme "
      "identifiers that join to air, water and waste permits" },

  { .id = "US_EPA_TRI_RELEASES", .name = "EPA TRI — toxic release quantities",
    .name_ja = "米国EPA — 有害物質排出量", .category = "government",
    .portal = "https://www.epa.gov/toxics-release-inventory-tri-program",
    .record_type = "us-toxic-release", .tags = "\"us\",\"environment\",\"emissions\"",
    .free_tier = 1,
    .url = "https://data.epa.gov/efservice/tri_facility/facility_name/CONTAINING/{q}/JSON",
    .title_keys = "facility_name,city_name", .id_keys = "tri_facility_id",
    .lat_key = "pref_latitude", .lon_key = "pref_longitude",
    .description = "Toxics Release Inventory facilities operated by a company — "
      "the chemicals reported, the quantities released to air, water and land, "
      "and the year-on-year trend" },

  { .id = "US_EPA_ENFORCEMENT_CASES", .name = "EPA — civil enforcement case results",
    .name_ja = "米国EPA — 民事執行事件", .category = "government",
    .portal = "https://echo.epa.gov", .record_type = "us-epa-enforcement",
    .tags = "\"us\",\"environment\",\"enforcement\"", .free_tier = 1,
    .url = "https://echodata.epa.gov/echo/case_rest_services.get_cases?output=JSON&p_defendant={q}",
    .title_keys = "CaseName,DefendantEntity", .id_keys = "CaseNumber",
    .date_keys = "SettlementDate",
    .description = "Federal environmental enforcement cases against a defendant — "
      "the statutes, the penalty assessed, the injunctive relief and the "
      "settlement date" },

  /* ── Financial & corporate conduct ─────────────────────────────────────── */
  { .id = "US_SEC_ALJ_ACTIONS", .name = "SEC — administrative proceedings & litigation",
    .name_ja = "米国SEC — 行政手続/訴訟", .category = "economy",
    .portal = "https://www.sec.gov", .record_type = "us-sec-enforcement",
    .tags = "\"us\",\"finance\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.sec.gov/cgi-srv/srqsb?text={q}&first=1&last=100",
    .base = "https://www.sec.gov", .filter_query = 1,
    .description = "SEC administrative proceedings, litigation releases and "
      "trading suspensions naming a firm or individual — the enforcement layer "
      "over the EDGAR filing record" },

  { .id = "US_FINRA_BROKERCHECK", .name = "FINRA BrokerCheck — broker & firm record",
    .name_ja = "米国FINRA — 業者/個人照会", .category = "economy",
    .portal = "https://brokercheck.finra.org", .record_type = "us-broker",
    .tags = "\"us\",\"finance\",\"licences\"", .free_tier = 1,
    .url = "https://api.brokercheck.finra.org/search/firm?query={q}&hl=true&json.wrf=&nrows=100&start=0",
    .array_path = "hits.hits", .title_keys = "_source.firm_name,_source.ind_nm",
    .id_keys = "_source.firm_source_id",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "FINRA's record for a brokerage or registered representative — "
      "registrations, exams, employment history and, critically, the disclosure "
      "events: customer complaints, regulatory actions and terminations" },

  { .id = "US_CFTC_ACTIONS", .name = "CFTC — enforcement actions & RED list",
    .name_ja = "米国CFTC — 執行措置", .category = "economy",
    .portal = "https://www.cftc.gov", .record_type = "us-cftc-enforcement",
    .tags = "\"us\",\"finance\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.cftc.gov/search/node?keys={q}",
    .base = "https://www.cftc.gov", .filter_query = 1,
    .description = "Commodity Futures Trading Commission enforcement actions and "
      "the Registration Deficient list of unregistered foreign entities "
      "soliciting US customers — the crypto-fraud surface in particular" },

  { .id = "US_FTC_CASES", .name = "FTC — enforcement cases & refunds",
    .name_ja = "米国FTC — 執行事件", .category = "government",
    .portal = "https://www.ftc.gov", .record_type = "us-ftc-case",
    .tags = "\"us\",\"consumer\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.ftc.gov/search?search={q}",
    .base = "https://www.ftc.gov", .filter_query = 1,
    .description = "Federal Trade Commission cases — deceptive practice actions, "
      "consent orders and redress programmes naming the company and its "
      "principals, who are often barred from the industry afterwards" },

  { .id = "US_CONSUMER_COMPLAINTS", .name = "CFPB — consumer complaint database",
    .name_ja = "米国CFPB — 消費者苦情", .category = "economy",
    .portal = "https://www.consumerfinance.gov", .record_type = "us-consumer-complaint",
    .tags = "\"us\",\"finance\",\"complaints\"", .free_tier = 1,
    .url = "https://www.consumerfinance.gov/data-research/consumer-complaints/search/api/v1/"
           "?company={q}&size=100&frm=0&format=json",
    .array_path = "hits.hits", .title_keys = "_source.issue,_source.company",
    .id_keys = "_source.complaint_id", .date_keys = "_source.date_received",
    .page_param = "frm", .page_size = 100, .page_start = 0,
    .description = "Consumer complaints against a named financial company — the "
      "product, the issue, the company's response and whether it was timely. "
      "Volume and pattern here often precede regulatory action" },

  /* ── Licences, spectrum, grants, tax status ────────────────────────────── */
  { .id = "US_IRS_EXEMPT_ORGS", .name = "IRS — tax-exempt organisation status",
    .name_ja = "米国IRS — 免税団体資格", .category = "economy",
    .portal = "https://www.irs.gov", .record_type = "us-exempt-org",
    .tags = "\"us\",\"nonprofit\",\"tax\"", .free_tier = 1,
    .url = "https://apps.irs.gov/pub/epostcard/dl/FullData/data-download-pub78.txt",
    .mode = HP_CSV, .csv_no_header = 1, .filter_query = 1,
    .title_keys = "col1", .id_keys = "col0",
    .description = "The IRS Publication 78 list of organisations eligible for "
      "deductible contributions, read directly — whether a US charity's exempt "
      "status is current, revoked or never existed" },

  { .id = "US_FCC_ENFORCEMENT", .name = "FCC — enforcement actions & forfeitures",
    .name_ja = "米国FCC — 執行措置", .category = "telecom",
    .portal = "https://www.fcc.gov", .record_type = "us-fcc-enforcement",
    .tags = "\"us\",\"telecom\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.fcc.gov/search/#q={q}",
    .base = "https://www.fcc.gov", .filter_query = 1,
    .description = "FCC notices of apparent liability, forfeiture orders and "
      "consent decrees — robocall operations, pirate broadcasting and equipment "
      "authorisation violations, with the penalty amounts" },

  { .id = "US_GSA_EXCLUSIONS", .name = "SAM.gov — federal exclusions (debarment)",
    .name_ja = "米国 連邦調達除外リスト", .category = "government",
    .portal = "https://sam.gov", .record_type = "us-exclusion",
    .tags = "\"us\",\"debarment\",\"procurement\"", .key_env = "SAM_API_KEY",
    .free_tier = 1,
    .url = "https://api.sam.gov/entity-information/v3/exclusions?api_key={key}"
           "&exclusionName={q}&page=0",
    .array_path = "excludedEntity", .title_keys = "exclusionDetails.exclusionName",
    .id_keys = "exclusionIdentifier.exclusionId",
    .date_keys = "exclusionActions.activationDate",
    .page_param = "page", .page_start = 0,
    .description = "Parties excluded from US federal contracting and assistance — "
      "the excluding agency, the cause, the classification and the termination "
      "date. The American debarment list, at record level" },

  { .id = "US_TREASURY_JUDGMENTS", .name = "US Treasury — judgment fund payments",
    .name_ja = "米国財務省 — 判決基金支払", .category = "government",
    .portal = "https://fiscaldata.treasury.gov", .record_type = "us-judgment-payment",
    .tags = "\"us\",\"spending\",\"litigation\"", .free_tier = 1,
    .url = "https://api.fiscaldata.treasury.gov/services/api/fiscal_service/v2/accounting/od/"
           "judgment_fund?filter=defendant_agency_nm:in:({q})&page[size]=100",
    .array_path = "data", .title_keys = "defendant_agency_nm,principal_amt",
    .id_keys = "record_date", .date_keys = "record_date",
    .page_param = "page[number]", .page_start = 1,
    .description = "Payments made from the US Judgment Fund to settle claims "
      "against the government — agency, amount and citation; the cost side of "
      "federal litigation" },

  { .id = "US_FEDERAL_GRANTS", .name = "Grants.gov — federal grant opportunities & awards",
    .name_ja = "米国 連邦助成金", .category = "government",
    .portal = "https://www.grants.gov", .record_type = "us-grant",
    .tags = "\"us\",\"grants\",\"spending\"", .free_tier = 1,
    .url = "https://api.grants.gov/v1/api/search2",
    .post_body = "{\"keyword\":\"{Q}\",\"rows\":100,\"startRecordNum\":0}",
    .array_path = "data.oppHits", .title_keys = "title,agency",
    .id_keys = "id", .date_keys = "openDate",
    .description = "Federal grant opportunities and their awarding agencies — "
      "the funding side that pairs with USAspending's award records for "
      "nonprofits and research institutions" },
};

HP_REGISTER_TABLE(HP2_US_REG)
