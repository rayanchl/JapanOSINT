/* Verified-live academic sources (60), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

VRSS(sci_arstechnica_science, "sci-arstechnica-science", "Ars Technica Science", "Ars Technica サイエンス",
  "academic", "literature",
  "https://arstechnica.com/science/feed/",
  "en", "[\"science-journalism\"]", 7200,
  "Ars Technica science desk coverage.");

VRSS(sci_cnrs_news, "sci-cnrs-news", "CNRS news", "フランス国立科学研究センター（CNRS）ニュース",
  "academic", "policy",
  "https://www.cnrs.fr/fr/rss.xml",
  "fr", "[\"cnrs\",\"france\",\"research\"]", 21600,
  "CNRS research announcements across all disciplines.");

/* sci-cordis-articles: CORDIS publishes one record per article PER LANGUAGE
 * (each record carries "language" and an "availableLanguages" list of up to
 * seven), and every language edition of an article repeats the SAME "id" —
 * the rcn-plus-slug of the English original. Live-verified 2026-09-07: a
 * num=50 page holds 50 records and only 25 distinct "id" values, one article
 * appearing three times; that is exactly the 47.6% collapse the registry sweep
 * measured over 1,000 records. The language editions are different documents
 * with different titles and teasers, so they must not be merged.
 * jsonlist_emit_paged_keyed takes a single field, not a composite, so this
 * hand-rolls the same paged-emit-with-relabelled-id pattern as
 * vsrc_environment_3.c (geo-tidesandcurrents-currents): "id" is rewritten to
 * id plus language, both the upstream's own values, and nothing is removed. */
typedef struct { const char *path, *record_type, *lang, *tags_json; } cordis_composite_opts;

static int cordis_emit_page_composite(const source_ctx *c, intel_sink *s,
                                      const char *id, cJSON *doc, void *ud,
                                      int *seen) {
  (void)c;
  cordis_composite_opts *o = (cordis_composite_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  if (arr) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (!cJSON_IsObject(rec)) continue;
      cJSON *rid = cJSON_GetObjectItemCaseSensitive(rec, "id");
      cJSON *lg  = cJSON_GetObjectItemCaseSensitive(rec, "language");
      if (!cJSON_IsString(rid) || !rid->valuestring) continue;
      if (!cJSON_IsString(lg) || !lg->valuestring) continue;
      char buf[256];
      snprintf(buf, sizeof buf, "%s_%s", rid->valuestring, lg->valuestring);
      cJSON_DeleteItemFromObjectCaseSensitive(rec, "id");
      cJSON_AddStringToObject(rec, "id", buf);
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->record_type, o->lang,
                          o->tags_json, seen);
}

static int run_sci_cordis_articles(const source_ctx *c, intel_sink *s) {
  cordis_composite_opts o = { "payload.results", "funding", "en",
                              "[\"eu\",\"horizon\",\"cordis\"]" };
  int n = pw_walk(c, s, "sci-cordis-articles",
                  "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27article%27&format=json&p=1&num=50",   /* exhaustive-ok: pw_walk advances p= (PW_PAGE_PARAMS, lib/pagewalk.c:199) and discloses the remainder */
                  pw_fetch_json, cordis_emit_page_composite, &o);
  if (n < 0) {
    fprintf(stderr, "[sci-cordis-articles] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def sci_cordis_articles = {
  .id = "sci-cordis-articles", .collector = "academic",
  .name = "CORDIS research articles", .name_ja = "CORDIS 研究記事",
  .update_interval_sec = 86400, .run = run_sci_cordis_articles,
  .category = "funding", .type = "api",
  .url = "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27article%27&format=json&p=1&num=50",   /* exhaustive-ok: documentation copy of the walked URL above; run() pages it */
  .description = "CORDIS editorial coverage of EU-funded research.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(sci_cordis_articles);

VJSON(sci_cordis_projects, "sci-cordis-projects", "CORDIS EU research projects", "CORDIS EU研究プロジェクト",
  "academic", "funding",
  "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27project%27&format=json&p=1&num=50",
  "payload.results",
  "en", "[\"eu\",\"horizon\",\"funding\",\"cordis\"]", 86400,
  "Horizon Europe and legacy EU framework projects with participants and budgets.");

/* sci-cordis-results: result records carry no `id` — their identifier is
 * `rcn` — so the emitter hashed title/link/date, and results sharing a title
 * ("Periodic Reporting for period 1 - …") collided across pages. Measured
 * 2026-09-15 over the 20 walked pages: 1,000 records, 1,000 distinct rcn, 952
 * distinct titles. Keyed on rcn; the walk is unchanged. */
#include "_vjson_idkeys.inc"
VJSON_IDKEYS(sci_cordis_results, "sci-cordis-results", "CORDIS project results", "CORDIS プロジェクト成果",
  "academic", "funding",
  "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27result%27&format=json&p=1&num=50",
  "payload.results",
  "en", "[\"eu\",\"horizon\",\"results\",\"cordis\"]", 86400,
  "Reported outcomes of EU-funded research projects.",
  "rcn");

VJSON(sci_crossref_books, "sci-crossref-books", "Crossref registered monographs", "Crossref 登録単行書",
  "academic", "literature",
  "https://api.crossref.org/works?filter=type:monograph&sort=created&order=desc&rows=100",
  "message.items",
  "en", "[\"book\",\"crossref\"]", 86400,
  "Scholarly monographs newly registered with Crossref.");

VJSON(sci_crossref_components, "sci-crossref-components", "Crossref article components", "Crossref 論文構成要素",
  "academic", "literature",
  "https://api.crossref.org/works?filter=type:component&sort=created&order=desc&rows=100",
  "message.items",
  "en", "[\"crossref\",\"components\"]", 86400,
  "Figures, tables and supplements registered as their own DOIs.");

VJSON(sci_crossref_corrections, "sci-crossref-corrections", "Crossref correction notices", "Crossref 訂正通知",
  "academic", "literature",
  "https://api.crossref.org/works?filter=update-type:correction&sort=updated&order=desc&rows=100",
  "message.items",
  "en", "[\"correction\",\"research-integrity\",\"crossref\"]", 7200,
  "Published corrections and errata registered against existing DOIs.");

VJSON(sci_crossref_datasets, "sci-crossref-datasets", "Crossref registered datasets", "Crossref 登録データセット",
  "academic", "repository",
  "https://api.crossref.org/works?filter=type:dataset&sort=created&order=desc&rows=100",
  "message.items",
  "en", "[\"dataset\",\"crossref\"]", 21600,
  "Datasets newly assigned a Crossref DOI.");

VJSON(sci_crossref_dissertations, "sci-crossref-dissertations", "Crossref registered dissertations", "Crossref 登録学位論文",
  "academic", "literature",
  "https://api.crossref.org/works?filter=type:dissertation&sort=created&order=desc&rows=100",
  "message.items",
  "en", "[\"dissertation\",\"crossref\"]", 86400,
  "Doctoral and masters theses newly assigned DOIs.");

VJSON(sci_crossref_expressions_of_concern, "sci-crossref-expressions-of-concern", "Crossref expressions of concern", "Crossref 懸念表明",
  "academic", "literature",
  "https://api.crossref.org/works?filter=update-type:expression_of_concern&sort=updated&order=desc&rows=100",
  "message.items",
  "en", "[\"research-integrity\",\"crossref\",\"expression-of-concern\"]", 7200,
  "Editorial expressions of concern — usually the first public sign of a misconduct investigation.");

VJSON(sci_crossref_funders, "sci-crossref-funders", "Crossref Funder Registry", "Crossref 助成機関レジストリ",
  "academic", "funding",
  "https://api.crossref.org/funders?rows=100",
  "message.items",
  "en", "[\"crossref\",\"funders\"]", 86400,
  "Canonical registry of research funders used in funding acknowledgements.");

VJSON(sci_crossref_grants, "sci-crossref-grants", "Crossref registered grants", "Crossref 登録助成金",
  "academic", "funding",
  "https://api.crossref.org/works?filter=type:grant&sort=created&order=desc&rows=100",
  "message.items",
  "en", "[\"grant\",\"funding\",\"crossref\"]", 86400,
  "Funding awards registered as Crossref grant DOIs.");

VJSON(sci_crossref_journals, "sci-crossref-journals", "Crossref journal registry", "Crossref 学術誌レジストリ",
  "academic", "policy",
  "https://api.crossref.org/journals?rows=100",
  "message.items",
  "en", "[\"crossref\",\"journals\"]", 86400,
  "Journal-level registry with ISSNs and publisher linkage.");

VJSON(sci_crossref_licenses, "sci-crossref-licenses", "Crossref licence registry", "Crossref ライセンスレジストリ",
  "academic", "policy",
  "https://api.crossref.org/licenses?rows=100",
  "message.items",
  "en", "[\"crossref\",\"licence\"]", 86400,
  "Licence URIs asserted on Crossref-registered content.");

VJSON(sci_crossref_members, "sci-crossref-members", "Crossref member publishers", "Crossref 会員出版社",
  "academic", "policy",
  "https://api.crossref.org/members?rows=100",
  "message.items",
  "en", "[\"crossref\",\"publishers\"]", 86400,
  "Registry of Crossref member publishers and their deposit behaviour.");

VJSON(sci_crossref_peer_reviews, "sci-crossref-peer-reviews", "Crossref peer review records", "Crossref 査読記録",
  "academic", "literature",
  "https://api.crossref.org/works?filter=type:peer-review&sort=created&order=desc&rows=100",
  "message.items",
  "en", "[\"peer-review\",\"crossref\"]", 21600,
  "Open peer-review reports registered with their own DOIs.");

VJSON(sci_crossref_preprints, "sci-crossref-preprints", "Crossref posted content (preprints)", "Crossref 掲載コンテンツ（プレプリント）",
  "academic", "preprint",
  "https://api.crossref.org/works?filter=type:posted-content&sort=created&order=desc&rows=100",
  "message.items",
  "en", "[\"preprint\",\"crossref\"]", 7200,
  "All newly registered preprints across every Crossref-participating preprint server.");

VJSON(sci_crossref_proceedings, "sci-crossref-proceedings", "Crossref conference papers", "Crossref 会議論文",
  "academic", "literature",
  "https://api.crossref.org/works?filter=type:proceedings-article&sort=created&order=desc&rows=100",
  "message.items",
  "en", "[\"proceedings\",\"conference\",\"crossref\"]", 21600,
  "Conference proceedings articles as they are deposited.");

VJSON(sci_crossref_reports, "sci-crossref-reports", "Crossref registered reports", "Crossref 登録報告書",
  "academic", "literature",
  "https://api.crossref.org/works?filter=type:report&sort=created&order=desc&rows=100",
  "message.items",
  "en", "[\"report\",\"crossref\"]", 86400,
  "Institutional and technical reports newly registered with Crossref.");

VJSON(sci_crossref_retractions, "sci-crossref-retractions", "Crossref retraction notices", "Crossref 撤回通知",
  "academic", "literature",
  "https://api.crossref.org/works?filter=update-type:retraction&sort=updated&order=desc&rows=100",
  "message.items",
  "en", "[\"retraction\",\"research-integrity\",\"crossref\"]", 7200,
  "DOIs whose Crossref record carries a retraction update — the cleanest machine-readable retraction feed.");

VJSON(sci_crossref_standards, "sci-crossref-standards", "Crossref registered standards", "Crossref 登録規格",
  "academic", "policy",
  "https://api.crossref.org/works?filter=type:standard&sort=created&order=desc&rows=100",
  "message.items",
  "en", "[\"standards\",\"crossref\"]", 86400,
  "Technical standards documents newly registered with DOIs.");

VJSON(sci_crossref_types, "sci-crossref-types", "Crossref work types", "Crossref 著作物タイプ",
  "academic", "policy",
  "https://api.crossref.org/types?rows=100",
  "message.items",
  "en", "[\"crossref\",\"metadata\"]", 86400,
  "Controlled vocabulary of Crossref work types.");

VJSON(sci_crossref_withdrawals, "sci-crossref-withdrawals", "Crossref withdrawal notices", "Crossref 取り下げ通知",
  "academic", "literature",
  "https://api.crossref.org/works?filter=update-type:withdrawal&sort=updated&order=desc&rows=100",
  "message.items",
  "en", "[\"withdrawal\",\"research-integrity\",\"crossref\"]", 21600,
  "Works withdrawn by publisher or author after registration.");

/* sci-datacite-* (the /dois rows): DataCite's own `links.next` DROPS the
 * `sort` parameter — page 1 of `?page[size]=50&sort=-created` links to
 * `?page[number]=2&page[size]=50&resource-type-id=…` with no sort, so every
 * later page is served in the API's default order and re-serves DOIs the walk
 * already has while never reaching others. Measured 2026-09-15 over the 20
 * pages the walk reads: resource-type text 1,000 records / 949 distinct DOIs
 * as followed, 997 with the sort carried forward; other 990 vs 999. The
 * residual is DOIs minted during the walk pushing a record onto the next page
 * (the same DOI re-served, collapsing correctly). The hook only re-appends the
 * sort the row itself asked for to the upstream's own next link. */
#include "_vjson_idkeys.inc"
static void datacite_keep_sort(cJSON *doc) {
  cJSON *links = cJSON_GetObjectItemCaseSensitive(doc, "links");
  cJSON *next = cJSON_GetObjectItemCaseSensitive(links, "next");
  const char *u = cJSON_GetStringValue(next);
  if (!u || !*u || strstr(u, "sort=")) return;
  size_t n = strlen(u) + 32;
  char *nu = malloc(n);
  if (!nu) return;
  snprintf(nu, n, "%s%csort=-created", u, strchr(u, '?') ? '&' : '?');
  cJSON_ReplaceItemInObjectCaseSensitive(links, "next", cJSON_CreateString(nu));
  free(nu);
}

VJSON_PREP(sci_datacite_audiovisual, "sci-datacite-audiovisual", "DataCite audiovisual", "DataCite 音声・映像資料",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=50&resource-type-id=audiovisual&sort=-created",
  "data",
  "en", "[\"datacite\",\"audiovisual\"]", 21600,
  "DataCite DOI registry slice: audiovisual.", NULL, datacite_keep_sort);

VJSON(sci_datacite_clients, "sci-datacite-clients", "DataCite repository clients", "DataCite リポジトリ登録者",
  "academic", "repository",
  "https://api.datacite.org/clients?page[size]=100",
  "data",
  "en", "[\"datacite\",\"repositories\"]", 86400,
  "Registered DataCite repositories and their DOI prefixes.");

VJSON_PREP(sci_datacite_collection, "sci-datacite-collection", "DataCite collection", "DataCite コレクション",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=50&resource-type-id=collection&sort=-created",
  "data",
  "en", "[\"datacite\",\"collection\"]", 21600,
  "DataCite DOI registry slice: collection.", NULL, datacite_keep_sort);

VJSON_PREP(sci_datacite_datasets, "sci-datacite-datasets", "DataCite datasets", "DataCite データセット",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=100&resource-type-id=dataset&sort=-created",
  "data",
  "en", "[\"datacite\",\"dataset\"]", 7200,
  "Newly registered dataset DOIs worldwide.", NULL, datacite_keep_sort);

VJSON_PREP(sci_datacite_image, "sci-datacite-image", "DataCite image", "DataCite 画像",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=50&resource-type-id=image&sort=-created",
  "data",
  "en", "[\"datacite\",\"image\"]", 21600,
  "DataCite DOI registry slice: image.", NULL, datacite_keep_sort);

VJSON_PREP(sci_datacite_japan, "sci-datacite-japan", "DataCite Japan-related DOIs", "DataCite 日本関連DOI",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=50&query=Japan&sort=-created",
  "data",
  "ja", "[\"datacite\",\"japan\",\"dataset\"]", 21600,
  "Newly registered DOIs whose metadata mentions Japan.", NULL, datacite_keep_sort);

VJSON_PREP(sci_datacite_latest, "sci-datacite-latest", "DataCite latest DOIs", "DataCite 最新DOI",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=100&sort=-created",
  "data",
  "en", "[\"datacite\",\"doi\",\"dataset\"]", 7200,
  "Every newly minted DataCite DOI across all research outputs and repositories.", NULL, datacite_keep_sort);

VJSON_PREP(sci_datacite_model, "sci-datacite-model", "DataCite model", "DataCite モデル",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=50&resource-type-id=model&sort=-created",
  "data",
  "en", "[\"datacite\",\"model\"]", 21600,
  "DataCite DOI registry slice: model.", NULL, datacite_keep_sort);

VJSON_PREP(sci_datacite_other, "sci-datacite-other", "DataCite other", "DataCite その他資料",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=50&resource-type-id=other&sort=-created",
  "data",
  "en", "[\"datacite\",\"other\"]", 21600,
  "DataCite DOI registry slice: other.", NULL, datacite_keep_sort);

VJSON(sci_datacite_providers, "sci-datacite-providers", "DataCite consortium providers", "DataCite 提供機関",
  "academic", "repository",
  "https://api.datacite.org/providers?page[size]=100",
  "data",
  "en", "[\"datacite\",\"consortium\"]", 86400,
  "National and consortium DataCite members.");

VJSON_PREP(sci_datacite_service, "sci-datacite-service", "DataCite service", "DataCite サービス",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=50&resource-type-id=service&sort=-created",
  "data",
  "en", "[\"datacite\",\"service\"]", 21600,
  "DataCite DOI registry slice: service.", NULL, datacite_keep_sort);

VJSON_PREP(sci_datacite_software, "sci-datacite-software", "DataCite software", "DataCite ソフトウェア",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=100&resource-type-id=software&sort=-created",
  "data",
  "en", "[\"datacite\",\"software\"]", 21600,
  "Newly registered research software DOIs.", NULL, datacite_keep_sort);

VJSON_PREP(sci_datacite_text, "sci-datacite-text", "DataCite text", "DataCite テキスト資料",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=50&resource-type-id=text&sort=-created",
  "data",
  "en", "[\"datacite\",\"text\"]", 21600,
  "DataCite DOI registry slice: text.", NULL, datacite_keep_sort);

VJSON_PREP(sci_datacite_workflow, "sci-datacite-workflow", "DataCite workflow", "DataCite ワークフロー",
  "academic", "repository",
  "https://api.datacite.org/dois?page[size]=50&resource-type-id=workflow&sort=-created",
  "data",
  "en", "[\"datacite\",\"workflow\"]", 21600,
  "DataCite DOI registry slice: workflow.", NULL, datacite_keep_sort);

VJSON(sci_doaj_articles_latest, "sci-doaj-articles-latest", "DOAJ latest open-access articles", "DOAJ 最新オープンアクセス論文",
  "academic", "literature",
  "https://doaj.org/api/search/articles/*?pageSize=100",
  "results",
  "en", "[\"doaj\",\"open-access\"]", 7200,
  "Newly indexed articles from vetted open-access journals worldwide.");

VJSON(sci_doaj_journals_all, "sci-doaj-journals-all", "DOAJ journal registry", "DOAJ 学術誌登録簿",
  "academic", "policy",
  "https://doaj.org/api/search/journals/*?pageSize=100",
  "results",
  "en", "[\"doaj\",\"journals\",\"open-access\"]", 86400,
  "Vetted open-access journals with licence, APC and peer-review policy.");

VJSON(sci_doaj_journals_oa, "sci-doaj-journals-oa", "DOAJ recently opened journals", "DOAJ 新規オープンアクセス誌",
  "academic", "policy",
  "https://doaj.org/api/search/journals/bibjson.oa_start%3A2024?pageSize=100",
  "results",
  "en", "[\"doaj\",\"journals\",\"open-access\"]", 86400,
  "Journals that recently converted to open access.");

VRSS(sci_frontiers_aging_neuroscience, "sci-frontiers-aging-neuroscience", "Frontiers in Aging Neuroscience | New and Recent Articles", "フロンティアーズ 加齢神経科学",
  "academic", "literature",
  "https://www.frontiersin.org/journals/aging-neuroscience/rss",
  "en", "[\"frontiers\",\"open-access\",\"aging-neuroscience\"]", 21600,
  "Latest open-access articles from Frontiers in Aging Neuroscience | New and Recent Articles.");

VRSS(sci_frontiers_artificial_intelligence, "sci-frontiers-artificial-intelligence", "Frontiers in Artificial Intelligence | New and Recent Articles", "フロンティアーズ 人工知能",
  "academic", "literature",
  "https://www.frontiersin.org/journals/artificial-intelligence/rss",
  "en", "[\"frontiers\",\"open-access\",\"artificial-intelligence\"]", 21600,
  "Latest open-access articles from Frontiers in Artificial Intelligence | New and Recent Articles.");

VRSS(sci_frontiers_astronomy_and_space_sciences, "sci-frontiers-astronomy-and-space-sciences", "Frontiers in Astronomy and Space Sciences | New and Recent Articles", "フロンティアーズ 天文学・宇宙科学",
  "academic", "astronomy",
  "https://www.frontiersin.org/journals/astronomy-and-space-sciences/rss",
  "en", "[\"frontiers\",\"open-access\",\"astronomy-and-space-sciences\"]", 21600,
  "Latest open-access articles from Frontiers in Astronomy and Space Sciences | New and Recent Articles.");

VRSS(sci_frontiers_big_data, "sci-frontiers-big-data", "Frontiers in Big Data | New and Recent Articles", "フロンティアーズ ビッグデータ",
  "academic", "literature",
  "https://www.frontiersin.org/journals/big-data/rss",
  "en", "[\"frontiers\",\"open-access\",\"big-data\"]", 21600,
  "Latest open-access articles from Frontiers in Big Data | New and Recent Articles.");

VRSS(sci_frontiers_bioengineering_and_biotechnology, "sci-frontiers-bioengineering-and-biotechnology", "Frontiers in Bioengineering and Biotechnology | New and Recent Articles", "フロンティアーズ 生物工学・バイオテクノロジー",
  "academic", "literature",
  "https://www.frontiersin.org/journals/bioengineering-and-biotechnology/rss",
  "en", "[\"frontiers\",\"open-access\",\"bioengineering-and-biotechnology\"]", 21600,
  "Latest open-access articles from Frontiers in Bioengineering and Biotechnology | New and Recent Articles.");

VRSS(sci_frontiers_built_environment, "sci-frontiers-built-environment", "Frontiers in Built Environment | New and Recent Articles", "フロンティアーズ 建築環境",
  "academic", "literature",
  "https://www.frontiersin.org/journals/built-environment/rss",
  "en", "[\"frontiers\",\"open-access\",\"built-environment\"]", 21600,
  "Latest open-access articles from Frontiers in Built Environment | New and Recent Articles.");

VRSS(sci_frontiers_cardiovascular_medicine, "sci-frontiers-cardiovascular-medicine", "Frontiers in Cardiovascular Medicine | New and Recent Articles", "フロンティアーズ 循環器医学",
  "academic", "clinical",
  "https://www.frontiersin.org/journals/cardiovascular-medicine/rss",
  "en", "[\"frontiers\",\"open-access\",\"cardiovascular-medicine\"]", 21600,
  "Latest open-access articles from Frontiers in Cardiovascular Medicine | New and Recent Articles.");

VRSS(sci_frontiers_cell_and_developmental_biology, "sci-frontiers-cell-and-developmental-biology", "Frontiers in Cell and Developmental Biology | New and Recent Articles", "フロンティアーズ 細胞・発生生物学",
  "academic", "literature",
  "https://www.frontiersin.org/journals/cell-and-developmental-biology/rss",
  "en", "[\"frontiers\",\"open-access\",\"cell-and-developmental-biology\"]", 21600,
  "Latest open-access articles from Frontiers in Cell and Developmental Biology | New and Recent Articles.");

VRSS(sci_frontiers_cellular_and_infection_microbiology, "sci-frontiers-cellular-and-infection-microbiology", "Frontiers in Cellular and Infection Microbiology | New and Recent Articles", "フロンティアーズ 細胞・感染微生物学",
  "academic", "genomics",
  "https://www.frontiersin.org/journals/cellular-and-infection-microbiology/rss",
  "en", "[\"frontiers\",\"open-access\",\"cellular-and-infection-microbiology\"]", 21600,
  "Latest open-access articles from Frontiers in Cellular and Infection Microbiology | New and Recent Articles.");

VRSS(sci_frontiers_chemistry, "sci-frontiers-chemistry", "Frontiers in Chemistry | New and Recent Articles", "フロンティアーズ 化学",
  "academic", "materials",
  "https://www.frontiersin.org/journals/chemistry/rss",
  "en", "[\"frontiers\",\"open-access\",\"chemistry\"]", 21600,
  "Latest open-access articles from Frontiers in Chemistry | New and Recent Articles.");

VRSS(sci_frontiers_climate, "sci-frontiers-climate", "Frontiers in Climate | New and Recent Articles", "フロンティアーズ 気候",
  "academic", "literature",
  "https://www.frontiersin.org/journals/climate/rss",
  "en", "[\"frontiers\",\"open-access\",\"climate\"]", 21600,
  "Latest open-access articles from Frontiers in Climate | New and Recent Articles.");

VRSS(sci_frontiers_communication, "sci-frontiers-communication", "Frontiers in Communication | New and Recent Articles", "フロンティアーズ コミュニケーション学",
  "academic", "literature",
  "https://www.frontiersin.org/journals/communication/rss",
  "en", "[\"frontiers\",\"open-access\",\"communication\"]", 21600,
  "Latest open-access articles from Frontiers in Communication | New and Recent Articles.");

VRSS(sci_frontiers_computer_science, "sci-frontiers-computer-science", "Frontiers in Computer Science | New and Recent Articles", "フロンティアーズ 計算機科学",
  "academic", "literature",
  "https://www.frontiersin.org/journals/computer-science/rss",
  "en", "[\"frontiers\",\"open-access\",\"computer-science\"]", 21600,
  "Latest open-access articles from Frontiers in Computer Science | New and Recent Articles.");

VRSS(sci_frontiers_earth_science, "sci-frontiers-earth-science", "Frontiers in Earth Science | New and Recent Articles", "フロンティアーズ 地球科学",
  "academic", "literature",
  "https://www.frontiersin.org/journals/earth-science/rss",
  "en", "[\"frontiers\",\"open-access\",\"earth-science\"]", 21600,
  "Latest open-access articles from Frontiers in Earth Science | New and Recent Articles.");

VRSS(sci_frontiers_ecology_and_evolution, "sci-frontiers-ecology-and-evolution", "Frontiers in Ecology and Evolution | New and Recent Articles", "フロンティアーズ 生態学・進化学",
  "academic", "literature",
  "https://www.frontiersin.org/journals/ecology-and-evolution/rss",
  "en", "[\"frontiers\",\"open-access\",\"ecology-and-evolution\"]", 21600,
  "Latest open-access articles from Frontiers in Ecology and Evolution | New and Recent Articles.");

VRSS(sci_frontiers_endocrinology, "sci-frontiers-endocrinology", "Frontiers in Endocrinology | New and Recent Articles", "フロンティアーズ 内分泌学",
  "academic", "clinical",
  "https://www.frontiersin.org/journals/endocrinology/rss",
  "en", "[\"frontiers\",\"open-access\",\"endocrinology\"]", 21600,
  "Latest open-access articles from Frontiers in Endocrinology | New and Recent Articles.");

VRSS(sci_frontiers_energy_research, "sci-frontiers-energy-research", "Frontiers in Energy Research | New and Recent Articles", "フロンティアーズ エネルギー研究",
  "academic", "literature",
  "https://www.frontiersin.org/journals/energy-research/rss",
  "en", "[\"frontiers\",\"open-access\",\"energy-research\"]", 21600,
  "Latest open-access articles from Frontiers in Energy Research | New and Recent Articles.");

VRSS(sci_frontiers_environmental_science, "sci-frontiers-environmental-science", "Frontiers in Environmental Science | New and Recent Articles", "フロンティアーズ 環境科学",
  "academic", "literature",
  "https://www.frontiersin.org/journals/environmental-science/rss",
  "en", "[\"frontiers\",\"open-access\",\"environmental-science\"]", 21600,
  "Latest open-access articles from Frontiers in Environmental Science | New and Recent Articles.");

VRSS(sci_frontiers_genetics, "sci-frontiers-genetics", "Frontiers in Genetics | New and Recent Articles", "フロンティアーズ 遺伝学",
  "academic", "genomics",
  "https://www.frontiersin.org/journals/genetics/rss",
  "en", "[\"frontiers\",\"open-access\",\"genetics\"]", 21600,
  "Latest open-access articles from Frontiers in Genetics | New and Recent Articles.");
