/* collectors/pivot/table/hp3b31_apacgov.c — batch 31: apacgov — Asia-Pacific public record.
 *
 * `hp2_asia_*.c` covers Asian jurisdictions through their corporate and
 * regulatory registers. This file covers the same jurisdictions through the
 * *state's own accountability machinery*: who won the tender, who lobbied the
 * minister, who donated to the party, which court ruled, which charity is
 * registered, and which dataset the government has quietly published.
 *
 * Australia and New Zealand publish this exceptionally well and Indonesia's
 * Supreme Court publishes every judgment in full text, which makes the region
 * far more penetrable than its reputation suggests.
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

static const hp_source HP3B31_APACGOV[] = {
  /* ── Australia ────────────────────────────────────────────────────────── */
  { .id = "AU_AUSTENDER_CONTRACTS", .name = "AusTender — Australian government contract notices",
    .name_ja = "豪州 政府調達契約", .category = "government",
    .portal = "https://www.tenders.gov.au", .record_type = "au-contract",
    .tags = "\"au\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.tenders.gov.au/Search/CnSearch?keyword={q}",
    .base = "https://www.tenders.gov.au", .filter_query = 1,
    .description = "Every Australian Commonwealth contract notice above the "
      "reporting threshold — the agency, the supplier and its ABN, the value, "
      "the procurement method and whether it was a limited tender" },

  { .id = "AU_LOBBYIST_REGISTER", .name = "Australia — federal register of lobbyists",
    .name_ja = "豪州 連邦ロビイスト登録", .category = "government",
    .portal = "https://lobbyists.ag.gov.au", .record_type = "au-lobbyist",
    .tags = "\"au\",\"lobbying\",\"influence\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://lobbyists.ag.gov.au/register?search={q}",
    .base = "https://lobbyists.ag.gov.au", .filter_query = 1,
    .description = "Third-party lobbyists registered with the Attorney-General's "
      "Department — the firm, its owners and employees, each client it acts "
      "for, and whether a lobbyist is a former government representative" },

  { .id = "AU_ASIC_BANNED_DISQUALIFIED", .name = "ASIC — banned and disqualified persons",
    .name_ja = "豪州ASIC 資格停止者名簿", .category = "government",
    .portal = "https://connectonline.asic.gov.au", .record_type = "au-disqualification",
    .tags = "\"au\",\"enforcement\",\"director\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://connectonline.asic.gov.au/RegistrySearch/faces/landing/"
      "bannedAndDisqualified.jspx?searchText={q}",
    .base = "https://connectonline.asic.gov.au", .filter_query = 1,
    .description = "People banned from managing corporations or from providing "
      "financial services — the ground, the period and the ASIC decision. The "
      "single most consequential adverse record about an Australian director" },

  { .id = "AU_ACNC_CHARITY_REGISTER", .name = "Australia ACNC — charity register",
    .name_ja = "豪州 慈善団体登録", .category = "government",
    .portal = "https://www.acnc.gov.au", .record_type = "au-charity",
    .tags = "\"au\",\"charity\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.acnc.gov.au/charity/charities?search={q}",
    .base = "https://www.acnc.gov.au", .filter_query = 1,
    .description = "Registered Australian charities — the responsible persons "
      "(a board list most jurisdictions do not publish for charities), the "
      "annual information statement, revenue, employee count and any "
      "enforcement action or revocation" },

  { .id = "AU_PARLINFO_HANSARD", .name = "Australia ParlInfo — Hansard & tabled papers",
    .name_ja = "豪州議会 議事録検索", .category = "government",
    .portal = "https://parlinfo.aph.gov.au", .record_type = "au-parliamentary-record",
    .tags = "\"au\",\"parliament\",\"oversight\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://parlinfo.aph.gov.au/parlInfo/search/summary/summary.w3p;query={q}",
    .base = "https://parlinfo.aph.gov.au", .filter_query = 1,
    .description = "Hansard, committee evidence, tabled papers and answers to "
      "questions on notice — Senate estimates in particular extracts contract "
      "and incident detail that agencies do not otherwise publish" },

  { .id = "AU_DATA_GOV_CKAN", .name = "data.gov.au — Australian dataset catalog",
    .name_ja = "豪州オープンデータ目録", .category = "government",
    .portal = "https://data.gov.au", .record_type = "au-dataset",
    .tags = "\"au\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://data.gov.au/data/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Australia's federal, state and territory data catalog — "
      "the register extracts, licence lists and incident datasets that agencies "
      "publish as files rather than as searchable services" },

  /* ── New Zealand ──────────────────────────────────────────────────────── */
  { .id = "NZ_GETS_TENDERS", .name = "New Zealand GETS — government tenders",
    .name_ja = "NZ 政府調達入札", .category = "government",
    .portal = "https://www.gets.govt.nz", .record_type = "nz-tender",
    .tags = "\"nz\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.gets.govt.nz/ExternalIndex.htm?keyword={q}",
    .base = "https://www.gets.govt.nz", .filter_query = 1,
    .description = "The Government Electronic Tenders Service — every open "
      "opportunity and contract award notice across New Zealand central and "
      "local government, with the agency, category and award value" },

  { .id = "NZ_DATA_CKAN", .name = "data.govt.nz — New Zealand dataset catalog",
    .name_ja = "NZ オープンデータ目録", .category = "government",
    .portal = "https://catalogue.data.govt.nz", .record_type = "nz-dataset",
    .tags = "\"nz\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://catalogue.data.govt.nz/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "New Zealand's CKAN catalog across agencies and councils — "
      "consent registers, contaminated-site lists, fisheries and biosecurity "
      "data, and the public-sector spending extracts" },

  { .id = "NZ_PARLIAMENT_PECUNIARY", .name = "New Zealand Parliament — pecuniary interests & papers",
    .name_ja = "NZ議会 資産利益登録", .category = "government",
    .portal = "https://www.parliament.nz", .record_type = "nz-member-interest",
    .tags = "\"nz\",\"parliament\",\"conflict-of-interest\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.parliament.nz/en/pb/sc/submissions-and-advice/search?Criteria.Keyword={q}",
    .base = "https://www.parliament.nz", .filter_query = 1,
    .description = "Select committee submissions, advice and the register of "
      "members' pecuniary interests — company directorships, shareholdings, "
      "trusts, gifts and overseas travel paid for by third parties" },

  /* ── Korea, Taiwan, Hong Kong ─────────────────────────────────────────── */
  { .id = "KR_ALIO_PUBLIC_INSTITUTIONS", .name = "Korea ALIO — public institution disclosure",
    .name_ja = "韓国 公共機関経営情報公開", .category = "government",
    .portal = "https://www.alio.go.kr", .record_type = "kr-public-institution",
    .tags = "\"kr\",\"state-owned\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.alio.go.kr/item/itemOrganList.do?searchWord={q}",
    .base = "https://www.alio.go.kr", .filter_query = 1,
    .description = "Korea's mandatory disclosure system for public "
      "institutions — executives and their appointment route, headcount, "
      "salaries, debt, subsidiary holdings and audit findings for every "
      "state-owned and quasi-governmental body" },

  { .id = "TW_LEGISLATIVE_YUAN_DATA", .name = "Taiwan Legislative Yuan — open data API",
    .name_ja = "台湾立法院 オープンデータ", .category = "government",
    .portal = "https://data.ly.gov.tw", .record_type = "tw-parliamentary-record",
    .tags = "\"tw\",\"parliament\",\"legislation\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://data.ly.gov.tw/getds.action?id=1&keyword={q}",
    .base = "https://data.ly.gov.tw", .filter_query = 1,
    .description = "The Legislative Yuan's own data service — bill texts, "
      "committee records, interpellations and legislators' declared assets and "
      "outside positions, published as bulk data rather than as web pages" },

  { .id = "TW_DATA_GOV_CATALOG", .name = "Taiwan data.gov.tw — dataset catalog",
    .name_ja = "台湾 政府資料開放平臺", .category = "government",
    .portal = "https://data.gov.tw", .record_type = "tw-dataset",
    .tags = "\"tw\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://data.gov.tw/api/front/dataset/search?qs={q}&size=100",
    .array_path = "result.result", .title_keys = "title,organization",
    .id_keys = "id", .date_keys = "modified",
    .page_param = "page", .page_max = 30,
    .description = "Taiwan's national open data platform — the food-safety "
      "inspection, company registration extract, air-quality and public-works "
      "datasets published by each ministry, with the API endpoint for each" },

  { .id = "HK_DATA_GOV_CATALOG", .name = "Hong Kong DATA.GOV.HK — dataset catalog",
    .name_ja = "香港 公共データ目録", .category = "government",
    .portal = "https://data.gov.hk", .record_type = "hk-dataset",
    .tags = "\"hk\",\"open-data\",\"catalog\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://data.gov.hk/en/search?q={q}",
    .base = "https://data.gov.hk", .filter_query = 1,
    .description = "Hong Kong's public data catalog — licensing registers, "
      "building records, tender awards and transport datasets published by "
      "each bureau and department" },

  { .id = "HK_GLD_TENDER_AWARDS", .name = "Hong Kong — government tender notices & awards",
    .name_ja = "香港 政府入札公告", .category = "government",
    .portal = "https://www.gld.gov.hk", .record_type = "hk-tender",
    .tags = "\"hk\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.gld.gov.hk/eng/tenders_notices.htm?q={q}",
    .base = "https://www.gld.gov.hk", .filter_query = 1,
    .description = "Government Logistics Department tender notices and the "
      "gazetted award results — the department, the contract, the successful "
      "bidder and the contract sum" },

  /* ── Southeast and South Asia ─────────────────────────────────────────── */
  { .id = "ID_SUPREME_COURT_JUDGMENTS", .name = "Indonesia — Supreme Court judgment directory",
    .name_ja = "インドネシア最高裁 判決検索", .category = "legal",
    .portal = "https://putusan3.mahkamahagung.go.id", .record_type = "id-judgment",
    .tags = "\"id\",\"courts\",\"judgment\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://putusan3.mahkamahagung.go.id/search.html?q={q}",
    .base = "https://putusan3.mahkamahagung.go.id", .filter_query = 1,
    .description = "Indonesia publishes essentially every court decision in "
      "full text, from district courts to the Supreme Court — parties, case "
      "number, the panel, the charge or claim and the ruling. One of the "
      "deepest court archives in Asia and almost entirely unindexed" },

  { .id = "ID_LKPP_BLACKLIST", .name = "Indonesia LKPP — procurement blacklist",
    .name_ja = "インドネシア 調達ブラックリスト", .category = "government",
    .portal = "https://inaproc.id", .record_type = "id-debarment",
    .tags = "\"id\",\"procurement\",\"debarment\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://inaproc.id/daftar-hitam?search={q}",
    .base = "https://inaproc.id", .filter_query = 1,
    .description = "Companies barred from Indonesian public procurement — the "
      "agency that imposed the sanction, the ground, and the start and end of "
      "the blacklisting period" },

  { .id = "ID_SATU_DATA_CATALOG", .name = "Indonesia Satu Data — national data catalog",
    .name_ja = "インドネシア 統合データ目録", .category = "government",
    .portal = "https://data.go.id", .record_type = "id-dataset",
    .tags = "\"id\",\"open-data\",\"catalog\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://data.go.id/dataset?q={q}",
    .base = "https://data.go.id", .filter_query = 1,
    .description = "Indonesia's one-data portal — ministry and provincial "
      "datasets covering permits, plantations, mining concessions and health "
      "facilities, with the publishing agency for each" },

  { .id = "TH_OPEN_DATA_CKAN", .name = "Thailand data.go.th — dataset catalog",
    .name_ja = "タイ オープンデータ目録", .category = "government",
    .portal = "https://data.go.th", .record_type = "th-dataset",
    .tags = "\"th\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://data.go.th/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Thailand's national CKAN catalog — provincial budget, "
      "licensing, land-use and public-health datasets published by ministries "
      "and provincial administrations" },

  { .id = "PH_OPEN_DATA_CATALOG", .name = "Philippines data.gov.ph — dataset catalog",
    .name_ja = "フィリピン オープンデータ目録", .category = "government",
    .portal = "https://data.gov.ph", .record_type = "ph-dataset",
    .tags = "\"ph\",\"open-data\",\"catalog\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://data.gov.ph/search?q={q}",
    .base = "https://data.gov.ph", .filter_query = 1,
    .description = "The Philippine open data portal — agency budgets, "
      "infrastructure project lists, disaster response data and the local "
      "government unit datasets that carry project-level spending" },

  { .id = "IN_CPGRAMS_ENFORCEMENT", .name = "India — Ministry of Corporate Affairs prosecutions",
    .name_ja = "インド企業省 訴追情報", .category = "government",
    .portal = "https://www.mca.gov.in", .record_type = "in-enforcement",
    .tags = "\"in\",\"enforcement\",\"corporate\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.mca.gov.in/content/mca/global/en/search.html?q={q}",
    .base = "https://www.mca.gov.in", .filter_query = 1,
    .description = "Ministry of Corporate Affairs notices — struck-off company "
      "lists, disqualified director notifications, prosecution and adjudication "
      "orders, and the vanishing-company list" },

  { .id = "PACIFIC_ISLAND_REGISTRIES", .name = "Pacific Islands — company & business registries",
    .name_ja = "太平洋島嶼国 法人登記", .category = "government",
    .portal = "https://www.pacificdata.org", .record_type = "pacific-entity",
    .tags = "\"pacific\",\"registry\",\"offshore\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://pacificdata.org/data/dataset?q={q}",
    .base = "https://pacificdata.org", .filter_query = 1,
    .description = "The Pacific Data Hub, which aggregates registry, fisheries "
      "licensing and revenue datasets for the Pacific Island states — the "
      "jurisdictions used for flag-of-convenience shipping and fisheries "
      "licences that have no other searchable presence" },
};

HP_REGISTER_TABLE(HP3B31_APACGOV)
