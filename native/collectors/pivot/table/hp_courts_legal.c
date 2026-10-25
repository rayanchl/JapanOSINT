/* collectors/sources/hp_courts_legal.c — court dockets and legal-record depth.
 *
 * A case name is not a case. CourtListener alone exposes dockets, the parties
 * and attorneys on them, the individual RECAP documents, judge biographies and
 * judges' financial disclosures — five different records behind one search hit,
 * all free. The rest of these rows cover the same idea in other jurisdictions:
 * ECHR's HUDOC, CanLII, AustLII, BAILII, the US Federal Register and GovInfo,
 * WIPO domain-name panel decisions, and the US bankruptcy/appellate surface. */
#include "lib/hpengine.h"

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
    .date_keys = "date_filed",
    .next_path = "next",
    .description = "US federal and state dockets whose case name contains the "
      "entity — court, docket number, nature of suit, cause, judge assigned and "
      "filing/termination dates" },

  { .id = "CL_PARTIES", .name = "CourtListener — parties & counsel",
    .name_ja = "CourtListener — 当事者と代理人", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-court-party",
    .tags = "\"us\",\"courts\",\"parties\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1, .url = CL_BASE "/parties/?name__icontains={q}&page_size=40",
    .array_path = "results", .title_keys = "name", .id_keys = "id",
    .next_path = "next",
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
    .id_keys = "id", .date_keys = "dateFiled",
    .next_path = "next",
    .description = "Full-text search across PACER documents archived in RECAP — the "
      "actual complaints, motions and exhibits mentioning an entity, with docket "
      "entry numbers and document links" },

  { .id = "CL_OPINIONS", .name = "CourtListener — opinions full text",
    .name_ja = "CourtListener — 判決本文検索", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-opinion",
    .tags = "\"us\",\"courts\",\"opinions\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1, .url = CL_BASE "/search/?q={q}&type=o&order_by=dateFiled%20desc",
    .array_path = "results", .title_keys = "caseName", .id_keys = "id",
    .date_keys = "dateFiled",
    .next_path = "next",
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
    .date_keys = "year",
    .next_path = "next",
    .description = "US federal judges' annual financial disclosure filings — "
      "investments, gifts, reimbursements and outside income, which is what shows a "
      "judge's exposure to a party before them" },

  { .id = "CL_JUDGES", .name = "CourtListener — judge biographies",
    .name_ja = "CourtListener — 裁判官経歴", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-judge",
    .tags = "\"us\",\"courts\",\"people\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1, .url = CL_BASE "/people/?name_last__icontains={q}&page_size=25",
    .array_path = "results", .title_keys = "name_full,name_last", .id_keys = "id",
    .date_keys = "date_dob",
    .next_path = "next",
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
    .date_keys = "columns.judgementdate",
    .page_param = "start", .page_size = 40, .page_start = 0,
    .description = "European Court of Human Rights judgments and decisions naming an "
      "entity or applicant — application number, respondent state, conclusion and "
      "judgment date" },

  /* THE STANDING INDEX, as against the pivot above. `gov-echr-hudoc` read this
   * same API on a 6-hour schedule and stored NOTHING while transferring 222 KB
   * over 20 requests a run — the invisible nothing, found 2026-10-25 by giving
   * the registry sweep's EMITS_NOTHING verdict the byte count that was already
   * on the run line.
   *
   * The cause is the e-Gov law API's cause, a second instance on a different
   * continent: **every field of a HUDOC record is nested.** A record is
   * `{"columns": {"itemid": …, "docname": …, "appno": …}}` with no top-level
   * scalar, and VJSON's uid and title precedence lists are single-level by
   * design, so every record was dropped. The pivot above has keyed on
   * `columns.itemid` since it was written; a VJSON row cannot say that, which
   * is why the fix is a move rather than an edit. "Every field nested" is a
   * FAMILY, not a quirk — look for it whenever a row transfers a real body and
   * stores nothing.
   *
   * Measured 2026-10-25, one request each. `resultcount` is **232,481** —
   * the whole ECHR case-law database — and the engine reads it without a
   * change, because `declared_total()` lists `resultCount` and cJSON's
   * `cJSON_GetObjectItem` is case-insensitive, so the row discloses its
   * remainder on every run. `length` is honoured to 1,000; page 1 is
   * byte-stable across two reads and disjoint from page 2.
   *
   * The sort is honoured, and the CONTROL is what says so in a shape worth
   * knowing: `sort=frobnicate Ascending` returns an **empty result set** —
   * this API validates the sort field and refuses an unknown one by answering
   * nothing, where data.europa.eu's type-checks its key and discards the value
   * (CLAUDE.md, "a parameter that validates is not a parameter that works").
   * `kpdate Ascending` and `kpdate Descending` give different, plausible
   * heads. Ascending is required by the cycle: new judgments land at the TAIL
   * where a forward walk has not been yet.
   *
   * **AND A PAGE CYCLE CANNOT READ IT, because the offset has a hard cap that
   * is served as an EMPTY SUCCESS.** The first attempt did exactly that:
   * `page_param = "start"` with `page_max = 40` read 20 pages, 10,000 records,
   * and filed "this run completed the cycle" — a tidy falsehood, because
   * `start=9500&length=500` answers 500 rows while **`start=9900` answers 0
   * rows AND `resultcount: 0`**. So `start + length <= 10000` is the API's own
   * bound, the engine cannot tell it from the end of data, and the position
   * would have cleared every run while the row re-read the same first 10,000
   * for ever. That is the Japan Search ceiling and the ENA false-completion,
   * both in one row, and this file's rule for it is the remedy: when no offset
   * reaches the rest, slice the QUERY.
   *
   * `kpdate` is the axis, so this is a `window_days` row. Sizing it is a
   * measurement and the obvious choice was wrong: a ONE-YEAR window peaks at
   * **9,647 (2012)** against the 10,000 cap — 353 records of headroom, and an
   * unsampled year could exceed it. An overflowed window is not a deferral
   * here, because the cap means no later run can reach past 10,000 of that
   * window either, so it is lost rather than late. Half-year windows peak near
   * 5,000, which is half the cap: 136 windows from 1959, caught up in about a
   * month at six-hourly, then pinned to the trailing window, which is what
   * makes one row both the backfill and the live feed.
   *
   * **The known gap is 11 records and is stated rather than hidden**, because
   * a date predicate is only exhaustive where every record is dated: the
   * collection answers 232,481 with no predicate and **232,470** to
   * `kpdate:[1900 TO 2040]`, so eleven documents carry no judgment date and no
   * window will ever reach them. Measured, not assumed — that is the check
   * this file asks for before windowing a row. */
  { .id = "ECHR_HUDOC_INDEX", .name = "ECHR HUDOC — the whole case-law index",
    .name_ja = "欧州人権裁判所 判例DB（全件）", .category = "legal",
    .portal = "https://hudoc.echr.coe.int", .record_type = "echr-judgment",
    .tags = "\"eu\",\"courts\",\"human-rights\",\"echr\"", .free_tier = 1,
    .url = "https://hudoc.echr.coe.int/app/query/results?query=contentsitename%3AECHR"
           "%20AND%20kpdate%3A%5B{wstart}T00%3A00%3A00Z%20TO%20{wend}T00%3A00%3A00Z%5D"
           "&select=itemid,docname,appno,kpdate,doctype,originatingbody"
           "&sort=kpdate%20Ascending&start=0&length=500",
    .array_path = "results",
    .title_keys = "columns.docname", .id_keys = "columns.itemid",
    .date_keys = "columns.kpdate",
    .interval = 21600,
    .window_days = 182, .window_from = "1959-01-01",
    .page_param = "start", .page_size = 500, .page_zero_based = 1,
    .page_max = 20,   /* 10,000 a window against a ~5,000 peak half-year, and the API's own start+length<=10000 cap */
    .description = "Every judgment and decision in the European Court of Human "
      "Rights' HUDOC database — item id, case name, application number, "
      "judgment date, document type and the originating body — read as "
      "half-year slices of judgment date, walking forward from 1959 and then "
      "pinning to the current window. 232,481 documents when measured on "
      "2026-10-25, of which 11 carry no date and no window reaches" },

  { .id = "CANLII_SEARCH", .name = "CanLII — Canadian case law & legislation",
    .name_ja = "カナダ 判例(CanLII)", .category = "legal",
    .portal = "https://www.canlii.org", .record_type = "ca-judgment",
    .tags = "\"ca\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.canlii.org/en/#search/text={q}",
    .href_must = "/doc/", .base = "https://www.canlii.org",
    .description = "Canadian federal and provincial court and tribunal decisions "
      "naming an entity. CanLII's JSON API needs a key; the public search surface "
      "is used here and yields real document links or nothing" },

  { .id = "AUSTLII_SEARCH", .name = "AustLII — Australian case law",
    .name_ja = "オーストラリア 判例(AustLII)", .category = "legal",
    .portal = "https://www.austlii.edu.au", .record_type = "au-judgment",
    .tags = "\"au\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.austlii.edu.au/cgi-bin/sinosrch.cgi?method=auto&query={q}",
    .href_must = "/cases/", .base = "https://www.austlii.edu.au",
    .description = "Australian judgments, tribunal decisions and legislation "
      "mentioning an entity — the full-text corpus behind ASIC's thin public "
      "company data" },

  { .id = "BAILII_SEARCH", .name = "BAILII — UK & Irish case law",
    .name_ja = "英・愛 判例(BAILII)", .category = "legal",
    .portal = "https://www.bailii.org", .record_type = "uk-judgment",
    .tags = "\"uk\",\"ie\",\"courts\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.bailii.org/cgi-bin/lucy_search_1.cgi?query={q}&method=boolean",
    .href_must = "/cases/", .base = "https://www.bailii.org",
    .description = "UK and Irish judgments naming an entity — the litigation record "
      "behind a Companies House profile, including winding-up and fraud proceedings" },

  { .id = "US_FEDERAL_REGISTER", .name = "Federal Register — US agency documents",
    .name_ja = "米国官報 — 連邦公報検索", .category = "government",
    .portal = "https://www.federalregister.gov", .record_type = "us-fedreg-document",
    .tags = "\"us\",\"regulation\"", .free_tier = 1,
    .url = "https://www.federalregister.gov/api/v1/documents.json"
           "?conditions%5Bterm%5D={q}&per_page=40&order=newest",
    .array_path = "results", .title_keys = "title", .id_keys = "document_number",
    .date_keys = "publication_date",
    .page_param = "page", .page_start = 1,
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
    .date_keys = "dateIssued",
    .description = "Congressional hearings and reports, US Courts opinions, the "
      "Congressional Record, CFR and public laws mentioning an entity — one index "
      "over the whole federal publication corpus" },

  { .id = "US_PACER_RSS_COURTS", .name = "CourtListener — recent PACER activity feed",
    .name_ja = "米国連邦裁判所 最新提出", .category = "legal",
    .portal = "https://www.courtlistener.com", .record_type = "us-court-filing",
    .tags = "\"us\",\"courts\",\"realtime\"", .key_env = CL_KEY, .headers = CL_AUTH,
    .free_tier = 1,
    .url = CL_BASE "/search/?q={q}&type=r&order_by=entry_date_filed%20desc&page_size=25",
    .array_path = "results", .title_keys = "caseName,short_description",
    .id_keys = "id", .date_keys = "entry_date_filed",
    .next_path = "next",
    .description = "Most recent docket activity mentioning an entity, newest first — "
      "the monitoring view: new complaints, new motions and new appearances as they "
      "enter RECAP" },

  { .id = "NZ_LEGAL_SEARCH", .name = "NZLII — New Zealand case law",
    .name_ja = "ニュージーランド 判例(NZLII)", .category = "legal",
    .portal = "https://www.nzlii.org", .record_type = "nz-judgment",
    .tags = "\"nz\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.nzlii.org/cgi-bin/sinosrch.cgi?method=auto&query={q}",
    .href_must = "/cases/", .base = "https://www.nzlii.org",
    .description = "New Zealand court and tribunal decisions naming an entity — "
      "completes the NZ picture alongside the fully public Companies Office record" },
};

HP_REGISTER_TABLE(HP_COURTS)
