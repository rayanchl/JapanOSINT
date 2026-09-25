/* Verified-live europe_gov sources (59), part 2.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

/* eur-parlch-committees / eur-parlch-party: the Swiss Parliament OData service
 * keys every entity on the PAIR (ID, Language) — its own __metadata.id reads
 * Committee(ID=1,Language='DE') — and publishes each row once per language.
 * Live-verified 2026-09-07: Committee returns five records per committee (DE,
 * EN, FR, IT, RM), so the 910 records the sweep emitted carried only 182
 * distinct "ID" values and Party's 415 carried 83 — exactly the 5:1 collapse
 * measured. jsonlist's id precedence finds "ID" case-insensitively and keyed
 * every language variant of a committee onto one uid. jsonlist_emit_paged_keyed
 * takes a single field, not a composite, so this hand-rolls the same
 * paged-emit-with-relabelled-id pattern as vsrc_environment_3.c
 * (geo-tidesandcurrents-currents): a lowercase "id" holding ID plus Language,
 * both values the upstream's own, is added to the record and takes precedence
 * over "ID". Nothing is removed, so the numeric ID and the language both stay
 * on the record. */
typedef struct { const char *path, *record_type, *lang, *tags_json; } parlch_composite_opts;

static int parlch_emit_page_composite(const source_ctx *c, intel_sink *s,
                                      const char *id, cJSON *doc, void *ud,
                                      int *seen) {
  (void)c;
  parlch_composite_opts *o = (parlch_composite_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  if (arr) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (!cJSON_IsObject(rec)) continue;
      cJSON *eid = cJSON_GetObjectItem(rec, "ID");
      cJSON *lg  = cJSON_GetObjectItem(rec, "Language");
      if (!eid || !cJSON_IsNumber(eid)) continue;
      char buf[96];
      if (lg && cJSON_IsString(lg) && lg->valuestring)
        snprintf(buf, sizeof buf, "%d_%s", (int)eid->valuedouble, lg->valuestring);
      else
        snprintf(buf, sizeof buf, "%d", (int)eid->valuedouble);
      cJSON_DeleteItemFromObjectCaseSensitive(rec, "id");
      cJSON_AddStringToObject(rec, "id", buf);
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->record_type, o->lang,
                          o->tags_json, seen);
}

static int run_eur_parlch_committees(const source_ctx *c, intel_sink *s) {
  parlch_composite_opts o = { "d", "politics", "de", "[\"che\",\"politics\"]" };
  int n = pw_walk(c, s, "eur-parlch-committees",
                  "https://ws.parlament.ch/odata.svc/Committee?$format=json&$top=100",
                  pw_fetch_json, parlch_emit_page_composite, &o);
  if (n < 0) {
    fprintf(stderr, "[eur-parlch-committees] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def eur_parlch_committees = {
  .id = "eur-parlch-committees", .collector = "europe_gov",
  .name = "Swiss Parliament — committees",
  .name_ja = "Swiss Parliament — committees",
  .update_interval_sec = 86400, .run = run_eur_parlch_committees,
  .category = "politics", .type = "api",
  .url = "https://ws.parlament.ch/odata.svc/Committee?$format=json&$top=100",
  .description = "Committees of the Swiss Federal Assembly.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eur_parlch_committees);

/* Same (ID, Language) entity key as eur-parlch-committees above. Live-verified
 * 2026-09-14: Council answers 15 records carrying 3 distinct "ID" values and 15
 * distinct (ID, Language) pairs — the sweep measured exactly that, 15 emitted
 * and 3 stored, while keyed on ID alone. */
static int run_eur_parlch_council(const source_ctx *c, intel_sink *s) {
  parlch_composite_opts o = { "d", "politics", "de", "[\"che\",\"politics\"]" };
  int n = pw_walk(c, s, "eur-parlch-council",
                  "https://ws.parlament.ch/odata.svc/Council?$format=json&$top=50",
                  pw_fetch_json, parlch_emit_page_composite, &o);
  if (n < 0) {
    fprintf(stderr, "[eur-parlch-council] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def eur_parlch_council = {
  .id = "eur-parlch-council", .collector = "europe_gov",
  .name = "Swiss Parliament — councils",
  .name_ja = "Swiss Parliament — councils",
  .update_interval_sec = 86400, .run = run_eur_parlch_council,
  .category = "politics", .type = "api",
  .url = "https://ws.parlament.ch/odata.svc/Council?$format=json&$top=50",
  .description = "Chambers and councils of the Swiss Federal Assembly.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eur_parlch_council);

/* Same (ID, Language) entity key as eur-parlch-committees above. */
static int run_eur_parlch_party(const source_ctx *c, intel_sink *s) {
  parlch_composite_opts o = { "d", "politics", "de", "[\"che\",\"politics\"]" };
  int n = pw_walk(c, s, "eur-parlch-party",
                  "https://ws.parlament.ch/odata.svc/Party?$format=json&$top=100",
                  pw_fetch_json, parlch_emit_page_composite, &o);
  if (n < 0) {
    fprintf(stderr, "[eur-parlch-party] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def eur_parlch_party = {
  .id = "eur-parlch-party", .collector = "europe_gov",
  .name = "Swiss Parliament — parties",
  .name_ja = "Swiss Parliament — parties",
  .update_interval_sec = 86400, .run = run_eur_parlch_party,
  .category = "politics", .type = "api",
  .url = "https://ws.parlament.ch/odata.svc/Party?$format=json&$top=100",
  .description = "Political parties represented in the Swiss Federal Assembly.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eur_parlch_party);

VJSON(eur_privatbank_rates, "eur-privatbank-rates", "PrivatBank Ukraine — cash exchange rates", "PrivatBank Ukraine — cash exchange rates",
  "europe_gov", "economy",
  "https://api.privatbank.ua/p24api/pubinfo?json&exchange&coursid=5",
  "",
  "uk", "[\"ukr\",\"economy\"]", 21600,
  "Commercial hryvnia cash exchange rates, a market check against the NBU official rate.");

VJSON(eur_pxweb_ee_stat, "eur-pxweb-ee-stat", "Statistics Estonia — PxWeb database index", "Statistics Estonia — PxWeb database index",
  "europe_gov", "statistics",
  "https://andmed.stat.ee/api/v1/en/stat",
  "",
  "en", "[\"est\",\"statistics\"]", 86400,
  "Top-level subject index of the Estonian statistical database.");

VJSON(eur_pxweb_ee_stat_et, "eur-pxweb-ee-stat-et", "Statistics Estonia — PxWeb index (ET)", "Statistics Estonia — PxWeb index (ET)",
  "europe_gov", "statistics",
  "https://andmed.stat.ee/api/v1/et/stat",
  "",
  "et", "[\"est\",\"statistics\"]", 86400,
  "Estonian-language subject index of the national statistical database.");

VJSON(eur_pxweb_fi_statfin, "eur-pxweb-fi-statfin", "Statistics Finland — StatFin database index (EN)", "Statistics Finland — StatFin database index (EN)",
  "europe_gov", "statistics",
  "https://pxdata.stat.fi/PxWeb/api/v1/en/StatFin/",
  "",
  "en", "[\"fin\",\"statistics\"]", 86400,
  "Subject index of the Finnish StatFin statistical database.");

VJSON(eur_pxweb_fi_statfin_fi, "eur-pxweb-fi-statfin-fi", "Statistics Finland — StatFin database index (FI)", "Statistics Finland — StatFin database index (FI)",
  "europe_gov", "statistics",
  "https://pxdata.stat.fi/PxWeb/api/v1/fi/StatFin/",
  "",
  "fi", "[\"fin\",\"statistics\"]", 86400,
  "Finnish-language subject index of the StatFin statistical database.");

VJSON(eur_pxweb_is_en, "eur-pxweb-is-en", "Statistics Iceland — population tables (EN)", "Statistics Iceland — population tables (EN)",
  "europe_gov", "statistics",
  "https://px.hagstofa.is/pxen/api/v1/en/Ibuar",
  "",
  "en", "[\"isl\",\"statistics\"]", 86400,
  "English-language index of Icelandic population statistics tables.");

VJSON(eur_pxweb_is_hagstofa, "eur-pxweb-is-hagstofa", "Statistics Iceland — population database index", "Statistics Iceland — population database index",
  "europe_gov", "statistics",
  "https://px.hagstofa.is/pxis/api/v1/is/Ibuar",
  "",
  "is", "[\"isl\",\"statistics\"]", 86400,
  "Index of Icelandic population statistics tables.");

VJSON(eur_pxweb_lv_stat, "eur-pxweb-lv-stat", "Statistics Latvia — PxWeb database index", "Statistics Latvia — PxWeb database index",
  "europe_gov", "statistics",
  "https://data.stat.gov.lv/api/v1/lv/OSP_PUB",
  "",
  "lv", "[\"lva\",\"statistics\"]", 86400,
  "Subject index of the Latvian official statistics database.");

VJSON(eur_pxweb_no_ssb, "eur-pxweb-no-ssb", "Statistics Norway — StatBank table index", "Statistics Norway — StatBank table index",
  "europe_gov", "statistics",
  "https://data.ssb.no/api/v0/no/table/",
  "",
  "no", "[\"nor\",\"statistics\"]", 86400,
  "Subject index of the Norwegian StatBank statistical database.");

VJSON(eur_pxweb_se_scb, "eur-pxweb-se-scb", "Statistics Sweden — national accounts index", "Statistics Sweden — national accounts index",
  "europe_gov", "statistics",
  "https://api.scb.se/OV0104/v1/doris/en/ssd/NR",
  "",
  "en", "[\"swe\",\"statistics\"]", 86400,
  "Statistics Sweden national-accounts branch of the PxWeb database tree.");

VJSON(eur_pxweb_si_stat, "eur-pxweb-si-stat", "Statistics Slovenia — SiStat table index", "Statistics Slovenia — SiStat table index",
  "europe_gov", "statistics",
  "https://pxweb.stat.si/SiStatData/api/v1/sl/Data",
  "",
  "sl", "[\"svn\",\"statistics\"]", 86400,
  "Full table index of the Slovenian SiStat statistical database.");

/* eur-rada-mps: the convocation file lists six deputies TWICE, the two rows
 * differing only in "new_member" (так / ні). Live-verified 2026-09-14: 469
 * records, 469 byte-distinct, 463 distinct "id" — the sweep measured 469
 * emitted, 463 stored. Both rows are the upstream's data, so the key is
 * (id, new_member); the upstream id is kept on the record as "mp_id". */
static int rada_mps_emit_page(const source_ctx *c, intel_sink *s,
                              const char *id, cJSON *doc, void *ud, int *seen) {
  (void)c; (void)ud;
  cJSON *arr = cJSON_IsArray(doc) ? doc : jsonlist_find_array(doc, "");
  cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    if (!cJSON_IsObject(rec)) continue;
    cJSON *mid = cJSON_GetObjectItemCaseSensitive(rec, "id");
    cJSON *nm  = cJSON_GetObjectItemCaseSensitive(rec, "new_member");
    char idv[64];
    if (cJSON_IsNumber(mid)) snprintf(idv, sizeof idv, "%.17g", mid->valuedouble);
    else if (cJSON_IsString(mid) && mid->valuestring) snprintf(idv, sizeof idv, "%s", mid->valuestring);
    else continue;
    char buf[128];
    if (cJSON_IsString(nm) && nm->valuestring)
      snprintf(buf, sizeof buf, "%s_%s", idv, nm->valuestring);
    else
      snprintf(buf, sizeof buf, "%s", idv);
    if (!cJSON_GetObjectItemCaseSensitive(rec, "mp_id"))
      cJSON_AddItemToObject(rec, "mp_id", cJSON_Duplicate(mid, 1));
    cJSON_DeleteItemFromObjectCaseSensitive(rec, "id");
    cJSON_AddStringToObject(rec, "id", buf);
  }
  return jsonlist_emit_ex(s, id, doc, "", "politics", "uk",
                          "[\"ukr\",\"politics\"]", seen);
}
static int run_eur_rada_mps(const source_ctx *c, intel_sink *s) {
  int n = pw_walk(c, s, "eur-rada-mps",
                  "https://data.rada.gov.ua/ogd/mps/skl9/mps09-data.json",
                  pw_fetch_json, rada_mps_emit_page, NULL);
  if (n < 0) {
    fprintf(stderr, "[eur-rada-mps] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def eur_rada_mps = {
  .id = "eur-rada-mps", .collector = "europe_gov",
  .name = "Verkhovna Rada Ukraine — members of parliament",
  .name_ja = "Verkhovna Rada Ukraine — members of parliament",
  .update_interval_sec = 86400, .run = run_eur_rada_mps,
  .category = "politics", .type = "api",
  .url = "https://data.rada.gov.ua/ogd/mps/skl9/mps09-data.json",
  .description = "Ukrainian parliament deputies of the current convocation with faction and committee.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eur_rada_mps);

VJSON(eur_riksdagen_bet, "eur-riksdagen-bet", "Riksdagen Sweden — committee reports", "Riksdagen Sweden — committee reports",
  "europe_gov", "politics",
  "https://data.riksdagen.se/dokumentlista/?sok=&doktyp=bet&utformat=json&sz=50",
  "dokumentlista.dokument",
  "sv", "[\"swe\",\"politics\"]", 21600,
  "Swedish parliamentary committee reports with document links.");

VJSON(eur_riksdagen_frs, "eur-riksdagen-frs", "Riksdagen Sweden — written questions", "Riksdagen Sweden — written questions",
  "europe_gov", "politics",
  "https://data.riksdagen.se/dokumentlista/?sok=&doktyp=fr&utformat=json&sz=50",
  "dokumentlista.dokument",
  "sv", "[\"swe\",\"politics\"]", 21600,
  "Written questions to Swedish ministers with answers.");

VJSON(eur_riksdagen_ip, "eur-riksdagen-ip", "Riksdagen Sweden — interpellations", "Riksdagen Sweden — interpellations",
  "europe_gov", "politics",
  "https://data.riksdagen.se/dokumentlista/?sok=&doktyp=ip&utformat=json&sz=50",
  "dokumentlista.dokument",
  "sv", "[\"swe\",\"politics\"]", 21600,
  "Interpellations tabled in the Swedish Riksdag.");

VJSON(eur_riksdagen_ledamot, "eur-riksdagen-ledamot", "Riksdagen Sweden — members of parliament", "Riksdagen Sweden — members of parliament",
  "europe_gov", "politics",
  "https://data.riksdagen.se/personlista/?utformat=json&iid=&fnamn=&enamn=&parti=",
  "personlista.person",
  "sv", "[\"swe\",\"politics\"]", 86400,
  "Members of the Swedish Riksdag with party, constituency and assignments.");

VJSON(eur_riksdagen_mot, "eur-riksdagen-mot", "Riksdagen Sweden — private members' bills", "Riksdagen Sweden — private members' bills",
  "europe_gov", "politics",
  "https://data.riksdagen.se/dokumentlista/?sok=&doktyp=mot&utformat=json&sz=50",
  "dokumentlista.dokument",
  "sv", "[\"swe\",\"politics\"]", 21600,
  "Motions tabled by Swedish MPs, newest first.");

VJSON(eur_riksdagen_prop, "eur-riksdagen-prop", "Riksdagen Sweden — government bills", "Riksdagen Sweden — government bills",
  "europe_gov", "politics",
  "https://data.riksdagen.se/dokumentlista/?sok=&doktyp=prop&utformat=json&sz=50",
  "dokumentlista.dokument",
  "sv", "[\"swe\",\"politics\"]", 21600,
  "Government bills submitted to the Swedish Riksdag.");

VJSON(eur_riksdagen_sfs, "eur-riksdagen-sfs", "Riksdagen Sweden — statute book entries", "Riksdagen Sweden — statute book entries",
  "europe_gov", "gazette",
  "https://data.riksdagen.se/dokumentlista/?sok=&doktyp=sfs&utformat=json&sz=50",
  "dokumentlista.dokument",
  "sv", "[\"swe\",\"gazette\"]", 21600,
  "Swedish Code of Statutes (SFS) entries as published to the Riksdag document store.");

VJSON(eur_riksdagen_votering, "eur-riksdagen-votering", "Riksdagen Sweden — vote records", "Riksdagen Sweden — vote records",
  "europe_gov", "politics",
  "https://data.riksdagen.se/voteringlista/?rm=&bet=&punkt=&valkrets=&rost=&iid=&sz=50&utformat=json",
  "voteringlista.votering",
  "sv", "[\"swe\",\"politics\"]", 21600,
  "Individual MP vote records from Swedish parliamentary divisions.");

VJSON(eur_scb_am, "eur-scb-am", "Statistics Sweden — labour market branch", "Statistics Sweden — labour market branch",
  "europe_gov", "statistics",
  "https://api.scb.se/OV0104/v1/doris/en/ssd/AM",
  "",
  "en", "[\"swe\",\"statistics\"]", 86400,
  "Labour-market branch of the Statistics Sweden PxWeb database tree.");

VJSON(eur_scb_be, "eur-scb-be", "Statistics Sweden — population branch", "Statistics Sweden — population branch",
  "europe_gov", "statistics",
  "https://api.scb.se/OV0104/v1/doris/en/ssd/BE",
  "",
  "en", "[\"swe\",\"statistics\"]", 86400,
  "Population branch of the Statistics Sweden PxWeb database tree.");

VJSON(eur_scb_en, "eur-scb-en", "Statistics Sweden — energy branch", "Statistics Sweden — energy branch",
  "europe_gov", "statistics",
  "https://api.scb.se/OV0104/v1/doris/en/ssd/EN",
  "",
  "en", "[\"swe\",\"statistics\"]", 86400,
  "Energy branch of the Statistics Sweden PxWeb database tree.");

VJSON(eur_scb_mi, "eur-scb-mi", "Statistics Sweden — environment branch", "Statistics Sweden — environment branch",
  "europe_gov", "statistics",
  "https://api.scb.se/OV0104/v1/doris/en/ssd/MI",
  "",
  "en", "[\"swe\",\"statistics\"]", 86400,
  "Environment branch of the Statistics Sweden PxWeb database tree.");

VJSON(eur_scb_root, "eur-scb-root", "Statistics Sweden — database root index", "Statistics Sweden — database root index",
  "europe_gov", "statistics",
  "https://api.scb.se/OV0104/v1/doris/en/ssd/",
  "",
  "en", "[\"swe\",\"statistics\"]", 86400,
  "Top-level subject index of the Statistics Sweden PxWeb database.");

VJSON(eur_sejm_eli_acts, "eur-sejm-eli-acts", "Sejm Poland — ELI publishers", "Sejm Poland — ELI publishers",
  "europe_gov", "gazette",
  "https://api.sejm.gov.pl/eli/acts",
  "",
  "pl", "[\"pol\",\"gazette\"]", 86400,
  "Publishers of Polish legal acts exposed through the Sejm ELI API.");

#include "lib/jocore.h"

/* eur-sejm-eli-du / eur-sejm-eli-mp: the publisher index answers
 * {actsCount:97788, years:[1918, …]} — years as bare integers — so the old
 * rows, pointed at "years", fetched a live index and emitted nothing: a list
 * of numbers is not a list of records. The acts are one hop down at
 * /eli/acts/<PUB>/<year> ({count, items:[{address, title, status, …}],
 * totalCount}). Live-verified 2026-09-14: Dziennik Ustaw 105 years and 97,788
 * acts; DU/2024 answers 1,984 items with totalCount 1,984 in one response.
 *
 * id = "address" (e.g. WDU20240000001), the ELI identifier itself. A year whose
 * listing answers fewer items than its own totalCount, or fails outright, is
 * disclosed as a notice rather than skipped. Weekly, because this walks the
 * whole archive; vsrc_legal_3.c's sejm rows already poll the CURRENT year
 * (…/DU/2026, …/MP/2026) on their own faster schedule, so that one year is
 * the only overlap. */
static int sejm_eli_walk(const source_ctx *c, intel_sink *s, const char *id,
                         const char *pub) {
  char url[160];
  snprintf(url, sizeof url, "https://api.sejm.gov.pl/eli/acts/%s", pub);
  cJSON *idx = feed_get_json(c->http, url, 25000);
  cJSON *years = idx ? cJSON_GetObjectItemCaseSensitive(idx, "years") : NULL;
  if (!cJSON_IsArray(years)) {
    fprintf(stderr, "[%s] year index fetch failed\n", id);
    cJSON_Delete(idx);
    return -1;
  }
  int nyears = 0, failed = 0, n = 0;
  cJSON *y;
  cJSON_ArrayForEach(y, years) {
    if (!cJSON_IsNumber(y)) continue;
    nyears++;
    int year = (int)y->valuedouble;
    snprintf(url, sizeof url, "https://api.sejm.gov.pl/eli/acts/%s/%d", pub, year);
    cJSON *doc = feed_get_json(c->http, url, 60000);
    cJSON *items = doc ? cJSON_GetObjectItemCaseSensitive(doc, "items") : NULL;
    if (!cJSON_IsArray(items)) {
      failed++;
      fprintf(stderr, "[%s] year listing failed: %s\n", id, url);
      cJSON_Delete(doc);
      continue;
    }
    cJSON *rec;
    cJSON_ArrayForEach(rec, items) {
      cJSON *ad = cJSON_GetObjectItemCaseSensitive(rec, "address");
      if (cJSON_IsString(ad) && ad->valuestring &&
          !cJSON_GetObjectItemCaseSensitive(rec, "id"))
        cJSON_AddStringToObject(rec, "id", ad->valuestring);
    }
    int got = cJSON_GetArraySize(items);
    cJSON *tc = cJSON_GetObjectItemCaseSensitive(doc, "totalCount");
    if (cJSON_IsNumber(tc) && (long)tc->valuedouble > got) {
      char q[48], why[160];
      snprintf(q, sizeof q, "%s/%d", pub, year);
      snprintf(why, sizeof why, "the %s/%d listing answered %d of its %ld acts",
               pub, year, got, (long)tc->valuedouble);
      jo_truncation_notice_ex(s, id, q, got, (long)tc->valuedouble, why,
                              "page the year listing with offset", NULL);
    }
    int e = jsonlist_emit_ex(s, id, doc, "items", "gazette", "pl",
                             "[\"pol\",\"gazette\"]", NULL);
    if (e > 0) n += e;
    cJSON_Delete(doc);
  }
  cJSON_Delete(idx);
  if (failed) {
    char why[160];
    snprintf(why, sizeof why, "%d of %d year listing(s) failed to fetch",
             failed, nyears);
    jo_truncation_notice_ex(s, id, NULL, n, -1, why,
                            "re-run; each failed year url is in the run log",
                            NULL);
  }
  fprintf(stderr, "[%s] emitted %d act(s) across %d year(s)\n", id, n, nyears);
  return (n == 0 && failed) ? -1 : 0;
}

static int run_eur_sejm_eli_du(const source_ctx *c, intel_sink *s) {
  return sejm_eli_walk(c, s, "eur-sejm-eli-du", "DU");
}
static const source_def eur_sejm_eli_du = {
  .id = "eur-sejm-eli-du", .collector = "europe_gov",
  .name = "Sejm Poland — Journal of Laws (Dziennik Ustaw), full archive",
  .name_ja = "Sejm Poland — Journal of Laws (Dziennik Ustaw), full archive",
  .update_interval_sec = 604800, .run = run_eur_sejm_eli_du,
  .category = "gazette", .type = "api",
  .url = "https://api.sejm.gov.pl/eli/acts/DU",
  .description = "Every act published in Dziennik Ustaw since 1918, walked year by year through the ELI API.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eur_sejm_eli_du);

static int run_eur_sejm_eli_mp(const source_ctx *c, intel_sink *s) {
  return sejm_eli_walk(c, s, "eur-sejm-eli-mp", "MP");
}
static const source_def eur_sejm_eli_mp = {
  .id = "eur-sejm-eli-mp", .collector = "europe_gov",
  .name = "Sejm Poland — Monitor Polski, full archive",
  .name_ja = "Sejm Poland — Monitor Polski, full archive",
  .update_interval_sec = 604800, .run = run_eur_sejm_eli_mp,
  .category = "gazette", .type = "api",
  .url = "https://api.sejm.gov.pl/eli/acts/MP",
  .description = "Every act published in Monitor Polski, walked year by year through the ELI API.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eur_sejm_eli_mp);

/* Interpellation records carry their identity in `num` (no id field), so the
 * uid was a hash of (title, link, date) and repeated titles on different
 * offset pages collapsed: sweep 2026-09-14 2,000 emitted, 1,982 stored. A
 * 20-page walk 2026-09-15 returned 2,000 distinct num values. */
VJSON_KEYED(eur_sejm_term10_committees_sits, "eur-sejm-term10-committees-sits", "Sejm Poland — interpellations", "Sejm Poland — interpellations",
  "europe_gov", "politics",
  "https://api.sejm.gov.pl/sejm/term10/interpellations?limit=100&offset=0",
  "",
  "pl", "[\"pol\",\"politics\"]", 21600,
  "Written interpellations tabled by Polish MPs with recipient ministry.",
  "num");

VJSON(eur_sejm_term9_mp, "eur-sejm-term9-mp", "Sejm Poland — MPs of the 9th term", "Sejm Poland — MPs of the 9th term",
  "europe_gov", "politics",
  "https://api.sejm.gov.pl/sejm/term9/MP",
  "",
  "pl", "[\"pol\",\"politics\"]", 86400,
  "Deputies of the previous Polish Sejm term with club and constituency.");

VJSON(eur_ssb_klass2, "eur-ssb-klass2", "Statistics Norway KLASS — classifications", "Statistics Norway KLASS — classifications",
  "europe_gov", "statistics",
  "https://data.ssb.no/api/klass/v1/classifications.json?size=100",
  "_embedded.classifications",
  "no", "[\"nor\",\"statistics\"]", 86400,
  "Official Norwegian statistical classifications and code lists.");

VJSON(eur_ssb_klassfam, "eur-ssb-klassfam", "Statistics Norway KLASS — classification families", "Statistics Norway KLASS — classification families",
  "europe_gov", "statistics",
  "https://data.ssb.no/api/klass/v1/classificationfamilies.json",
  "_embedded.classificationfamilies",
  "no", "[\"nor\",\"statistics\"]", 86400,
  "Thematic families grouping Norwegian statistical classifications.");

VJSON(eur_statbank_subjects, "eur-statbank-subjects", "Statistics Denmark — subject tree", "Statistics Denmark — subject tree",
  "europe_gov", "statistics",
  "https://api.statbank.dk/v1/subjects?format=JSON&lang=en",
  "",
  "en", "[\"dnk\",\"statistics\"]", 86400,
  "Top-level subject tree of the Danish StatBank.");

VJSON(eur_statbank_tables, "eur-statbank-tables", "Statistics Denmark — table catalogue", "Statistics Denmark — table catalogue",
  "europe_gov", "statistics",
  "https://api.statbank.dk/v1/tables?format=JSON&lang=en",
  "",
  "en", "[\"dnk\",\"statistics\"]", 86400,
  "Every StatBank table with identifier, period coverage and update time.");

VJSON(eur_statfin_hinta, "eur-statfin-hinta", "Statistics Finland — consumer price tables", "Statistics Finland — consumer price tables",
  "europe_gov", "statistics",
  "https://pxdata.stat.fi/PxWeb/api/v1/en/StatFin/khi/",
  "",
  "en", "[\"fin\",\"statistics\"]", 86400,
  "Table inventory of the Finnish consumer price index.");

VJSON(eur_statfin_kans, "eur-statfin-kans", "Statistics Finland — migration tables", "Statistics Finland — migration tables",
  "europe_gov", "statistics",
  "https://pxdata.stat.fi/PxWeb/api/v1/en/StatFin/muutl/",
  "",
  "en", "[\"fin\",\"statistics\"]", 86400,
  "Table inventory of Finnish migration and citizenship statistics.");

VJSON(eur_statfin_rikos, "eur-statfin-rikos", "Statistics Finland — crime statistics tables", "Statistics Finland — crime statistics tables",
  "europe_gov", "statistics",
  "https://pxdata.stat.fi/PxWeb/api/v1/en/StatFin/rpk/",
  "",
  "en", "[\"fin\",\"statistics\"]", 86400,
  "Table inventory of Finnish recorded-crime statistics.");

VJSON(eur_statfin_tyti, "eur-statfin-tyti", "Statistics Finland — labour force survey tables", "Statistics Finland — labour force survey tables",
  "europe_gov", "statistics",
  "https://pxdata.stat.fi/PxWeb/api/v1/en/StatFin/tyti/",
  "",
  "en", "[\"fin\",\"statistics\"]", 86400,
  "Table inventory of the Finnish labour force survey.");

VJSON(eur_statfin_vaerak, "eur-statfin-vaerak", "Statistics Finland — population structure tables", "Statistics Finland — population structure tables",
  "europe_gov", "statistics",
  "https://pxdata.stat.fi/PxWeb/api/v1/en/StatFin/vaerak/",
  "",
  "en", "[\"fin\",\"statistics\"]", 86400,
  "Table inventory of the Finnish population structure statistics.");

VJSON(eur_statistics_sk_collection, "eur-statistics-sk-collection", "Statistics Slovakia — dataset collection", "Statistics Slovakia — dataset collection",
  "europe_gov", "statistics",
  "https://data.statistics.sk/api/v2/collection?lang=en",
  "link.item",
  "en", "[\"svk\",\"statistics\"]", 86400,
  "Machine-readable collection of Slovak official statistics datasets.");

VJSON(eur_statistics_sk_sk, "eur-statistics-sk-sk", "Statistics Slovakia — dataset collection (SK)", "Statistics Slovakia — dataset collection (SK)",
  "europe_gov", "statistics",
  "https://data.statistics.sk/api/v2/collection?lang=sk",
  "link.item",
  "sk", "[\"svk\",\"statistics\"]", 86400,
  "Slovak-language machine-readable collection of Slovak official statistics datasets.");

VJSON(eur_stortinget_emner, "eur-stortinget-emner", "Stortinget Norway — subject index", "Stortinget Norway — subject index",
  "europe_gov", "politics",
  "https://data.stortinget.no/eksport/emner?format=json",
  "emne_liste",
  "no", "[\"nor\",\"politics\"]", 86400,
  "Subject taxonomy used to classify Norwegian parliamentary business.");

VJSON(eur_stortinget_komiteer, "eur-stortinget-komiteer", "Stortinget Norway — sitting representatives", "Stortinget Norway — sitting representatives",
  "europe_gov", "politics",
  "https://data.stortinget.no/eksport/dagensrepresentanter?format=json",
  "dagensrepresentanter_liste",
  "no", "[\"nor\",\"politics\"]", 86400,
  "Currently sitting Norwegian MPs with party and constituency.");

/* eur-stortinget-moter2: Stortinget publishes one row per calendar day of the
 * session, and every day WITHOUT a sitting carries "id": -1 (with the reason in
 * ikke_motedag_tekst). Live-verified 2026-09-14: 155 records, 99 distinct "id"
 * values (-1 appears 57 times), 155 distinct mote_dato_tid and 155 distinct
 * (id, mote_dato_tid) pairs — the sweep measured 155 emitted, 99 stored. The
 * shared keyed emitter would DELETE the upstream "id" before relabelling, so
 * this keeps its value as "mote_id" first and then keys on the pair, both
 * values the upstream's own. Nothing is removed from the record. */
static int stortinget_moter_emit_page(const source_ctx *c, intel_sink *s,
                                      const char *id, cJSON *doc, void *ud,
                                      int *seen) {
  (void)c; (void)ud;
  cJSON *arr = jsonlist_find_array(doc, "moter_liste");
  if (arr) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (!cJSON_IsObject(rec)) continue;
      cJSON *mid = cJSON_GetObjectItemCaseSensitive(rec, "id");
      cJSON *dt  = cJSON_GetObjectItemCaseSensitive(rec, "mote_dato_tid");
      if (!cJSON_IsNumber(mid) || !cJSON_IsString(dt) || !dt->valuestring) continue;
      char buf[128];
      snprintf(buf, sizeof buf, "%.17g_%s", mid->valuedouble, dt->valuestring);
      if (!cJSON_GetObjectItemCaseSensitive(rec, "mote_id"))
        cJSON_AddNumberToObject(rec, "mote_id", mid->valuedouble);
      cJSON_DeleteItemFromObjectCaseSensitive(rec, "id");
      cJSON_AddStringToObject(rec, "id", buf);
    }
  }
  return jsonlist_emit_ex(s, id, doc, "moter_liste", "politics", "no",
                          "[\"nor\",\"politics\"]", seen);
}

static int run_eur_stortinget_moter2(const source_ctx *c, intel_sink *s) {
  int n = pw_walk(c, s, "eur-stortinget-moter2",
                  "https://data.stortinget.no/eksport/moter?format=json",
                  pw_fetch_json, stortinget_moter_emit_page, NULL);
  if (n < 0) {
    fprintf(stderr, "[eur-stortinget-moter2] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def eur_stortinget_moter2 = {
  .id = "eur-stortinget-moter2", .collector = "europe_gov",
  .name = "Stortinget Norway — sittings",
  .name_ja = "Stortinget Norway — sittings",
  .update_interval_sec = 21600, .run = run_eur_stortinget_moter2,
  .category = "politics", .type = "api",
  .url = "https://data.stortinget.no/eksport/moter?format=json",
  .description = "Scheduled and held sittings of the Norwegian parliament for the running session.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eur_stortinget_moter2);

VJSON(eur_stortinget_partier, "eur-stortinget-partier", "Stortinget Norway — parties", "Stortinget Norway — parties",
  "europe_gov", "politics",
  "https://data.stortinget.no/eksport/partier?format=json",
  "partier_liste",
  "no", "[\"nor\",\"politics\"]", 86400,
  "Political parties represented in the Norwegian parliament.");

VJSON(eur_stortinget_publikasjoner, "eur-stortinget-publikasjoner", "Stortinget Norway — verbatim reports", "Stortinget Norway — verbatim reports",
  "europe_gov", "politics",
  "https://data.stortinget.no/eksport/publikasjoner?format=json&publikasjontype=referat",
  "publikasjoner_liste",
  "no", "[\"nor\",\"politics\"]", 21600,
  "Published verbatim proceedings of the Norwegian parliament.");

VJSON(eur_stortinget_saker, "eur-stortinget-saker", "Stortinget Norway — parliamentary cases", "Stortinget Norway — parliamentary cases",
  "europe_gov", "politics",
  "https://data.stortinget.no/eksport/saker?format=json",
  "saker_liste",
  "no", "[\"nor\",\"politics\"]", 21600,
  "Cases before the Norwegian parliament with status and committee.");

VJSON(eur_tk_activiteit, "eur-tk-activiteit", "Tweede Kamer Netherlands — activities", "Tweede Kamer Netherlands — activities",
  "europe_gov", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Activiteit?$top=50&$format=json",
  "value",
  "nl", "[\"nld\",\"politics\"]", 21600,
  "Scheduled and completed activities of the Dutch House of Representatives.");

VJSON(eur_tk_agendapunt, "eur-tk-agendapunt", "Tweede Kamer Netherlands — agenda items", "Tweede Kamer Netherlands — agenda items",
  "europe_gov", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Agendapunt?$top=50&$format=json",
  "value",
  "nl", "[\"nld\",\"politics\"]", 21600,
  "Agenda items attached to Dutch parliamentary activities.");

VJSON(eur_tk_besluit, "eur-tk-besluit", "Tweede Kamer Netherlands — decisions", "Tweede Kamer Netherlands — decisions",
  "europe_gov", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Besluit?$top=50&$format=json&$orderby=GewijzigdOp%20desc",
  "value",
  "nl", "[\"nld\",\"politics\"]", 21600,
  "Decisions taken by the Dutch House of Representatives, newest first.");

VJSON(eur_tk_commissie, "eur-tk-commissie", "Tweede Kamer Netherlands — committees", "Tweede Kamer Netherlands — committees",
  "europe_gov", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Commissie?$top=100&$format=json",
  "value",
  "nl", "[\"nld\",\"politics\"]", 86400,
  "Committees of the Dutch House of Representatives.");

VJSON(eur_tk_fractie, "eur-tk-fractie", "Tweede Kamer Netherlands — parliamentary groups", "Tweede Kamer Netherlands — parliamentary groups",
  "europe_gov", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Fractie?$top=100&$format=json",
  "value",
  "nl", "[\"nld\",\"politics\"]", 86400,
  "Parliamentary groups in the Dutch House with seat counts.");

VJSON(eur_tk_vergadering, "eur-tk-vergadering", "Tweede Kamer Netherlands — plenary meetings", "Tweede Kamer Netherlands — plenary meetings",
  "europe_gov", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Vergadering?$top=50&$format=json",
  "value",
  "nl", "[\"nld\",\"politics\"]", 21600,
  "Plenary meetings of the Dutch House of Representatives.");

VJSON(eur_tk_verslag, "eur-tk-verslag", "Tweede Kamer Netherlands — room reservations", "Tweede Kamer Netherlands — room reservations",
  "europe_gov", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Reservering?$top=50&$format=json",
  "value",
  "nl", "[\"nld\",\"politics\"]", 21600,
  "Room reservations linking Dutch parliamentary activities to times and places.");

VJSON(eur_tk_zaal, "eur-tk-zaal", "Tweede Kamer Netherlands — meeting rooms", "Tweede Kamer Netherlands — meeting rooms",
  "europe_gov", "politics",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Zaal?$top=100&$format=json",
  "value",
  "nl", "[\"nld\",\"politics\"]", 86400,
  "Rooms used for Dutch parliamentary meetings, needed to resolve activity locations.");
