/* collectors/sources/hp_sanctions_deep.c — sanctions, PEP and debarment depth.
 *
 * The repo had name-level screening: "is this name on a list". That answers the
 * compliance question and almost none of the investigative one. These rows go
 * after the structure inside the listings — the aliases and addresses OFAC
 * publishes alongside each SDN entry, the linked relatives and associates in an
 * OpenSanctions PEP record, the specific programme and legal basis, the "wanted"
 * and "crime" datasets that are not sanctions at all, and the ransomware victim
 * lists that are the closest thing to a real-time exposure feed.
 *
 * The OpenSanctions rows share one credential (OPENSANCTIONS_API_KEY). Without
 * it they return an honest empty — the entity is not declared clean, it is
 * declared unchecked. */
#include "lib/hpengine.h"

#define OS_KEY  "OPENSANCTIONS_API_KEY"
#define OS_AUTH { "Authorization: ApiKey {key}", NULL }
#define OS_BASE "https://api.opensanctions.org"

static const hp_source HP_SANCTIONS[] = {
  { .id = "OS_ENTITY_DETAIL", .name = "OpenSanctions — entity record (nested)",
    .name_ja = "OpenSanctions — 対象者詳細", .category = "investigation",
    .portal = "https://www.opensanctions.org", .record_type = "sanctions-entity",
    .tags = "\"sanctions\",\"ftm\"", .key_env = OS_KEY, .headers = OS_AUTH,
    .free_tier = 1, .url = OS_BASE "/entities/{q}?nested=true",
    .title_keys = "caption", .id_keys = "id", .date_keys = "last_change",
    .description = "The full FollowTheMoney record behind an OpenSanctions entity "
      "id, expanded — nested relatives, associates, ownership links, addresses, "
      "identifiers, birth data and every source dataset asserting them" },

  { .id = "OS_MATCH_SCREEN", .name = "OpenSanctions — scored match screening",
    .name_ja = "OpenSanctions — 名寄せスコア照合", .category = "investigation",
    .portal = "https://www.opensanctions.org", .record_type = "sanctions-match",
    .tags = "\"sanctions\",\"screening\"", .key_env = OS_KEY, .headers = OS_AUTH,
    .free_tier = 1, .url = OS_BASE "/match/default?algorithm=best",
    .post_body = "{\"queries\":{\"q1\":{\"schema\":\"Thing\",\"properties\":"
                 "{\"name\":[\"{Q}\"]}}}}",
    .array_path = "responses.q1.results", .title_keys = "caption", .id_keys = "id",
    .description = "Fuzzy screening with match scores rather than substring hits — "
      "each candidate carries its score, features and the datasets it comes from, "
      "so near-miss transliterations surface instead of silently failing" },

  { .id = "OS_PEPS", .name = "OpenSanctions — politically exposed persons",
    .name_ja = "OpenSanctions — 重要な公的地位者(PEP)", .category = "investigation",
    .portal = "https://www.opensanctions.org", .record_type = "pep",
    .tags = "\"pep\",\"ftm\"", .key_env = OS_KEY, .headers = OS_AUTH,
    .free_tier = 1, .url = OS_BASE "/search/peps?q={q}&limit=25",
    .array_path = "results", .title_keys = "caption", .id_keys = "id",
    .description = "The structured PEP graph: office held, country, term dates and "
      "the family and business associates recorded alongside the official — the part "
      "a yes/no PEP flag throws away" },

  { .id = "OS_CRIME", .name = "OpenSanctions — criminal & enforcement records",
    .name_ja = "OpenSanctions — 犯罪・執行記録", .category = "investigation",
    .portal = "https://www.opensanctions.org", .record_type = "crime-record",
    .tags = "\"crime\",\"ftm\"", .key_env = OS_KEY, .headers = OS_AUTH,
    .free_tier = 1, .url = OS_BASE "/search/crime?q={q}&limit=25",
    .array_path = "results", .title_keys = "caption", .id_keys = "id",
    .description = "Convictions, indictments, asset forfeitures and enforcement "
      "actions collected from national authorities — a category distinct from "
      "sanctions that ordinary screening never looks at" },

  { .id = "OS_DEBARMENT", .name = "OpenSanctions — procurement debarment",
    .name_ja = "OpenSanctions — 調達除外リスト", .category = "investigation",
    .portal = "https://www.opensanctions.org", .record_type = "debarment",
    .tags = "\"debarment\",\"procurement\"", .key_env = OS_KEY, .headers = OS_AUTH,
    .free_tier = 1, .url = OS_BASE "/search/debarment?q={q}&limit=25",
    .array_path = "results", .title_keys = "caption", .id_keys = "id",
    .description = "Firms and individuals excluded from public contracting by the "
      "World Bank, ADB, AfDB, EBRD, IADB, EU EDES and national authorities — with "
      "the exclusion period and grounds" },

  { .id = "OS_WANTED", .name = "OpenSanctions — wanted persons",
    .name_ja = "OpenSanctions — 被指名手配者", .category = "investigation",
    .portal = "https://www.opensanctions.org", .record_type = "wanted-person",
    .tags = "\"wanted\",\"law-enforcement\"", .key_env = OS_KEY, .headers = OS_AUTH,
    .free_tier = 1, .url = OS_BASE "/search/wanted?q={q}&limit=25",
    .array_path = "results", .title_keys = "caption", .id_keys = "id",
    .description = "Wanted-person notices aggregated across Interpol, national "
      "police forces and prosecutors — charges, nationality, birth data and the "
      "issuing authority" },

  { .id = "OS_SANCTIONS_DATASET", .name = "OpenSanctions — consolidated sanctions",
    .name_ja = "OpenSanctions — 統合制裁リスト", .category = "investigation",
    .portal = "https://www.opensanctions.org", .record_type = "sanctions-entity",
    .tags = "\"sanctions\",\"ftm\"", .key_env = OS_KEY, .headers = OS_AUTH,
    .free_tier = 1, .url = OS_BASE "/search/sanctions?q={q}&limit=25",
    .array_path = "results", .title_keys = "caption", .id_keys = "id",
    .description = "The consolidated sanctions collection alone (OFAC, EU, UK, UN, "
      "CH, CA, AU, UA and more) — programme, listing date, authority and all "
      "recorded identifiers per designation" },

  { .id = "OS_STATEMENTS", .name = "OpenSanctions — raw source statements",
    .name_ja = "OpenSanctions — 出典ステートメント", .category = "investigation",
    .portal = "https://www.opensanctions.org", .record_type = "sanctions-statement",
    .tags = "\"sanctions\",\"provenance\"", .key_env = OS_KEY, .headers = OS_AUTH,
    .free_tier = 1, .url = OS_BASE "/statements?canonical_id={q}&limit=100",
    .array_path = "results", .title_keys = "prop,value", .id_keys = "id",
    .date_keys = "first_seen",
    .description = "Provenance level: every individual assertion about an entity — "
      "which dataset said which property value, and when it was first and last "
      "seen. This is how you tell a stale designation from a live one" },

  /* ── Law-enforcement notices ───────────────────────────────────────────── */
  { .id = "INTERPOL_RED_NOTICES", .name = "Interpol — red notices",
    .name_ja = "インターポール 赤色手配書", .category = "safety",
    .portal = "https://www.interpol.int", .record_type = "interpol-red-notice",
    .tags = "\"wanted\",\"interpol\"", .free_tier = 1,
    .url = "https://ws-public.interpol.int/notices/v1/red?name={q}&resultPerPage=40",
    .array_path = "_embedded.notices", .title_keys = "name,forename",
    .id_keys = "entity_id", .date_keys = "date_of_birth",
    .page_param = "page", .page_start = 1,
    .description = "Interpol red notices matching a name — forename/surname, "
      "nationality, date and place of birth, charges and the issuing country. "
      "Keyless and authoritative" },

  { .id = "INTERPOL_UN_NOTICES", .name = "Interpol — UN Security Council special notices",
    .name_ja = "インターポール 国連特別通知", .category = "safety",
    .portal = "https://www.interpol.int", .record_type = "interpol-un-notice",
    .tags = "\"sanctions\",\"interpol\",\"un\"", .free_tier = 1,
    .url = "https://ws-public.interpol.int/notices/v1/un?name={q}&resultPerPage=40",
    .array_path = "_embedded.notices", .title_keys = "name,forename",
    .id_keys = "entity_id",
    .page_param = "page", .page_start = 1,
    .description = "Interpol–UN Security Council special notices — individuals and "
      "entities under UN sanctions regimes, with the identifying detail Interpol "
      "publishes alongside the designation" },

  { .id = "EUROPOL_MOST_WANTED", .name = "Europol — EU most wanted",
    .name_ja = "ユーロポール 重要指名手配", .category = "safety",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://eumostwanted.eu", .record_type = "wanted-person",
    .tags = "\"wanted\",\"europol\"", .free_tier = 1,
    .url = "https://eumostwanted.eu/search?keyword={q}",
    .base = "https://eumostwanted.eu", .filter_query = 1,
    .description = "Europol's EU most-wanted listings matching a name — the fugitive "
      "profiles member states have published Europe-wide, with the requesting "
      "country and offence" },

  /* ── Debarment & screening lists ───────────────────────────────────────── */
  { .id = "OS_DATASET_INDEX", .name = "OpenSanctions — dataset coverage index",
    .name_ja = "OpenSanctions — データセット網羅状況", .category = "investigation",
    .portal = "https://www.opensanctions.org/datasets/", .record_type = "sanctions-dataset",
    .tags = "\"sanctions\",\"provenance\",\"coverage\"", .free_tier = 1,
    .url = "https://data.opensanctions.org/datasets/latest/index.json",
    .array_path = "datasets", .filter_query = 1,
    .title_keys = "title,name", .id_keys = "name", .date_keys = "last_change",
    .description = "Which source lists the corpus actually contains, filtered to the "
      "queried authority or country — entity counts, coverage dates, publisher and "
      "last update. This is how you establish that a negative result means anything" },

  { .id = "US_CSL_SCREENING", .name = "US Consolidated Screening List",
    .name_ja = "米国 統合スクリーニングリスト", .category = "government",
    .portal = "https://www.trade.gov/consolidated-screening-list",
    .record_type = "us-screening-list", .tags = "\"sanctions\",\"us\",\"export-control\"",
    .key_env = "CSL_API_KEY", .free_tier = 1,
    .url = "https://data.trade.gov/consolidated_screening_list/v1/search?name={q}&size=40",
    .headers = { "subscription-key: {key}", NULL },
    .array_path = "results", .title_keys = "name", .id_keys = "id",
    .date_keys = "start_date",
    .description = "All eleven US restricted-party lists in one query — OFAC SDN and "
      "non-SDN, BIS Entity/Denied Persons/Unverified, State Department debarments — "
      "with programmes, addresses, alternate names and federal-register citations" },

  { .id = "OFAC_SDN_CSV", .name = "OFAC — SDN primary list (raw)",
    .name_ja = "OFAC SDNリスト(原本)", .category = "government",
    .type = "dataset", .mode = HP_CSV, .csv_no_header = 1,
    .portal = "https://sanctionslist.ofac.treas.gov", .record_type = "ofac-sdn",
    .tags = "\"sanctions\",\"us\",\"ofac\"", .free_tier = 1,
    .url = "https://www.treasury.gov/ofac/downloads/sdn.csv",
    .filter_query = 1, .title_keys = "col1", .id_keys = "col0",
    .description = "The authoritative OFAC Specially Designated Nationals file, read "
      "directly and filtered to the query — SDN id, name, type, programmes, title, "
      "vessel detail and remarks exactly as Treasury publishes them" },

  { .id = "OFAC_SDN_ALT_NAMES", .name = "OFAC — SDN alternate names",
    .name_ja = "OFAC SDN 別名リスト", .category = "government",
    .type = "dataset", .mode = HP_CSV, .csv_no_header = 1,
    .portal = "https://sanctionslist.ofac.treas.gov", .record_type = "ofac-alias",
    .tags = "\"sanctions\",\"us\",\"aliases\"", .free_tier = 1,
    .url = "https://www.treasury.gov/ofac/downloads/alt.csv",
    .filter_query = 1, .title_keys = "col3", .id_keys = "col1",
    .description = "Every alias, a.k.a. and transliteration OFAC records for a "
      "designated party. Sanctions evasion is mostly a spelling exercise, so the "
      "alias file catches what the primary name file misses" },

  { .id = "OFAC_SDN_ADDRESSES", .name = "OFAC — SDN addresses",
    .name_ja = "OFAC SDN 住所リスト", .category = "government",
    .type = "dataset", .mode = HP_CSV, .csv_no_header = 1,
    .portal = "https://sanctionslist.ofac.treas.gov", .record_type = "ofac-address",
    .tags = "\"sanctions\",\"us\",\"addresses\"", .free_tier = 1,
    .url = "https://www.treasury.gov/ofac/downloads/add.csv",
    .filter_query = 1, .title_keys = "col2", .id_keys = "col0",
    .description = "The addresses attached to OFAC designations — the field that "
      "links a sanctioned party to a building, a registered agent or a shared "
      "corporate address used by other entities" },

  { .id = "CA_SEMA_SANCTIONS", .name = "Canada — consolidated autonomous sanctions",
    .name_ja = "カナダ 制裁リスト", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.international.gc.ca", .record_type = "ca-sanction",
    .tags = "\"sanctions\",\"ca\"", .free_tier = 1,
    .url = "https://www.international.gc.ca/world-monde/international_relations-"
           "relations_internationales/sanctions/consolidated-consolide.aspx?lang=eng",
    .filter_query = 1, .base = "https://www.international.gc.ca",
    .description = "Canada's consolidated SEMA/JVCFOA listings filtered to the "
      "query — Canada lists parties the US and EU sometimes do not, so it is a "
      "genuinely independent check" },

  { .id = "UA_NAZK_SANCTIONS", .name = "Ukraine NAZK — war & sanctions register",
    .name_ja = "ウクライナ 制裁登録(NAZK)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://sanctions.nazk.gov.ua", .record_type = "ua-sanction",
    .tags = "\"sanctions\",\"ua\"", .free_tier = 1,
    .url = "https://sanctions.nazk.gov.ua/en/search/?search={q}",
    .base = "https://sanctions.nazk.gov.ua", .filter_query = 1,
    .description = "Ukraine's war-and-sanctions register — international sponsors of "
      "war, sanctioned individuals, companies and vessels, with the evidence pages "
      "NAZK publishes per listing" },

  /* ── Ransomware exposure ───────────────────────────────────────────────── */
  { .id = "RANSOMWARE_LIVE_VICTIMS", .name = "ransomware.live — victim disclosures",
    .name_ja = "ランサムウェア被害公開(ransomware.live)", .category = "cyber",
    .portal = "https://www.ransomware.live", .record_type = "ransomware-victim",
    .tags = "\"cyber\",\"ransomware\",\"exposure\"", .free_tier = 1,
    .url = "https://api.ransomware.live/v2/searchvictims/{q}",
    .title_keys = "victim,post_title", .id_keys = "post_url",
    .date_keys = "published",
    .description = "Ransomware leak-site posts naming an organisation — which group "
      "claimed it, when it was published, the description and the leak-site URL. "
      "Often the first public evidence of a breach" },

  { .id = "RANSOMLOOK_SEARCH", .name = "RansomLook — leak-site & group search",
    .name_ja = "RansomLook — 攻撃者/被害者検索", .category = "cyber",
    .portal = "https://www.ransomlook.io", .record_type = "ransomware-victim",
    .tags = "\"cyber\",\"ransomware\"", .free_tier = 1,
    .url = "https://www.ransomlook.io/api/search/{q}",
    .title_keys = "post_title,name,group_name", .date_keys = "discovered",
    .description = "Second independent ransomware-leak index — victim posts, group "
      "profiles and mirrors. Two trackers matter because leak sites vanish and each "
      "tracker misses different windows" },
};

HP_REGISTER_TABLE(HP_SANCTIONS)
