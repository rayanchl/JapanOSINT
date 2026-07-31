/* collectors/sources/hp3_gov_us_federal.c — US federal government record depth.
 *
 * The US federal government is the most API-exposed state apparatus in the
 * world, and almost none of that surface is a "search box": it is record-level
 * disclosure with a stable identifier per record — a lobbying filing keyed to a
 * registrant, an FCC licence keyed to a call sign, an NIH project keyed to a
 * core project number, an exclusion keyed to a provider NPI. This file wires
 * the endpoints where the *record* lives, not the press release about it.
 *
 * Nothing here is a scrape of convenience: every row names an endpoint the
 * agency publishes for programmatic use, and where the agency requires a key
 * the row declares `key_env` so a missing credential is an honest empty rather
 * than a silent zero. */
#include "../../lib/hpengine.h"

static const hp_source HP3_GOV_US_FED[] = {
  /* ── Lobbying, influence and foreign agents ───────────────────────────── */
  { .id = "US_SENATE_LDA_LOBBYISTS", .name = "US Senate LDA — individual registered lobbyists",
    .name_ja = "米国上院 登録ロビイスト個人", .category = "government",
    .portal = "https://lda.senate.gov", .record_type = "us-lobbyist",
    .tags = "\"us\",\"lobbying\",\"influence\"", .free_tier = 1,
    .url = "https://lda.senate.gov/api/v1/lobbyists/?lobbyist_name={q}&page_size=100",
    .array_path = "results", .title_keys = "lobbyist_name,registrant_name",
    .id_keys = "id", .next_path = "next", .page_max = 40,
    .description = "The named humans doing the lobbying, not the firms — with "
      "the covered-position field that discloses whether they previously held "
      "federal office, which is the revolving door in machine-readable form" },

  { .id = "US_SENATE_LDA_CLIENTS", .name = "US Senate LDA — lobbying clients",
    .name_ja = "米国上院 ロビー依頼主", .category = "government",
    .portal = "https://lda.senate.gov", .record_type = "us-lobbying-client",
    .tags = "\"us\",\"lobbying\",\"influence\"", .free_tier = 1,
    .url = "https://lda.senate.gov/api/v1/clients/?client_name={q}&page_size=100",
    .array_path = "results", .title_keys = "name,state_display",
    .id_keys = "id", .next_path = "next", .page_max = 40,
    .description = "Who is buying federal lobbying — the client entity, its "
      "general description, country of incorporation and the registrant "
      "retained. Foreign-domiciled clients are visible here by country code" },

  { .id = "US_SENATE_LDA_REGISTRANTS", .name = "US Senate LDA — registered lobbying firms",
    .name_ja = "米国上院 ロビー登録業者", .category = "government",
    .portal = "https://lda.senate.gov", .record_type = "us-lobbying-registrant",
    .tags = "\"us\",\"lobbying\",\"influence\"", .free_tier = 1,
    .url = "https://lda.senate.gov/api/v1/registrants/?registrant_name={q}&page_size=100",
    .array_path = "results", .title_keys = "name", .id_keys = "id",
    .next_path = "next", .page_max = 40,
    .detail_url = "https://lda.senate.gov/api/v1/registrants/{v}/", .detail_key = "id",
    .description = "The lobbying firms themselves — address, contact, "
      "description of the practice and the identifier that joins them to every "
      "filing and contribution report they have submitted" },

  { .id = "US_SENATE_LDA_CONTRIBUTIONS", .name = "US Senate LDA — LD-203 political contributions",
    .name_ja = "米国上院 ロビイスト政治献金", .category = "government",
    .portal = "https://lda.senate.gov", .record_type = "us-lobbying-contribution",
    .tags = "\"us\",\"lobbying\",\"campaign-finance\"", .free_tier = 1,
    .url = "https://lda.senate.gov/api/v1/contributions/?registrant_name={q}&page_size=100",
    .array_path = "results", .title_keys = "registrant.name,lobbyist.lobbyist_name",
    .id_keys = "filing_uuid", .date_keys = "dt_posted",
    .next_path = "next", .page_max = 40,
    .description = "The LD-203 semi-annual reports: contributions made by "
      "lobbyists and their firms to federal candidates, leadership PACs and "
      "presidential libraries — the money that runs alongside the lobbying" },

  { .id = "US_FARA_REGISTRANTS", .name = "US FARA — active foreign agent registrants",
    .name_ja = "米国FARA 外国代理人登録", .category = "government",
    .portal = "https://efile.fara.gov", .record_type = "us-foreign-agent",
    .tags = "\"us\",\"foreign-influence\",\"lobbying\"", .free_tier = 1,
    .url = "https://efile.fara.gov/api/v1/Registrants/json/Active",
    .array_path = "REGISTRANTS_ACTIVE.ROW", .filter_query = 1,
    .title_keys = "Name,Address_1", .id_keys = "Registration_Number",
    .date_keys = "Registration_Date",
    .description = "Everyone currently registered under the Foreign Agents "
      "Registration Act — the registrant, its registration number and date, and "
      "the foreign principal it acts for. The definitive list of who is paid to "
      "represent a foreign government or party inside the US" },

  { .id = "US_FARA_FOREIGN_PRINCIPALS", .name = "US FARA — foreign principals represented",
    .name_ja = "米国FARA 外国本人一覧", .category = "government",
    .portal = "https://efile.fara.gov", .record_type = "us-foreign-principal",
    .tags = "\"us\",\"foreign-influence\"", .free_tier = 1,
    .url = "https://efile.fara.gov/api/v1/ForeignPrincipals/json/Active",
    .array_path = "FOREIGNPRINCIPALS_ACTIVE.ROW", .filter_query = 1,
    .title_keys = "Foreign_principal,Registrant_name",
    .id_keys = "Registration_number", .date_keys = "Foreign_principal_registration_date",
    .description = "The other end of the FARA relationship — which foreign "
      "government, party or state enterprise each US registrant works for, with "
      "the country and the date the representation began" },

  /* ── Campaign finance ─────────────────────────────────────────────────── */
  { .id = "US_FEC_CANDIDATE_SEARCH", .name = "FEC — federal candidate records",
    .name_ja = "米国FEC 連邦候補者", .category = "government",
    .portal = "https://api.open.fec.gov", .record_type = "us-candidate",
    .tags = "\"us\",\"campaign-finance\",\"elections\"",
    .key_env = "FEC_API_KEY", .free_tier = 1,
    .url = "https://api.open.fec.gov/v1/candidates/search/?api_key={key}&q={q}&per_page=100",
    .array_path = "results", .title_keys = "name", .id_keys = "candidate_id",
    .page_param = "page", .page_size = 100, .page_max = 30,
    .detail_url = "https://api.open.fec.gov/v1/candidate/{v}/totals/?api_key={key}",
    .detail_key = "candidate_id", .detail_path = "results",
    .description = "Federal candidates by name, then the second hop into their "
      "cycle totals — receipts, disbursements, cash on hand and debts. The "
      "candidate's actual financial position, not just their filing status" },

  { .id = "US_FEC_COMMITTEE_SEARCH", .name = "FEC — political committees & PACs",
    .name_ja = "米国FEC 政治委員会/PAC", .category = "government",
    .portal = "https://api.open.fec.gov", .record_type = "us-pac",
    .tags = "\"us\",\"campaign-finance\"", .key_env = "FEC_API_KEY", .free_tier = 1,
    .url = "https://api.open.fec.gov/v1/committees/?api_key={key}&q={q}&per_page=100",
    .array_path = "results", .title_keys = "name", .id_keys = "committee_id",
    .page_param = "page", .page_size = 100, .page_max = 30,
    .detail_url = "https://api.open.fec.gov/v1/committee/{v}/totals/?api_key={key}",
    .detail_key = "committee_id", .detail_path = "results",
    .description = "PACs, super PACs, party and corporate-connected committees "
      "— treasurer, connected organisation, designation and the receipts and "
      "independent-expenditure totals behind the name" },

  { .id = "US_FEC_INDEPENDENT_EXPENDITURES", .name = "FEC — independent expenditures",
    .name_ja = "米国FEC 独立支出", .category = "government",
    .portal = "https://api.open.fec.gov", .record_type = "us-independent-expenditure",
    .tags = "\"us\",\"campaign-finance\",\"dark-money\"",
    .key_env = "FEC_API_KEY", .free_tier = 1,
    .url = "https://api.open.fec.gov/v1/schedules/schedule_e/?api_key={key}"
      "&payee_name={q}&per_page=100",
    .array_path = "results", .title_keys = "payee_name,candidate_name",
    .id_keys = "transaction_id", .date_keys = "expenditure_date",
    .page_param = "page", .page_size = 100, .page_max = 30,
    .description = "Spending for or against a federal candidate by committees "
      "that do not coordinate with them — the vendor paid, the amount, the "
      "candidate supported or opposed, and the date it was disseminated" },

  { .id = "US_FEC_ELECTIONEERING", .name = "FEC — electioneering communications",
    .name_ja = "米国FEC 選挙関連広告支出", .category = "government",
    .portal = "https://api.open.fec.gov", .record_type = "us-electioneering",
    .tags = "\"us\",\"campaign-finance\",\"media\"",
    .key_env = "FEC_API_KEY", .free_tier = 1,
    .url = "https://api.open.fec.gov/v1/electioneering/?api_key={key}"
      "&candidate_name={q}&per_page=100",
    .array_path = "results", .title_keys = "candidate_name,communication_date",
    .id_keys = "sub_id", .date_keys = "communication_date",
    .page_param = "page", .page_size = 100, .page_max = 30,
    .description = "Broadcast advertising that names a federal candidate close "
      "to an election without expressly advocating — the payer, the audience "
      "reached and the disbursement, which is where issue-ad money surfaces" },

  /* ── Rulemaking, regulation and the record of the state ───────────────── */
  { .id = "US_REGULATIONS_DOCUMENTS", .name = "Regulations.gov — rulemaking documents",
    .name_ja = "米国 規則制定文書", .category = "government",
    .portal = "https://www.regulations.gov", .record_type = "us-rulemaking-document",
    .tags = "\"us\",\"regulation\"", .key_env = "REGULATIONS_GOV_API_KEY",
    .free_tier = 1,
    .url = "https://api.regulations.gov/v4/documents?filter[searchTerm]={q}"
      "&page[size]=250&api_key={key}",
    .array_path = "data", .title_keys = "attributes.title",
    .id_keys = "id", .date_keys = "attributes.postedDate",
    .page_param = "page[number]", .page_size = 250, .page_max = 20,
    .detail_url = "https://api.regulations.gov/v4/documents/{v}?api_key={key}",
    .detail_key = "id",
    .description = "Proposed and final rules, notices and supporting studies, "
      "then the detail hop for the full document metadata — the agency, the "
      "docket, the comment window and the regulatory identifier number" },

  { .id = "US_REGULATIONS_COMMENTS", .name = "Regulations.gov — public comments on rules",
    .name_ja = "米国 規則へのパブリックコメント", .category = "government",
    .portal = "https://www.regulations.gov", .record_type = "us-rule-comment",
    .tags = "\"us\",\"regulation\",\"influence\"",
    .key_env = "REGULATIONS_GOV_API_KEY", .free_tier = 1,
    .url = "https://api.regulations.gov/v4/comments?filter[searchTerm]={q}"
      "&page[size]=250&api_key={key}",
    .array_path = "data", .title_keys = "attributes.title",
    .id_keys = "id", .date_keys = "attributes.postedDate",
    .page_param = "page[number]", .page_size = 250, .page_max = 20,
    .detail_url = "https://api.regulations.gov/v4/comments/{v}?api_key={key}",
    .detail_key = "id",
    .description = "Who wrote to a federal agency about a rule, and what they "
      "asked for. Corporate comment letters are a public statement of interest "
      "that rarely appears anywhere else in a company's own disclosures" },

  { .id = "US_FR_PUBLIC_INSPECTION", .name = "Federal Register — public inspection desk",
    .name_ja = "米国官報 事前公開文書", .category = "government",
    .portal = "https://www.federalregister.gov", .record_type = "us-fr-pending",
    .tags = "\"us\",\"regulation\",\"early-signal\"", .free_tier = 1,
    .url = "https://www.federalregister.gov/api/v1/public-inspection-documents.json"
      "?conditions[term]={q}&per_page=1000",
    .array_path = "results", .title_keys = "title", .id_keys = "document_number",
    .date_keys = "filing_date", .link_keys = "html_url",
    .next_path = "next_page_url", .page_max = 20,
    .description = "Documents filed for publication but not yet in the Federal "
      "Register — typically a one-to-three-day lead on a rule, sanction or "
      "enforcement notice becoming official" },

  { .id = "US_ECFR_SEARCH", .name = "eCFR — Code of Federal Regulations full text",
    .name_ja = "米国連邦規則集 全文検索", .category = "government",
    .portal = "https://www.ecfr.gov", .record_type = "us-cfr-section",
    .tags = "\"us\",\"regulation\",\"law\"", .free_tier = 1,
    .url = "https://www.ecfr.gov/api/search/v1/results?query={q}&per_page=100",
    .array_path = "results", .title_keys = "hierarchy_headings.section,full_text_excerpt",
    .id_keys = "structure_index", .date_keys = "starts_on",
    .page_param = "page", .page_size = 100, .page_max = 20,
    .description = "The operative text of US federal regulation, section by "
      "section, with the title/part/section hierarchy and the effective date — "
      "what a firm is actually bound by, as opposed to what was proposed" },

  /* ── Money out: grants, awards and research funding ───────────────────── */
  { .id = "US_CLINICAL_TRIALS_SPONSORS", .name = "ClinicalTrials.gov — studies by sponsor",
    .name_ja = "米国臨床試験 スポンサー検索", .category = "health",
    .portal = "https://clinicaltrials.gov", .record_type = "us-clinical-trial",
    .tags = "\"us\",\"health\",\"research\"", .free_tier = 1,
    .url = "https://clinicaltrials.gov/api/v2/studies?query.spons={q}&pageSize=1000",
    .array_path = "studies",
    .title_keys = "protocolSection.identificationModule.briefTitle",
    .id_keys = "protocolSection.identificationModule.nctId",
    .date_keys = "protocolSection.statusModule.startDateStruct.date",
    .next_path = "nextPageToken", .page_max = 40,
    .description = "Every registered clinical trial for a sponsor — the NCT "
      "number, phase, status, enrolment, the collaborators and the "
      "investigator sites with their cities. Terminated and withdrawn trials "
      "are included, and they are usually the interesting ones" },

  { .id = "US_NSF_AWARDS", .name = "NSF — awards by institution or investigator",
    .name_ja = "米国NSF 助成金", .category = "research",
    .portal = "https://api.nsf.gov", .record_type = "us-research-grant",
    .tags = "\"us\",\"research\",\"funding\"", .free_tier = 1,
    .url = "https://api.nsf.gov/services/v1/awards.json?keyword={q}&rpp=25"
      "&printFields=id,title,awardeeName,awardeeCity,awardeeStateCode,piFirstName,"
      "piLastName,date,startDate,expDate,estimatedTotalAmt,fundsObligatedAmt,"
      "fundProgramName,primaryProgram,agency,awardeeCountryCode,coPDPI,abstractText",
    .array_path = "response.award", .title_keys = "title,awardeeName",
    .id_keys = "id", .date_keys = "startDate",
    .page_param = "offset", .page_size = 25, .page_start = 1, .page_max = 40,
    .description = "National Science Foundation awards — the institution, the "
      "named PI and co-PIs, obligated and total amounts, programme and the "
      "abstract. Explicitly requests the full field set rather than the "
      "four-field default the API returns when asked for nothing" },

  { .id = "US_DOE_OSTI_RECORDS", .name = "US DOE OSTI — scientific & technical research records",
    .name_ja = "米国エネルギー省 科学技術情報", .category = "research",
    .portal = "https://www.osti.gov", .record_type = "us-research-output",
    .tags = "\"us\",\"research\",\"energy\"", .free_tier = 1,
    .url = "https://www.osti.gov/api/v1/records?q={q}&rows=100",
    .title_keys = "title,research_org", .id_keys = "osti_id",
    .date_keys = "publication_date", .link_keys = "links.0.href",
    .page_param = "page", .page_size = 100, .page_max = 40,
    .description = "Department of Energy research output — the performing "
      "national laboratory or contractor, the sponsoring DOE office, the "
      "contract number funding the work and the authors. The contract number "
      "links a technical result back to a specific federal award" },

  /* ── Financial institutions and the banking perimeter ─────────────────── */
  { .id = "US_FDIC_INSTITUTIONS", .name = "FDIC BankFind — insured institutions",
    .name_ja = "米国FDIC 預金保険加盟機関", .category = "finance",
    .portal = "https://banks.data.fdic.gov", .record_type = "us-bank",
    .tags = "\"us\",\"banking\",\"licence\"", .free_tier = 1,
    .url = "https://banks.data.fdic.gov/api/institutions?search={q}&limit=250"
      "&format=json",
    .array_path = "data", .title_keys = "data.NAME,data.CITY",
    .id_keys = "data.CERT", .date_keys = "data.ESTYMD",
    .page_param = "offset", .page_size = 250, .page_start = 0, .page_max = 20,
    .description = "Every FDIC-insured bank — certificate number, charter "
      "class, regulator, holding company, total assets and deposits, and the "
      "dates of establishment, merger or failure" },

  { .id = "US_FDIC_FAILURES", .name = "FDIC — failed bank list",
    .name_ja = "米国FDIC 破綻銀行", .category = "finance",
    .portal = "https://banks.data.fdic.gov", .record_type = "us-bank-failure",
    .tags = "\"us\",\"banking\",\"distress\"", .free_tier = 1,
    .url = "https://banks.data.fdic.gov/api/failures?search={q}&limit=250&format=json",
    .array_path = "data", .title_keys = "data.NAME,data.CITYST",
    .id_keys = "data.CERT", .date_keys = "data.FAILDATE",
    .page_param = "offset", .page_size = 250, .page_start = 0, .page_max = 20,
    .description = "Bank failures and assisted transactions — the failure date, "
      "the acquiring institution, total assets and deposits at failure and the "
      "estimated cost to the insurance fund" },

  { .id = "US_NCUA_CREDIT_UNIONS", .name = "NCUA — federally insured credit unions",
    .name_ja = "米国NCUA 信用組合", .category = "finance",
    .portal = "https://mapping.ncua.gov", .record_type = "us-credit-union",
    .tags = "\"us\",\"banking\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://mapping.ncua.gov/ResearchCreditUnion?searchTerm={q}",
    .base = "https://mapping.ncua.gov", .filter_query = 1,
    .description = "Federally insured credit unions — charter number, field of "
      "membership, branch network and the call-report financials. The part of "
      "US retail banking that never appears in an FDIC query" },

  /* ── Exclusions, debarment and provider integrity ─────────────────────── */
  { .id = "US_HHS_OIG_EXCLUSIONS", .name = "HHS OIG — LEIE healthcare exclusions",
    .name_ja = "米国HHS監察官 医療除外リスト", .category = "government",
    .portal = "https://oig.hhs.gov", .record_type = "us-exclusion",
    .tags = "\"us\",\"health\",\"debarment\",\"sanctions\"", .free_tier = 1,
    .type = "dataset", .mode = HP_CSV,
    .url = "https://oig.hhs.gov/exclusions/downloadables/UPDATED.csv",
    .filter_query = 1, .title_keys = "LASTNAME,BUSNAME,FIRSTNAME",
    .id_keys = "NPI,UPIN", .date_keys = "EXCLDATE",
    .description = "The List of Excluded Individuals and Entities — everyone "
      "barred from federal healthcare programmes, with the statutory exclusion "
      "type, the NPI where known, the general business address and the "
      "exclusion and reinstatement dates" },

  { .id = "US_SAM_EXCLUSIONS", .name = "SAM.gov — federal contractor exclusions",
    .name_ja = "米国SAM 連邦調達除外", .category = "government",
    .portal = "https://sam.gov", .record_type = "us-debarment",
    .tags = "\"us\",\"procurement\",\"debarment\"", .key_env = "SAM_GOV_API_KEY",
    .free_tier = 1,
    .url = "https://api.sam.gov/entity-information/v4/exclusions?api_key={key}"
      "&exclusionName={q}",
    .array_path = "excludedEntity", .title_keys = "exclusionName,exclusionProgram",
    .id_keys = "exclusionIdentifier", .date_keys = "exclusionActions.activationDate",
    .page_param = "page", .page_size = 100, .page_start = 0, .page_max = 20,
    .description = "Suspensions and debarments across the whole federal "
      "government — the excluding agency, the cause, the exclusion type and "
      "whether it reaches affiliates. The single hardest disqualifier in US "
      "public contracting" },

  { .id = "US_FPDS_CONTRACT_ACTIONS", .name = "FPDS — federal procurement data feed",
    .name_ja = "米国連邦調達データ", .category = "government",
    .portal = "https://www.fpds.gov", .record_type = "us-contract-action",
    .tags = "\"us\",\"procurement\",\"contract\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.fpds.gov/ezsearch/search.do?s=FPDS&indexName=awardfull"
      "&templateName=1.5.3&q={q}",
    .base = "https://www.fpds.gov", .filter_query = 1,
    .description = "The Federal Procurement Data System, which records every "
      "contract action rather than the summarised award — modifications, "
      "exercised options, the contracting office, the competition status and "
      "the place of performance for each action" },

  /* ── Spectrum, aviation and the physical federal estate ───────────────── */
  { .id = "US_COLLEGE_SCORECARD", .name = "US Dept of Education — College Scorecard",
    .name_ja = "米国教育省 大学スコアカード", .category = "government",
    .portal = "https://collegescorecard.ed.gov", .record_type = "us-institution",
    .tags = "\"us\",\"education\",\"registry\"", .key_env = "DATA_GOV_API_KEY",
    .free_tier = 1,
    .url = "https://api.data.gov/ed/collegescorecard/v1/schools?api_key={key}"
      "&school.name={q}&per_page=100",
    .array_path = "results", .title_keys = "school.name,school.city",
    .id_keys = "id", .lat_key = "location.lat", .lon_key = "location.lon",
    .page_param = "page", .page_size = 100, .page_start = 0, .page_max = 20,
    .description = "Every degree-granting institution the federal government "
      "recognises — ownership (public, private non-profit, for-profit), "
      "accreditor, enrolment, federal aid volume and outcome metrics. The "
      "for-profit education sector's regulatory record in one query" },

  { .id = "US_FCC_ECFS_FILINGS", .name = "FCC ECFS — proceedings filings by party",
    .name_ja = "米国FCC 電子申請文書", .category = "government",
    .portal = "https://www.fcc.gov", .record_type = "us-fcc-filing",
    .tags = "\"us\",\"telecom\",\"regulation\"", .key_env = "FCC_API_KEY",
    .free_tier = 1,
    .url = "https://publicapi.fcc.gov/ecfs/filings?api_key={key}&q={q}&limit=100",
    .array_path = "filing", .title_keys = "brief_comment_summary,submissiontype.description",
    .id_keys = "id_submission", .date_keys = "date_received",
    .page_param = "offset", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Everything a party has filed in an FCC proceeding — ex "
      "parte notices naming who met which commissioner, comments, and the "
      "docket each belongs to. The clearest record of telecom lobbying" },

  { .id = "US_FAA_AIRCRAFT_REGISTRY", .name = "FAA — N-number aircraft registry",
    .name_ja = "米国FAA 航空機登録", .category = "transport",
    .portal = "https://registry.faa.gov", .record_type = "us-aircraft",
    .tags = "\"us\",\"aviation\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://registry.faa.gov/aircraftinquiry/Search/NameResult?Nametxt={q}",
    .base = "https://registry.faa.gov", .filter_query = 1,
    .description = "Aircraft registered to a named owner — N-number, serial, "
      "make and model, airworthiness class and the registered address. Corporate "
      "aircraft ownership is one of the few asset classes with a public index" },

  { .id = "US_NARA_CATALOG", .name = "US National Archives — catalog records",
    .name_ja = "米国国立公文書館 目録", .category = "government",
    .portal = "https://catalog.archives.gov", .record_type = "us-archive-record",
    .tags = "\"us\",\"archive\",\"history\"", .free_tier = 1,
    .url = "https://catalog.archives.gov/api/v1?q={q}&rows=100",
    .array_path = "opaResponse.results.result", .title_keys = "description.title,title",
    .id_keys = "naId", .date_keys = "releaseDate",
    .page_param = "offset", .page_size = 100, .page_start = 0, .page_max = 20,
    .description = "The National Archives catalog — declassified files, agency "
      "series, personnel and immigration records, with the National Archives "
      "Identifier and the digitised object where one exists" },

  { .id = "US_DATA_GOV_CKAN", .name = "Data.gov — federal dataset catalog",
    .name_ja = "米国データポータル 目録", .category = "government",
    .portal = "https://catalog.data.gov", .record_type = "us-dataset",
    .tags = "\"us\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://catalog.data.gov/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "The federal open-data catalog — which agency publishes a "
      "dataset touching a subject, the resource URLs behind it and the "
      "publication and modification dates. A directory of where the raw "
      "government record on a topic actually lives" },
};

HP_REGISTER_TABLE(HP3_GOV_US_FED)
