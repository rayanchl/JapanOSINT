/* collectors/pivot/table/hp3b31_jpgov.c — batch 31: jpgov — Japanese government record depth.
 *
 * Japan's public record is unusually good and unusually under-used, because the
 * good parts are APIs with Japanese-only documentation sitting behind an
 * application-ID wall that most tooling never crosses. The corporate number
 * (法人番号) is the join key for all of it: gBizINFO hangs subsidies, government
 * contracts, certifications, patents and financial summaries off that one
 * number, and the NTA publishes the number-to-name mapping and the qualified
 * invoice issuer status against it.
 *
 * That is exactly the shape this engine is for — a list hit that names an
 * identifier, and a detail endpoint behind it that carries the substance. The
 * gBizINFO rows below are five separate second hops off the same corporate
 * number, so a single pivot on a Japanese company name returns its subsidy
 * history, its government contracts, its certifications and its patents rather
 * than just confirming that the company exists.
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

static const hp_source HP3B31_JPGOV[] = {
  /* ── The corporate number spine ───────────────────────────────────────── */
  { .id = "JP_NTA_HOUJIN_WEBAPI", .name = "NTA — corporate number (法人番号) Web-API",
    .name_ja = "国税庁 法人番号システムWeb-API", .category = "government",
    .portal = "https://www.houjin-bangou.nta.go.jp", .record_type = "jp-entity",
    .tags = "\"jp\",\"registry\",\"identity\"", .key_env = "NTA_HOUJIN_APP_ID",
    .free_tier = 1, .type = "api", .mode = HP_CSV,
    .url = "https://api.houjin-bangou.nta.go.jp/4/name?id={key}&name={q}"
      "&type=02&mode=2&history=1",
    .filter_query = 1, .title_keys = "col6,col9", .id_keys = "col2",
    .date_keys = "col3",
    .description = "The National Tax Agency's authoritative corporate-number "
      "register, requested with history=1 so name changes, address changes, "
      "mergers and closures are returned rather than only the current state. "
      "The 13-digit number is the join key for every other Japanese "
      "government dataset about a company" },

  { .id = "JP_NTA_INVOICE_ISSUER", .name = "NTA — qualified invoice issuer register",
    .name_ja = "国税庁 適格請求書発行事業者公表", .category = "government",
    .portal = "https://www.invoice-kohyo.nta.go.jp", .record_type = "jp-tax-registration",
    .tags = "\"jp\",\"tax\",\"identity\"", .key_env = "NTA_INVOICE_APP_ID",
    .want = HP_NUMERIC, .free_tier = 1,
    /* Registration numbers are "T" + 13 digits. {qd} keeps only the digits, so the
     * T is put back here: the row then works whether the analyst types it or not. */
    .url = "https://web-api.invoice-kohyo.nta.go.jp/1/num?id={key}&number=T{qd}&type=21",
    .array_path = "announcement", .title_keys = "name,address",
    .id_keys = "registratedNumber", .date_keys = "registrationDate",
    .description = "Whether a counterparty is a registered qualified invoice "
      "issuer under Japan's 2023 consumption-tax regime — registration number, "
      "the date it took effect, and any disqualification. A supplier missing "
      "from this register cannot pass on deductible tax" },

  { .id = "JP_GBIZINFO_COMPANY", .name = "gBizINFO — government-held company profile",
    .name_ja = "gBizINFO 法人情報", .category = "government",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-entity",
    .tags = "\"jp\",\"registry\",\"government\"", .key_env = "GBIZINFO_API_TOKEN",
    .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name,location",
    .id_keys = "corporate_number", .date_keys = "update_date",
    .page_param = "page", .page_max = 20,
    .description = "The Japanese government's consolidated view of a company — "
      "corporate number, registered location, capital, employee count, business "
      "items and the date each field was last confirmed by a ministry" },

  { .id = "JP_GBIZINFO_SUBSIDY", .name = "gBizINFO — subsidies received (補助金)",
    .name_ja = "gBizINFO 補助金情報", .category = "government",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-subsidy",
    .tags = "\"jp\",\"subsidy\",\"public-money\"", .key_env = "GBIZINFO_API_TOKEN",
    .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name", .id_keys = "corporate_number",
    .page_param = "page", .page_max = 20,
    .detail_url = "https://info.gbiz.go.jp/hojin/v1/hojin/{v}/subsidy",
    .detail_key = "corporate_number", .detail_path = "hojin-infos",
    .description = "Every government subsidy a named company has received — the "
      "granting ministry, the programme, the amount and the fiscal year. Public "
      "money into a private company, keyed to its corporate number" },

  { .id = "JP_GBIZINFO_PROCUREMENT", .name = "gBizINFO — government contracts won (調達)",
    .name_ja = "gBizINFO 調達情報", .category = "government",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-public-contract",
    .tags = "\"jp\",\"procurement\",\"public-money\"", .key_env = "GBIZINFO_API_TOKEN",
    .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name", .id_keys = "corporate_number",
    .page_param = "page", .page_max = 20,
    .detail_url = "https://info.gbiz.go.jp/hojin/v1/hojin/{v}/procurement",
    .detail_key = "corporate_number", .detail_path = "hojin-infos",
    .description = "Central government contracts awarded to a company — the "
      "procuring body, the contract name, the amount, the award date and "
      "whether it was competitively tendered or a single-source award" },

  { .id = "JP_GBIZINFO_CERTIFICATION", .name = "gBizINFO — certifications & commendations (認定)",
    .name_ja = "gBizINFO 表彰/認定情報", .category = "government",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-certification",
    .tags = "\"jp\",\"certification\",\"government\"", .key_env = "GBIZINFO_API_TOKEN",
    .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name", .id_keys = "corporate_number",
    .page_param = "page", .page_max = 20,
    .detail_url = "https://info.gbiz.go.jp/hojin/v1/hojin/{v}/certification",
    .detail_key = "corporate_number", .detail_path = "hojin-infos",
    .description = "Ministry certifications and commendations held by a company "
      "— management innovation plans, health-management certification, "
      "regional-revitalisation designations. These reveal which ministry has "
      "an active relationship with the firm" },

  { .id = "JP_GBIZINFO_PATENT", .name = "gBizINFO — patents held (特許)",
    .name_ja = "gBizINFO 特許情報", .category = "research",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-patent",
    .tags = "\"jp\",\"patent\",\"ip\"", .key_env = "GBIZINFO_API_TOKEN",
    .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name", .id_keys = "corporate_number",
    .page_param = "page", .page_max = 20,
    .detail_url = "https://info.gbiz.go.jp/hojin/v1/hojin/{v}/patent",
    .detail_key = "corporate_number", .detail_path = "hojin-infos",
    .description = "Patents and applications attributed to the corporate "
      "number — application number, title, classification and status, joined to "
      "the legal entity rather than to a free-text applicant string" },

  { .id = "JP_GBIZINFO_FINANCE", .name = "gBizINFO — financial summary (財務情報)",
    .name_ja = "gBizINFO 財務情報", .category = "finance",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-financials",
    .tags = "\"jp\",\"financials\"", .key_env = "GBIZINFO_API_TOKEN", .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name", .id_keys = "corporate_number",
    .page_param = "page", .page_max = 20,
    .detail_url = "https://info.gbiz.go.jp/hojin/v1/hojin/{v}/finance",
    .detail_key = "corporate_number", .detail_path = "hojin-infos",
    .description = "Net sales, operating profit, ordinary profit and net assets "
      "as filed with the government, per fiscal year — available for unlisted "
      "companies that publish nothing on EDINET" },

  /* ── Law, rulemaking and the courts ───────────────────────────────────── */
  { .id = "JP_EGOV_LAW_SEARCH", .name = "e-Gov — Japanese statute & ordinance API",
    .name_ja = "e-Gov 法令検索API", .category = "government",
    .portal = "https://laws.e-gov.go.jp", .record_type = "jp-legislation",
    .tags = "\"jp\",\"law\"", .free_tier = 1,
    .url = "https://laws.e-gov.go.jp/api/2/keyword?keyword={q}&limit=500",
    .array_path = "items", .title_keys = "law_info.law_title,revision_info.law_title",
    .id_keys = "law_info.law_id", .date_keys = "revision_info.amendment_promulgate_date",
    .page_param = "offset", .page_size = 500, .page_start = 0, .page_max = 20,
    .description = "Full-text search across every Japanese act, cabinet order "
      "and ministerial ordinance in force, returning the law ID, the title and "
      "the amendment history — the operative text, not a commentary on it" },

  { .id = "JP_KANPO_DAILY_CONTENTS",
    .name = "Kanpō — official gazette, each day's table of contents",
    .name_ja = "官報 日次目次（直近90日）", .category = "government",
    .portal = "https://www.kanpo.go.jp/", .record_type = "jp-gazette-notice",
    .tags = "\"jp\",\"gazette\",\"insolvency\",\"notice\",\"kanpo\"",
    .mode = HP_HTML, .want = HP_ANY, .free_tier = 1,
    .url = "https://www.kanpo.go.jp/{date:%Y%m%d}/{date:%Y%m%d}.fullcontents.html",
    /* No `base`: the hrefs are relative to the DAY'S DIRECTORY, and with base
     * unset the engine resolves them against the page it fetched, which is
     * that directory. Declaring the host instead (the obvious thing, and the
     * first thing tried) resolved every notice to
     * /20261022h01813/… — a 404 for all 25 of them, with the record count and
     * the run line perfectly healthy. */
    .href_must = "f.html",
    .date_back = 4, .date_step = "d",
    .date_field = "kanpo_date",
    .interval = 21600,
    .description = "Every notice in the Japanese official gazette for one day, "
      "as the gazette's own table of contents lists it: the notice's full "
      "title, the ministry or agency that issued it and its notice number, and "
      "the page of the issue it appears on. The main edition, the extra, the "
      "government-procurement edition and any special extra are all listed "
      "together. Ministerial ordinances, public-notice amendments, company "
      "dissolutions, capital reductions, merger announcements and bankruptcy "
      "commencements are all published here first." },

    /* ── Licensing and sectoral supervision ───────────────────────────────── */

  { .id = "JP_MLIT_LAND_TRADE_PRICES", .name = "MLIT — actual land transaction prices",
    .name_ja = "国土交通省 不動産取引価格情報", .category = "government",
    .portal = "https://www.land.mlit.go.jp", .record_type = "jp-land-transaction",
    .tags = "\"jp\",\"property\",\"prices\"", .free_tier = 1,
    .url = "https://www.land.mlit.go.jp/webland/api/TradeListSearch?from=20051&to=20244&area={qd}",
    .array_path = "data", .title_keys = "Municipality,DistrictName",
    .id_keys = "Period+MunicipalityCode+DistrictName+TradePrice+Area+BuildingYear", .date_keys = "Period",
    /* id_keys composite (+ composes, , chooses): MLIT publishes no transaction id, so identity is the discriminating tuple;
     * `Municipality` alone kept one sale per city. */
    /* Not paged: from=/to= are the quarter range (2005Q1-2024Q4), and the API
     * returns every transaction in the range for the area in one response. */
    .description = "Surveyed actual transaction prices for land and buildings — "
      "price, area, structure, building year, city planning zone and frontage "
      "road, by municipality and quarter. Requests the full history rather than "
      "a single quarter so a district's price series is complete" },

  { .id = "JP_PMDA_NEW_DRUG_APPROVALS",
    .name = "PMDA — new drug approvals, this fiscal year's monthly register",
    .name_ja = "PMDA 新医薬品承認品目一覧（年度・月別）", .category = "health",
    .portal = "https://www.pmda.go.jp/review-services/drug-reviews/"
              "review-information/p-drugs/0010.html",
    .record_type = "jp-medical-approval",
    .tags = "\"jp\",\"health\",\"regulatory\",\"pmda\",\"drug-approval\"",
    .mode = HP_XLSX, .want = HP_ANY, .free_tier = 1,
    .url = "https://www.pmda.go.jp/files/000281578.xlsx",
    .index_url = "https://www.pmda.go.jp/review-services/drug-reviews/"
                 "review-information/p-drugs/0040.html",
    .index_href_must = ".xlsx",
    .xlsx_sheet_all = 1,
    .csv_no_header = 1,
    .csv_skip_lines = 5,
    .interval = 604800,
    .title_keys = "col3",
    .id_keys = "col1+col2+col3",
    .description = "Every new drug PMDA approved this Japanese fiscal year, as "
      "the agency publishes it: the review division, the approval date, the "
      "month's sequence number, the brand name with the marketing "
      "authorisation holder and its corporate number, whether the entry is a "
      "first approval (承認) or a partial change (一変), the active ingredient "
      "with new active ingredients underlined, and the indication granted. One "
      "worksheet per month, all of them read. The corporate number in the "
      "brand-name cell is the join to every other Japanese company register in "
      "this tree, which makes this the authorisation side of a pharmaceutical "
      "company's record." },

  { .id = "JP_TDNET_TIMELY_DISCLOSURE", .name = "TDnet — Japanese listed company timely disclosure",
    .name_ja = "TDnet 適時開示情報", .category = "finance",
    .portal = "https://www.release.tdnet.info", .record_type = "jp-listed-disclosure",
    .tags = "\"jp\",\"listed\",\"disclosure\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.release.tdnet.info/inbs/I_main_00.html?q={q}",
    .base = "https://www.release.tdnet.info", .filter_query = 1,
    .description = "The Tokyo Stock Exchange timely disclosure network — "
      "earnings revisions, share buybacks, subsidiary transfers, third-party "
      "allotments, MBO announcements and the internal-control failures a "
      "listed company must report within the day it occurs" },

  /* ── Public money and procurement ─────────────────────────────────────── */
  { .id = "JP_ESTAT_STATSLIST", .name = "e-Stat — official statistics catalogue API",
    .name_ja = "e-Stat 統計表情報API", .category = "government",
    .portal = "https://www.e-stat.go.jp", .record_type = "jp-statistic",
    .tags = "\"jp\",\"statistics\",\"open-data\"", .key_env = "ESTAT_APP_ID",
    .free_tier = 1,
    .url = "https://api.e-stat.go.jp/rest/3.0/app/json/getStatsList?appId={key}"
      "&searchWord={q}&limit=100000",
    .array_path = "GET_STATS_LIST.DATALIST_INF.TABLE_INF",
    .title_keys = "STATISTICS_NAME,TITLE.$", .id_keys = "@id",
    .date_keys = "UPDATED_DATE",
    /* limit=100000 is the API maximum per response; when more exist e-Stat says
     * so with RESULT_INF.NEXT_KEY, which is the startPosition to ask for next. */
    .next_path = "GET_STATS_LIST.DATALIST_INF.RESULT_INF.NEXT_KEY",
    .next_tmpl = "https://api.e-stat.go.jp/rest/3.0/app/json/getStatsList?appId={key}&searchWord={q}&limit=100000&startPosition={v}",
    .description = "The index of every official Japanese statistical table — "
      "which ministry produces it, the survey behind it, the geographic and "
      "time coverage, and the table ID needed to pull the actual figures" },

  { .id = "JP_NDL_SEARCH_API", .name = "National Diet Library — search OpenSearch API",
    .name_ja = "国立国会図書館サーチAPI", .category = "research",
    .portal = "https://ndlsearch.ndl.go.jp", .record_type = "jp-bibliographic",
    .tags = "\"jp\",\"library\",\"archive\"", .free_tier = 1,
    .type = "api", .mode = HP_HTML,
    .url = "https://ndlsearch.ndl.go.jp/api/opensearch?any={q}&cnt=200",
    .base = "https://ndlsearch.ndl.go.jp", .filter_query = 1,
    .description = "The National Diet Library's union catalogue — books, "
      "periodicals, official gazettes, government reports and digitised "
      "collections held across Japanese public libraries, with holdings "
      "location for each hit" },

};

HP_REGISTER_TABLE(HP3B31_JPGOV)
