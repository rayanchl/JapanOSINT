/* collectors/sources/hp_courts_legal.c — court dockets and legal-record depth.
 *
 * A case name is not a case. CourtListener alone exposes dockets, the parties
 * and attorneys on them, the individual RECAP documents, judge biographies and
 * judges' financial disclosures — five different records behind one search hit,
 * all free. The rest of these rows cover the same idea in other jurisdictions:
 * ECHR's HUDOC, CanLII, AustLII, BAILII, the US Federal Register and GovInfo,
 * WIPO domain-name panel decisions, and the US bankruptcy/appellate surface. */
#include "../../lib/hpengine.h"

#define CL_KEY  "COURTLISTENER_API_KEY"
#define CL_AUTH { "Authorization: Token {key}", NULL }
#define CL_BASE "https://www.courtlistener.com/api/rest/v4"

static const hp_source HP_COURTS[] = {
  { .id = "CL_DOCKETS", .name = "CourtListener — dockets by party",
    .name_ja = "CourtListener — 事件記録検索", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-docket",
    .tags = "\"us\",\"courts\",\"dockets\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1, .url = CL_BASE "/dockets/?case_name__icontains={q}&page_size=40",
    .array_path = "results", .title_keys = "case_name", .id_keys = "id",
    .date_keys = "date_filed", .max_items = 40,
    .description = "US federal and state dockets whose case name contains the "
      "entity — court, docket number, nature of suit, cause, judge assigned and "
      "filing/termination dates" },

  { .id = "CL_PARTIES", .name = "CourtListener — parties & counsel",
    .name_ja = "CourtListener — 当事者と代理人", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-court-party",
    .tags = "\"us\",\"courts\",\"parties\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1, .url = CL_BASE "/parties/?name__icontains={q}&page_size=40",
    .array_path = "results", .title_keys = "name", .id_keys = "id", .max_items = 40,
    .description = "Named parties in US litigation and the attorneys who appeared "
      "for them — the law-firm relationships around an entity, which are frequently "
      "more stable than its corporate filings" },

  { .id = "CL_RECAP_DOCUMENTS", .name = "CourtListener — RECAP filings full text",
    .name_ja = "CourtListener — RECAP 提出書類", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-court-filing",
    .tags = "\"us\",\"courts\",\"recap\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1,
    .url = CL_BASE "/search/?q={q}&type=r&order_by=dateFiled%20desc",
    .array_path = "results", .title_keys = "caseName,description",
    .id_keys = "id", .date_keys = "dateFiled", .max_items = 40,
    .description = "Full-text search across PACER documents archived in RECAP — the "
      "actual complaints, motions and exhibits mentioning an entity, with docket "
      "entry numbers and document links" },

  { .id = "CL_OPINIONS", .name = "CourtListener — opinions full text",
    .name_ja = "CourtListener — 判決本文検索", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-opinion",
    .tags = "\"us\",\"courts\",\"opinions\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1, .url = CL_BASE "/search/?q={q}&type=o&order_by=dateFiled%20desc",
    .array_path = "results", .title_keys = "caseName", .id_keys = "id",
    .date_keys = "dateFiled", .max_items = 40,
    .description = "Published US opinions naming an entity — court, panel, citation "
      "and snippet. Opinions carry the findings of fact that other sources only "
      "allude to" },

  { .id = "CL_JUDGE_DISCLOSURES", .name = "CourtListener — judicial financial disclosures",
    .name_ja = "CourtListener — 裁判官財産開示", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-judge-disclosure",
    .tags = "\"us\",\"courts\",\"pep\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1,
    .url = CL_BASE "/financial-disclosures/?person__name_last__icontains={q}&page_size=25",
    .array_path = "results", .title_keys = "person,filepath", .id_keys = "id",
    .date_keys = "year", .max_items = 25,
    .description = "US federal judges' annual financial disclosure filings — "
      "investments, gifts, reimbursements and outside income, which is what shows a "
      "judge's exposure to a party before them" },

  { .id = "CL_JUDGES", .name = "CourtListener — judge biographies",
    .name_ja = "CourtListener — 裁判官経歴", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-judge",
    .tags = "\"us\",\"courts\",\"people\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1, .url = CL_BASE "/people/?name_last__icontains={q}&page_size=25",
    .array_path = "results", .title_keys = "name_full,name_last", .id_keys = "id",
    .date_keys = "date_dob", .max_items = 25,
    .description = "Biographical records for US judges — appointments, confirmation "
      "dates, prior positions, education, political affiliations and ABA ratings" },

  /* ── Other jurisdictions ───────────────────────────────────────────────── */
  { .id = "ECHR_HUDOC", .name = "ECHR HUDOC — Strasbourg case law",
    .name_ja = "欧州人権裁判所 判例(HUDOC)", .category = "legal",
    .portal = "https://hudoc.echr.coe.int", .record_type = "echr-judgment",
    .tags = "\"eu\",\"courts\",\"human-rights\"", .free_tier = 1,
    .url = "https://hudoc.echr.coe.int/app/query/results?query=contentsitename%3AECHR"
           "%20AND%20%28NOT%20%28doctype%3DPR%20OR%20doctype%3DHFCOMOLD%29%29%20AND%20"
           "%22{q}%22&select=itemid,docname,doctype,appno,conclusion,judgementdate"
           "&sort=&start=0&length=40",
    .array_path = "results", .title_keys = "columns.docname", .id_keys = "columns.itemid",
    .date_keys = "columns.judgementdate", .max_items = 40,
    .description = "European Court of Human Rights judgments and decisions naming an "
      "entity or applicant — application number, respondent state, conclusion and "
      "judgment date" },

  { .id = "CANLII_SEARCH", .name = "CanLII — Canadian case law & legislation",
    .name_ja = "カナダ 判例(CanLII)", .category = "legal",
    .portal = "https://www.canlii.org", .record_type = "ca-judgment",
    .tags = "\"ca\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.canlii.org/en/#search/text={q}",
    .href_must = "/doc/", .base = "https://www.canlii.org", .max_items = 25,
    .description = "Canadian federal and provincial court and tribunal decisions "
      "naming an entity. CanLII's JSON API needs a key; the public search surface "
      "is used here and yields real document links or nothing" },

  { .id = "AUSTLII_SEARCH", .name = "AustLII — Australian case law",
    .name_ja = "オーストラリア 判例(AustLII)", .category = "legal",
    .portal = "https://www.austlii.edu.au", .record_type = "au-judgment",
    .tags = "\"au\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.austlii.edu.au/cgi-bin/sinosrch.cgi?method=auto&query={q}",
    .href_must = "/cases/", .base = "https://www.austlii.edu.au", .max_items = 25,
    .description = "Australian judgments, tribunal decisions and legislation "
      "mentioning an entity — the full-text corpus behind ASIC's thin public "
      "company data" },

  { .id = "BAILII_SEARCH", .name = "BAILII — UK & Irish case law",
    .name_ja = "英・愛 判例(BAILII)", .category = "legal",
    .portal = "https://www.bailii.org", .record_type = "uk-judgment",
    .tags = "\"uk\",\"ie\",\"courts\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.bailii.org/cgi-bin/lucy_search_1.cgi?query={q}&method=boolean",
    .href_must = "/cases/", .base = "https://www.bailii.org", .max_items = 25,
    .description = "UK and Irish judgments naming an entity — the litigation record "
      "behind a Companies House profile, including winding-up and fraud proceedings" },

  { .id = "US_FEDERAL_REGISTER", .name = "Federal Register — US agency documents",
    .name_ja = "米国官報 — 連邦公報検索", .category = "government",
    .portal = "https://www.federalregister.gov", .record_type = "us-fedreg-document",
    .tags = "\"us\",\"regulation\"", .free_tier = 1,
    .url = "https://www.federalregister.gov/api/v1/documents.json"
           "?conditions%5Bterm%5D={q}&per_page=40&order=newest",
    .array_path = "results", .title_keys = "title", .id_keys = "document_number",
    .date_keys = "publication_date", .max_items = 40,
    .description = "US Federal Register documents naming an entity — rules, notices, "
      "sanctions designations, licence grants and enforcement orders, with agency, "
      "docket and publication date" },

  { .id = "US_GOVINFO_SEARCH", .name = "GovInfo — US government publications",
    .name_ja = "米国政府刊行物検索(GovInfo)", .category = "government",
    .portal = "https://www.govinfo.gov", .record_type = "us-govinfo-document",
    .tags = "\"us\",\"congress\",\"courts\"", .key_env = "GOVINFO_API_KEY",
    .free_tier = 1, .url = "https://api.govinfo.gov/search?api_key={key}",
    .post_body = "{\"query\":\"{Q}\",\"pageSize\":40,\"offsetMark\":\"*\","
                 "\"sorts\":[{\"field\":\"publishdate\",\"sortOrder\":\"DESC\"}]}",
    .array_path = "results", .title_keys = "title", .id_keys = "packageId",
    .date_keys = "dateIssued", .max_items = 40,
    .description = "Congressional hearings and reports, US Courts opinions, the "
      "Congressional Record, CFR and public laws mentioning an entity — one index "
      "over the whole federal publication corpus" },

  { .id = "WIPO_UDRP_DECISIONS", .name = "WIPO — UDRP domain dispute decisions",
    .name_ja = "WIPO ドメイン紛争裁定", .category = "legal",
    .portal = "https://www.wipo.int/amc/en/domains/", .record_type = "udrp-decision",
    .tags = "\"domains\",\"disputes\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.wipo.int/amc/en/domains/search/legalindex-results.jsp?q={q}",
    .base = "https://www.wipo.int", .filter_query = 1, .max_items = 25,
    .description = "WIPO domain-name panel decisions involving an entity or domain — "
      "complainant, respondent, the disputed domains and the outcome; a reliable way "
      "to attribute cybersquatting portfolios to a person" },

  { .id = "US_PACER_RSS_COURTS", .name = "CourtListener — recent PACER activity feed",
    .name_ja = "米国連邦裁判所 最新提出", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-court-filing",
    .tags = "\"us\",\"courts\",\"realtime\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1,
    .url = CL_BASE "/search/?q={q}&type=r&order_by=entry_date_filed%20desc&page_size=25",
    .array_path = "results", .title_keys = "caseName,short_description",
    .id_keys = "id", .date_keys = "entry_date_filed", .max_items = 25,
    .description = "Most recent docket activity mentioning an entity, newest first — "
      "the monitoring view: new complaints, new motions and new appearances as they "
      "enter RECAP" },

  { .id = "IE_COURTS_SEARCH", .name = "Courts Service of Ireland — legal diary & judgments",
    .name_ja = "アイルランド 裁判所検索", .category = "legal",
    .portal = "https://www.courts.ie", .record_type = "ie-judgment",
    .tags = "\"ie\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.courts.ie/search/judgments/%22{q}%22",
    .href_must = "/acc/", .base = "https://www.courts.ie", .max_items = 25,
    .description = "Irish written judgments mentioning an entity — the counterpart to "
      "CRO filings for Irish holding companies and their directors" },

  { .id = "NZ_LEGAL_SEARCH", .name = "NZLII — New Zealand case law",
    .name_ja = "ニュージーランド 判例(NZLII)", .category = "legal",
    .portal = "https://www.nzlii.org", .record_type = "nz-judgment",
    .tags = "\"nz\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.nzlii.org/cgi-bin/sinosrch.cgi?method=auto&query={q}",
    .href_must = "/cases/", .base = "https://www.nzlii.org", .max_items = 25,
    .description = "New Zealand court and tribunal decisions naming an entity — "
      "completes the NZ picture alongside the fully public Companies Office record" },
};

HP_REGISTER_TABLE(HP_COURTS)
