/* collectors/sources/hp2_asia_east.c — East Asia critical-value records.
 *
 * "Critical value" here means the record that changes a decision: the
 * blacklist a company is on, the court judgment against it, the licence that
 * lets it operate, the tender it won from the state. East Asia publishes a
 * surprising amount of exactly that — Taiwan's judiciary and procurement, the
 * Credit China discredited-entity lists, Korea's business-status and tender
 * APIs, Hong Kong's SFC licensee register — and almost none of it was reachable
 * from here before. */
#include "../../lib/hpengine.h"

static const hp_source HP2_ASIA_EAST[] = {
  /* ── Korea ─────────────────────────────────────────────────────────────── */
  { .id = "KR_NTS_BUSINESS_STATUS", .name = "Korea NTS — business registration status",
    .name_ja = "韓国国税庁 — 事業者登録状態照会", .category = "government",
    .portal = "https://www.nts.go.kr", .record_type = "kr-business-status",
    .tags = "\"kr\",\"tax\",\"status\"", .want = HP_NUMERIC,
    .key_env = "KR_ODCLOUD_KEY", .free_tier = 1,
    .url = "https://api.odcloud.kr/api/nts-businessman/v1/status?serviceKey={key}",
    .post_body = "{\"b_no\":[\"{qd}\"]}",
    .array_path = "data", .title_keys = "b_no,b_stt", .id_keys = "b_no",
    .description = "Whether a Korean business registration number is active, "
      "closed or suspended, with the tax-type code and closure date. The first "
      "question to ask about any Korean counterparty" },

  { .id = "KR_G2B_TENDERS", .name = "Korea G2B — public tender notices",
    .name_ja = "韓国 ナラ장터 入札公告", .category = "government",
    .portal = "https://www.g2b.go.kr", .record_type = "kr-tender",
    .tags = "\"kr\",\"procurement\"", .key_env = "KR_DATA_GO_KR_KEY", .free_tier = 1,
    .url = "https://apis.data.go.kr/1230000/BidPublicInfoService/getBidPblancListInfoServcPPSSrch"
           "?serviceKey={key}&type=json&numOfRows=100&pageNo=1&bidNtceNm={q}",
    .array_path = "response.body.items", .title_keys = "bidNtceNm",
    .id_keys = "bidNtceNo", .date_keys = "bidNtceDt",
    .page_param = "pageNo", .page_start = 1,
    .description = "Korean public tender notices matching a term — contracting "
      "agency, estimated price, notice and closing dates, and the notice number "
      "that keys the award record" },

  { .id = "KR_KIPRIS_PATENTS", .name = "Korea KIPRIS — patent & trademark search",
    .name_ja = "韓国 特許情報(KIPRIS)", .category = "research",
    .portal = "https://www.kipris.or.kr", .record_type = "kr-patent",
    .tags = "\"kr\",\"patents\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://engdipl.kipris.or.kr/eng/search/total_search.do?searchWord={q}",
    .base = "https://engdipl.kipris.or.kr", .filter_query = 1,
    .description = "Korean patents, utility models, designs and trademarks by "
      "applicant or keyword — the IP position of a Korean manufacturer, which "
      "often names subsidiaries the corporate register does not" },

  { .id = "KR_DATA_GO_KR", .name = "Korea data.go.kr — dataset & register catalogue",
    .name_ja = "韓国 公共データ カタログ", .category = "government",
    .portal = "https://www.data.go.kr", .record_type = "kr-dataset",
    .tags = "\"kr\",\"opendata\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.data.go.kr/tcs/dss/selectDataSetList.do?keyword={q}",
    .base = "https://www.data.go.kr", .filter_query = 1,
    .description = "Korea's open-data catalogue — licensee registers, penalty "
      "lists, facility inventories and subsidy records published by each "
      "ministry, with the dataset ids needed to pull the rows" },

  /* ── Taiwan ────────────────────────────────────────────────────────────── */
  { .id = "TW_JUDICIAL_JUDGMENTS", .name = "Taiwan Judicial Yuan — court judgments",
    .name_ja = "台湾 司法院 判決検索", .category = "legal",
    .portal = "https://opendata.judicial.gov.tw", .record_type = "tw-judgment",
    .tags = "\"tw\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://judgment.judicial.gov.tw/FJUD/qryresultlst.aspx?judtype=JUDBOOK&kw={q}",
    .base = "https://judgment.judicial.gov.tw", .filter_query = 1,
    .description = "Taiwanese court judgments naming a party — Taiwan publishes "
      "full judgment text including company names, so litigation, fraud and "
      "labour cases are traceable to the entity" },

  { .id = "TW_PCC_TENDERS", .name = "Taiwan PCC — government procurement awards",
    .name_ja = "台湾 政府調達 落札公告", .category = "government",
    .portal = "https://web.pcc.gov.tw", .record_type = "tw-tender",
    .tags = "\"tw\",\"procurement\"", .free_tier = 1,
    .url = "https://pcc.g0v.ronny.tw/api/searchbytitle?query={q}",
    .array_path = "records", .title_keys = "brief.title,unit_name",
    .id_keys = "job_number", .date_keys = "date",
    .page_param = "page", .page_start = 1,
    .description = "Taiwanese tender awards and notices via the g0v mirror of the "
      "PCC bulletin — buying agency, award amount, winning supplier and the "
      "job number that keys the full notice" },

  { .id = "TW_GCIS_BRANCHES", .name = "Taiwan GCIS — branch offices",
    .name_ja = "台湾 商工登記 — 支店一覧", .category = "government",
    .portal = "https://data.gcis.nat.gov.tw", .record_type = "tw-establishment",
    .tags = "\"tw\",\"registry\",\"branches\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://data.gcis.nat.gov.tw/od/data/api/CA9B9BE9-3F5D-4EC1-BD0B-FB1FA33C1EBF"
           "?$format=json&$filter=Business_Accounting_NO%20eq%20{qd}",
    .title_keys = "Branch_Name,Company_Name", .id_keys = "Branch_Accounting_NO",
    .description = "Registered branch offices of a Taiwanese company — each with "
      "its own accounting number, address and responsible person; the operating "
      "footprint behind one headquarters registration" },

  { .id = "TW_IMPORT_EXPORT", .name = "Taiwan — importer/exporter registration",
    .name_ja = "台湾 輸出入業者登録", .category = "economy",
    .portal = "https://fbfh.trade.gov.tw", .record_type = "tw-trader",
    .tags = "\"tw\",\"trade\",\"customs\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://fbfh.trade.gov.tw/rich/text/indexfbOL.asp?keyword={q}",
    .base = "https://fbfh.trade.gov.tw", .filter_query = 1,
    .description = "Taiwan's register of licensed importers and exporters — "
      "English trading name, registration status and approval date; the link "
      "between a shipping document and a registered Taiwanese entity" },

  /* ── China ─────────────────────────────────────────────────────────────── */
  { .id = "CN_CREDIT_CHINA", .name = "Credit China — discredited entity lists",
    .name_ja = "信用中国 — 失信被執行者リスト", .category = "government",
    .portal = "https://www.creditchina.gov.cn", .record_type = "cn-blacklist",
    .tags = "\"cn\",\"blacklist\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.creditchina.gov.cn/xinyongxinxi/index.html?searchState=1&keyword={q}",
    .base = "https://www.creditchina.gov.cn", .filter_query = 1,
    .description = "China's national credit platform: the discredited-persons "
      "(失信被执行人) and administrative-penalty lists. Being on one restricts "
      "flights, bidding and borrowing — the highest-value single fact about a "
      "Chinese counterparty" },

  { .id = "CN_WENSHU_JUDGMENTS", .name = "China Judgements Online — court decisions",
    .name_ja = "中国 裁判文書網", .category = "legal",
    .portal = "https://wenshu.court.gov.cn", .record_type = "cn-judgment",
    .tags = "\"cn\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://wenshu.court.gov.cn/website/wenshu/181217BMTKHNT2W0/index.html?s21={q}",
    .base = "https://wenshu.court.gov.cn", .filter_query = 1,
    .description = "The Supreme People's Court judgment database. Heavily "
      "bot-protected, so a zero result is expected from a datacenter IP and is "
      "reported as such rather than as an absence of litigation" },

  { .id = "CN_CNIPA_PATENTS", .name = "CNIPA — Chinese patent search",
    .name_ja = "中国 特許検索(CNIPA)", .category = "research",
    .portal = "https://pss-system.cponline.cnipa.gov.cn", .record_type = "cn-patent",
    .tags = "\"cn\",\"patents\"", .free_tier = 1,
    .url = "https://api.patentsview.org/patents/query?q=%7B%22_contains%22%3A%7B%22assignee_organization%22%3A%22{q}%22%7D%7D",
    .array_path = "patents", .title_keys = "patent_title", .id_keys = "patent_number",
    .description = "Chinese-assignee patents seen from the US collection — CNIPA's "
      "own search is JS-only, but a Chinese company's US filings name the same "
      "assignee entity and are fully queryable" },

  { .id = "CN_MOFCOM_SANCTIONS", .name = "China MOFCOM — unreliable entity & export lists",
    .name_ja = "中国商務部 — 信頼できない企業リスト", .category = "government",
    .portal = "https://www.mofcom.gov.cn", .record_type = "cn-countersanction",
    .tags = "\"cn\",\"sanctions\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.mofcom.gov.cn/was5/web/search?channelid=226248&searchword={q}",
    .base = "https://www.mofcom.gov.cn", .filter_query = 1,
    .description = "Chinese counter-sanctions surface: the unreliable-entity list, "
      "export-control designations and anti-dumping determinations naming a firm "
      "— the mirror image of OFAC exposure for companies trading both ways" },

  /* ── Hong Kong & Macau ─────────────────────────────────────────────────── */
  { .id = "HK_SFC_LICENSEES", .name = "Hong Kong SFC — licensed persons & firms",
    .name_ja = "香港証券先物委員会 — 免許業者", .category = "economy",
    .portal = "https://www.sfc.hk", .record_type = "hk-licensee",
    .tags = "\"hk\",\"finance\",\"licences\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://apps.sfc.hk/publicregWeb/searchByName?searchText={q}",
    .base = "https://apps.sfc.hk", .filter_query = 1,
    .description = "Hong Kong's register of licensed corporations and responsible "
      "officers, including disciplinary history. A fund or broker's licence status "
      "is the fastest test of whether a Hong Kong finance entity is real" },

  { .id = "HK_HKEX_FILINGS", .name = "HKEXnews — listed company filings",
    .name_ja = "香港取引所 — 上場企業開示", .category = "economy",
    .portal = "https://www.hkexnews.hk", .record_type = "hk-filing",
    .tags = "\"hk\",\"filings\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www1.hkexnews.hk/search/titlesearch.xhtml?searchType=1&t1code=-1&stockId=-1&title={q}",
    .base = "https://www1.hkexnews.hk", .filter_query = 1,
    .description = "Filings by Hong Kong listed issuers — announcements, "
      "circulars, connected-transaction disclosures and shareholding changes; "
      "where a mainland group's offshore structure is actually described" },

  { .id = "HK_LAND_REGISTRY", .name = "Hong Kong Land Registry — property records",
    .name_ja = "香港 土地登記", .category = "economy",
    .portal = "https://www.landreg.gov.hk", .record_type = "hk-property",
    .tags = "\"hk\",\"property\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.iris.gov.hk/eservices/searchByAddress?keyword={q}",
    .base = "https://www.iris.gov.hk", .filter_query = 1,
    .description = "Hong Kong property ownership search — the register that ties "
      "a person or company to real property in one of the world's most-used "
      "asset-holding jurisdictions" },

  /* ── Japan (deep records the JP set did not reach) ──────────────────────── */
  { .id = "JP_MLIT_NEGATIVE_LIST", .name = "Japan MLIT — construction penalty & disposition list",
    .name_ja = "国交省 — 建設業者処分情報", .category = "government",
    .portal = "https://www.mlit.go.jp", .record_type = "jp-sanction",
    .tags = "\"jp\",\"enforcement\",\"construction\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.mlit.go.jp/nega-inf/cgi-bin/search.cgi?NAME={q}",
    .base = "https://www.mlit.go.jp", .filter_query = 1,
    .description = "Administrative dispositions against Japanese construction "
      "firms — business suspensions, licence revocations and the conduct behind "
      "them. Japan's public debarment surface for the construction sector" },

  { .id = "JP_FSA_ACTIONS", .name = "Japan FSA — administrative actions",
    .name_ja = "金融庁 — 行政処分", .category = "government",
    .portal = "https://www.fsa.go.jp", .record_type = "jp-enforcement",
    .tags = "\"jp\",\"finance\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.fsa.go.jp/cgi-bin/search.cgi?q={q}",
    .base = "https://www.fsa.go.jp", .filter_query = 1,
    .description = "Financial Services Agency administrative actions and warnings "
      "— unregistered operators, business-improvement orders and suspensions "
      "naming the firm and its officers" },

  { .id = "JP_JICA_CONTRACTS", .name = "Japan JICA — aid contracts & debarment",
    .name_ja = "JICA — 契約と資格停止", .category = "government",
    .portal = "https://www.jica.go.jp", .record_type = "jp-aid-contract",
    .tags = "\"jp\",\"aid\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.jica.go.jp/search/index.html?q={q}",
    .base = "https://www.jica.go.jp", .filter_query = 1,
    .description = "JICA's contract awards and its list of firms barred from "
      "bidding — the Japanese counterpart to World Bank debarment, and the money "
      "trail for Japanese contractors working overseas" },

  { .id = "JP_NITE_RECALLS", .name = "Japan NITE — product accident & recall records",
    .name_ja = "NITE — 製品事故情報", .category = "safety",
    .portal = "https://www.nite.go.jp", .record_type = "jp-product-accident",
    .tags = "\"jp\",\"recalls\",\"safety\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.jiko.nite.go.jp/php/jiko/keyword/index.php?kw={q}",
    .base = "https://www.jiko.nite.go.jp", .filter_query = 1,
    .description = "Japanese product-accident and recall records by maker or "
      "product — fires, injuries and the manufacturer's response; the safety "
      "record behind a consumer brand" },

  /* ── Mongolia & regional ───────────────────────────────────────────────── */
  { .id = "MN_COMPANY_REGISTER", .name = "Mongolia — legal entity register",
    .name_ja = "モンゴル 法人登記", .category = "government",
    .portal = "https://burtgel.gov.mn", .record_type = "mn-company",
    .tags = "\"mn\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://burtgel.gov.mn/search?name={q}",
    .base = "https://burtgel.gov.mn", .filter_query = 1,
    .description = "Mongolian General Authority for State Registration entity "
      "search — register number, status and legal form; the entry point for "
      "mining-licence ownership questions" },

  { .id = "MN_MINING_LICENCES", .name = "Mongolia — mineral licence cadastre",
    .name_ja = "モンゴル 鉱区ライセンス", .category = "industry",
    .portal = "https://mrpam.gov.mn", .record_type = "mn-mining-licence",
    .tags = "\"mn\",\"mining\",\"licences\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://mrpam.gov.mn/search/?q={q}",
    .base = "https://mrpam.gov.mn", .filter_query = 1,
    .description = "Mongolian exploration and mining licences — holder, area, "
      "mineral and validity. In a resource economy the licence register is the "
      "asset register" },

  { .id = "KR_COURT_REGISTRY", .name = "Korea Supreme Court — corporate registry search",
    .name_ja = "韓国 大法院 法人登記", .category = "government",
    .portal = "https://www.iros.go.kr", .record_type = "kr-company",
    .tags = "\"kr\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.iros.go.kr/PMainJ.jsp?searchWord={q}",
    .base = "https://www.iros.go.kr", .filter_query = 1,
    .description = "Korea's court-operated corporate registry (IROS) — the "
      "authoritative source for a Korean company's registration number, capital "
      "and directors, above the tax-side business number" },
};

HP_REGISTER_TABLE(HP2_ASIA_EAST)
