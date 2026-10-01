/* Verified-live legal sources (60), part 2.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

/* Oireachtas API records are WRAPPED: every element of "results" is
 * {"member":{memberCode, fullName, uri, …}}, {"party":{partyCode, showAs,
 * uri}, "house":{…}} or {"debateRecord":{uri, date, …}, "contextDate"} — the
 * record's own fields sit one level down under a key jsonlist's envelope
 * descent does not know. Measured live 2026-09-06: members and parties have
 * NO top-level scalar at all, so every record was dropped as unlabelled
 * (EMITS_NOTHING); debates do have one, "contextDate", so each debate was
 * labelled by its date alone and the debates of one sitting day collapsed
 * onto one uid (emitted 30, stored 12).
 *
 * jl_emit_page_lifted surfaces the inner object's own identifier, label and
 * date onto the record as "id" / "title" / "date" — values that are already
 * on the record, copied, not composed — and hands the page to the shared
 * jsonlist_emit_ex so paging, disclosure and the collision guard are
 * unchanged. Nothing is renamed to a value not present upstream. (The same
 * helper is duplicated in vsrc_security_1.c for Alpine's {"pkg":{…}} shape;
 * the generic fix belongs in lib/jsonlist.c's envelope descent.) */
typedef struct {
  const char *path, *record_type, *lang, *tags_json;
  const char *wrapper, *id_field, *title_field, *date_field;
} jl_lift_opts;

static void jl_lift_one(cJSON *rec, cJSON *inner, const char *from, const char *to) {
  if (!from) return;
  cJSON *v = cJSON_GetObjectItemCaseSensitive(inner, from);
  if (!cJSON_IsString(v) || !v->valuestring || !v->valuestring[0]) return;
  if (cJSON_GetObjectItemCaseSensitive(rec, to)) return;   /* upstream wins */
  cJSON_AddStringToObject(rec, to, v->valuestring);
}

static int jl_emit_page_lifted(const source_ctx *c, intel_sink *s,
                               const char *id, cJSON *doc, void *ud,
                               int *seen) {
  (void)c;
  jl_lift_opts *o = (jl_lift_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  if (arr) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (!cJSON_IsObject(rec)) continue;
      cJSON *inner = cJSON_GetObjectItemCaseSensitive(rec, o->wrapper);
      if (!cJSON_IsObject(inner)) continue;
      jl_lift_one(rec, inner, o->id_field, "id");
      jl_lift_one(rec, inner, o->title_field, "title");
      jl_lift_one(rec, inner, o->date_field, "date");
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->record_type, o->lang,
                          o->tags_json, seen);
}

/* As VJSON, plus the wrapper key and the inner field names to surface. Each
 * named field must have been seen in a live response of THIS endpoint. */
#define VJSON_LIFTED(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, PATH, LANG, TAGS, IVAL, DESC, WRAPPER, IDF, TITLEF, DATEF) \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                  \
    jl_lift_opts o = { PATH, CAT, LANG, TAGS, WRAPPER, IDF, TITLEF, DATEF };  \
    int n = pw_walk(c, s, ID, URL, pw_fetch_json, jl_emit_page_lifted, &o);   \
    if (n < 0) { fprintf(stderr, "[%s] fetch failed\n", ID); return -1; }     \
    return 0; }                                                               \
  static const source_def SYM = {                                            \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,             \
    .update_interval_sec = IVAL, .run = run_##SYM,                            \
    .category = CAT, .type = "api", .url = URL,                               \
    .description = DESC, .layer = NULL, .free_tier = 1 };                     \
  REGISTER_SOURCE(SYM)

VJSON(gov_il_knesset_plenum, "gov-il-knesset-plenum", "Knesset — Plenum Sessions", "クネセト 本会議",
  "legal", "parliament",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_PlenumSession?$top=50&$format=json",
  "value",
  "he", "[\"israel\",\"parliament\",\"sessions\"]", 3600,
  "Plenum sessions of the Israeli Knesset with dates and agenda linkage.");

VJSON(gov_il_knesset_votes, "gov-il-knesset-votes", "Knesset — Member Vote Results", "クネセト 議員別採決結果",
  "legal", "parliament",
  "https://knesset.gov.il/Odata/Votes.svc/vote_rslts_kmmbr_shadow?$top=50&$format=json",
  "value",
  "he", "[\"israel\",\"parliament\",\"votes\"]", 3600,
  "Per-member vote results in the Israeli Knesset from the official votes OData service.");

/* SPARQL JSON results: every binding is {s:{type,value}, p:{…}, o:{…}} with no
 * scalar, so all 20 were dropped as unlabelled (live 2026-09-15). vshape_sparql
 * lifts each variable's value beside its binding; the triple is the key. */
#include "_vjson_shapes.inc"
static void camera_sparql_prep(cJSON *doc) { vshape_sparql(doc, "results.bindings"); }

VJSON_PREP(gov_it_camera_sparql, "gov-it-camera-sparql", "Camera dei Deputati — Linked Open Data", "イタリア下院 リンクト・オープンデータ",
  "legal", "parliament",
  "https://dati.camera.it/sparql?query=SELECT%20*%20WHERE%20%7B%3Fs%20%3Fp%20%3Fo%7D%20LIMIT%2020&format=application%2Fsparql-results%2Bjson",
  "results.bindings",
  "it", "[\"italy\",\"parliament\",\"sparql\",\"linkeddata\"]", 86400,
  "SPARQL endpoint over the Italian Chamber of Deputies linked open data on members and bills.",
  "s+p+o", camera_sparql_prep);

VRSS(gov_it_gazzetta_s1, "gov-it-gazzetta-s1", "Gazzetta Ufficiale — Constitutional Court Series", "イタリア官報 憲法裁判所特別号",
  "legal", "gazette",
  "https://www.gazzettaufficiale.it/rss/S1",
  "it", "[\"italy\",\"gazette\",\"constitutionalcourt\"]", 86400,
  "Italian Official Gazette special series carrying Constitutional Court rulings and referrals.");

VJSON(gov_jp_egov_laws, "gov-jp-egov-laws", "e-Gov — Japanese Statute Database", "e-Gov 法令データベース",
  "legal", "parliament",
  "https://laws.e-gov.go.jp/api/2/laws?limit=50",
  "laws",
  "ja", "[\"japan\",\"legislation\",\"statute\",\"egov\"]", 86400,
  "Official Japanese e-Gov statute API listing laws with promulgation number and effective date.");

VJSON(gov_jp_kokkai_meetings, "gov-jp-kokkai-meetings", "National Diet — Meeting Records API", "国会会議録検索 会議一覧",
  "legal", "parliament",
  "https://kokkai.ndl.go.jp/api/meeting_list?nameOfHouse=%E8%A1%86%E8%AD%B0%E9%99%A2&maximumRecords=10&recordPacking=json",
  "meetingrecord",
  "ja", "[\"japan\",\"parliament\",\"diet\",\"hansard\"]", 3600,
  "National Diet Library API listing House of Representatives meeting records with committee and date.");

VJSON(gov_jp_kokkai_speech, "gov-jp-kokkai-speech", "National Diet — Speech Search API", "国会会議録検索 発言検索",
  "legal", "parliament",
  "https://kokkai.ndl.go.jp/api/speech?any=%E4%BA%88%E7%AE%97&maximumRecords=10&recordPacking=json",
  "speechrecord",
  "ja", "[\"japan\",\"parliament\",\"diet\",\"hansard\"]", 3600,
  "Full-text search over verbatim National Diet speeches with speaker, party and committee.");

VRSS(gov_judiciary_uk, "gov-judiciary-uk", "Judiciary of England and Wales — Judgments", "英国司法府 判決",
  "legal", "courts",
  "https://www.judiciary.uk/feed/",
  "en", "[\"uk\",\"courts\",\"judgments\",\"rulings\"]", 3600,
  "Judgments, sentencing remarks and coroner reports published by the Judiciary of England and Wales.");

VRSS(gov_justice_news, "gov-justice-news", "US Department of Justice — All News", "米国司法省 ニュース",
  "legal", "courts",
  "https://www.justice.gov/news/rss",
  "en", "[\"usa\",\"doj\",\"prosecution\",\"enforcement\"]", 3600,
  "All Department of Justice news items: indictments, settlements, policy and leadership announcements.");

VJSON(gov_kr_law_search, "gov-kr-law-search", "Korea Law Information — Statute Search", "韓国法令情報 法令検索",
  "legal", "parliament",
  "https://www.law.go.kr/DRF/lawSearch.do?OC=test&target=law&type=JSON&display=20",
  "lawsearch.law",
  "ko", "[\"korea\",\"legislation\",\"statute\",\"asia\"]", 86400,
  "Korean national statute search API returning law names, ministries and effective dates.");

VJSON(gov_nl_tweedekamer_besluit, "gov-nl-tweedekamer-besluit", "Tweede Kamer — Decisions", "オランダ下院 決定",
  "legal", "parliament",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Besluit?$top=20&$format=json",
  "value",
  "nl", "[\"netherlands\",\"parliament\",\"decisions\"]", 3600,
  "Decisions taken by the Dutch House of Representatives on tabled business.");

VJSON(gov_nl_tweedekamer_document, "gov-nl-tweedekamer-document", "Tweede Kamer — Parliamentary Documents", "オランダ下院 議会文書",
  "legal", "parliament",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Document?$top=20&$format=json",
  "value",
  "nl", "[\"netherlands\",\"parliament\",\"documents\"]", 3600,
  "Documents tabled in the Dutch House of Representatives via the official OData magazine.");

VJSON(gov_nl_tweedekamer_persoon, "gov-nl-tweedekamer-persoon", "Tweede Kamer — Person Registry", "オランダ下院 人物登録",
  "legal", "parliament",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Persoon?$top=20&$format=json",
  "value",
  "nl", "[\"netherlands\",\"parliament\",\"members\",\"pep\"]", 86400,
  "Registry of persons appearing in Dutch parliamentary records: MPs, ministers and officials.");

VJSON(gov_nl_tweedekamer_stemming, "gov-nl-tweedekamer-stemming", "Tweede Kamer — Votes", "オランダ下院 採決",
  "legal", "parliament",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Stemming?$top=20&$format=json",
  "value",
  "nl", "[\"netherlands\",\"parliament\",\"votes\"]", 3600,
  "Vote records in the Dutch House of Representatives by party and motion.");

VJSON(gov_nl_tweedekamer_zaak, "gov-nl-tweedekamer-zaak", "Tweede Kamer — Parliamentary Cases", "オランダ下院 議案",
  "legal", "parliament",
  "https://gegevensmagazijn.tweedekamer.nl/OData/v4/2.0/Zaak?$top=20&$format=json",
  "value",
  "nl", "[\"netherlands\",\"parliament\",\"cases\"]", 3600,
  "Cases (zaken) before the Dutch House of Representatives with type and status.");

VJSON(gov_no_stortinget_komiteer, "gov-no-stortinget-komiteer", "Stortinget — Committees", "ノルウェー議会 委員会",
  "legal", "parliament",
  "https://data.stortinget.no/eksport/komiteer?format=json",
  "komiteer_liste",
  "no", "[\"norway\",\"parliament\",\"committees\"]", 604800,
  "Standing committees of the Norwegian Storting.");

VJSON(gov_no_stortinget_representanter, "gov-no-stortinget-representanter", "Stortinget — Representatives", "ノルウェー議会 議員",
  "legal", "parliament",
  "https://data.stortinget.no/eksport/representanter?format=json",
  "representanter_liste",
  "no", "[\"norway\",\"parliament\",\"members\",\"pep\"]", 86400,
  "Sitting members of the Norwegian Storting with party and county.");

VJSON(gov_no_stortinget_saker, "gov-no-stortinget-saker", "Stortinget — Parliamentary Cases", "ノルウェー議会 議案",
  "legal", "parliament",
  "https://data.stortinget.no/eksport/saker?format=json&sesjonid=2025-2026",
  "saker_liste",
  "no", "[\"norway\",\"parliament\",\"bills\"]", 3600,
  "All cases before the Norwegian Storting for the current session with committee and status.");

VJSON(gov_no_stortinget_sesjoner, "gov-no-stortinget-sesjoner", "Stortinget — Sessions", "ノルウェー議会 会期",
  "legal", "parliament",
  "https://data.stortinget.no/eksport/sesjoner?format=json",
  "sesjoner_liste",
  "no", "[\"norway\",\"parliament\",\"sessions\"]", 604800,
  "Historic and current Storting sessions, the key needed to page other Storting exports.");

/* debateRecord.uri is the record's Akoma Ntoso identifier (10 of 10 distinct
 * on the live page, live 2026-09-06); debateRecord.date is its sitting date.
 * No inner field is a human title, so the label stays the engine's
 * "<record_type> <id>" fallback. See jl_emit_page_lifted above. */
VJSON_LIFTED(gov_oireachtas_debates, "gov-oireachtas-debates", "Oireachtas — Debate Records", "アイルランド議会 議事録",
  "legal", "parliament",
  "https://api.oireachtas.ie/v1/debates?limit=10",
  "results",
  "en", "[\"ireland\",\"parliament\",\"hansard\",\"debates\"]", 3600,
  "Irish parliamentary debate records with speaker attribution and full text links.",
  "debateRecord", "uri", NULL, "date");

VJSON(gov_oireachtas_divisions, "gov-oireachtas-divisions", "Oireachtas — Division Votes", "アイルランド議会 採決",
  "legal", "parliament",
  "https://api.oireachtas.ie/v1/divisions?limit=25",
  "results",
  "en", "[\"ireland\",\"parliament\",\"votes\"]", 3600,
  "Recorded division votes in the Dáil and Seanad.");

VJSON(gov_oireachtas_legislation, "gov-oireachtas-legislation", "Oireachtas — Irish Legislation", "アイルランド議会 立法",
  "legal", "parliament",
  "https://api.oireachtas.ie/v1/legislation?limit=50",
  "results",
  "en", "[\"ireland\",\"parliament\",\"bills\",\"legislation\"]", 3600,
  "Bills and acts before the Irish Oireachtas with stage history and sponsor.");

/* {"member":{memberCode, fullName, …}}: memberCode is the Oireachtas member
 * identifier, fullName the member's name (live 2026-09-06, 1,928 members). */
VJSON_LIFTED(gov_oireachtas_members, "gov-oireachtas-members", "Oireachtas — Members", "アイルランド議会 議員",
  "legal", "parliament",
  "https://api.oireachtas.ie/v1/members?limit=50",
  "results",
  "en", "[\"ireland\",\"parliament\",\"members\",\"pep\"]", 86400,
  "Members of the Dáil and Seanad with party, constituency and terms.",
  "member", "memberCode", "fullName", NULL);

/* {"party":{partyCode, showAs, uri}, "house":{…}}: uri is unique per party
 * per house (partyCode alone repeats across houses); showAs is the name. */
VJSON_LIFTED(gov_oireachtas_parties, "gov-oireachtas-parties", "Oireachtas — Political Parties", "アイルランド議会 政党一覧",
  "legal", "parliament",
  "https://api.oireachtas.ie/v1/parties?limit=25",
  "results",
  "en", "[\"ireland\",\"parliament\",\"parties\"]", 604800,
  "Registry of political parties represented in the Irish Oireachtas.",
  "party", "uri", "showAs", NULL);

VJSON(gov_oireachtas_questions, "gov-oireachtas-questions", "Oireachtas — Parliamentary Questions", "アイルランド議会 質問",
  "legal", "parliament",
  "https://api.oireachtas.ie/v1/questions?limit=25",
  "results",
  "en", "[\"ireland\",\"parliament\",\"questions\",\"oversight\"]", 3600,
  "Parliamentary questions and ministerial answers in the Irish Oireachtas.");

/* gov-opensanctions-crime: the collection index's "children" (and
 * "datasets") are arrays of 62 dataset NAME STRINGS, not objects, so the
 * declared path yielded no records and the row emitted nothing on every run.
 * The only object array in the document is "resources" — the collection's
 * published artefacts {name, title, mime_type, size, checksum, url, path}, 5
 * records live 2026-09-06 — so that is what this row now emits. The catalogue
 * that carries the child datasets AS objects (datasets/latest/index.json) is
 * already collected by vsrc_government_3.c and hp_sanctions_deep.c. */
VJSON(gov_opensanctions_crime, "gov-opensanctions-crime", "OpenSanctions — Crime and Wanted Lists", "OpenSanctions 犯罪・手配リスト",
  "legal", "watchlist",
  "https://data.opensanctions.org/datasets/latest/crime/index.json",
  "resources",
  "en", "[\"crime\",\"wanted\",\"global\"]", 86400,
  "Published resource files (FollowTheMoney entities, names, targets) of the OpenSanctions crime collection, with size and checksum per build.");

VRSS(gov_pacer_akd, "gov-pacer-akd", "PACER — District of Alaska Docket Feed", "米連邦地裁 アラスカ 訴訟記録",
  "legal", "courts",
  "https://ecf.akd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Alaska.");

VRSS(gov_pacer_azd, "gov-pacer-azd", "PACER — District of Arizona Docket Feed", "米連邦地裁 アリゾナ 訴訟記録",
  "legal", "courts",
  "https://ecf.azd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Arizona.");

VRSS(gov_pacer_cacd, "gov-pacer-cacd", "PACER — C.D. California Docket Feed", "米連邦地裁 カリフォルニア中部 訴訟記録",
  "legal", "courts",
  "https://ecf.cacd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Central District of California.");

VRSS(gov_pacer_caed, "gov-pacer-caed", "PACER — E.D. California Docket Feed", "米連邦地裁 カリフォルニア東部 訴訟記録",
  "legal", "courts",
  "https://ecf.caed.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Eastern District of California.");

VRSS(gov_pacer_casd, "gov-pacer-casd", "PACER — S.D. California Docket Feed", "米連邦地裁 カリフォルニア南部 訴訟記録",
  "legal", "courts",
  "https://ecf.casd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Southern District of California.");

VRSS(gov_pacer_cod, "gov-pacer-cod", "PACER — District of Colorado Docket Feed", "米連邦地裁 コロラド 訴訟記録",
  "legal", "courts",
  "https://ecf.cod.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Colorado.");

VRSS(gov_pacer_ctd, "gov-pacer-ctd", "PACER — District of Connecticut Docket Feed", "米連邦地裁 コネチカット 訴訟記録",
  "legal", "courts",
  "https://ecf.ctd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Connecticut.");

VRSS(gov_pacer_ded, "gov-pacer-ded", "PACER — District of Delaware Docket Feed", "米連邦地裁 デラウェア 訴訟記録",
  "legal", "courts",
  "https://ecf.ded.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\",\"corporate\"]", 1800,
  "Live docket entries from the District of Delaware, the main US corporate and IP litigation venue.");

VRSS(gov_pacer_flmd, "gov-pacer-flmd", "PACER — M.D. Florida Docket Feed", "米連邦地裁 フロリダ中部 訴訟記録",
  "legal", "courts",
  "https://ecf.flmd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Middle District of Florida.");

VRSS(gov_pacer_flsd, "gov-pacer-flsd", "PACER — S.D. Florida Docket Feed", "米連邦地裁 フロリダ南部 訴訟記録",
  "legal", "courts",
  "https://ecf.flsd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Southern District of Florida.");

VRSS(gov_pacer_hid, "gov-pacer-hid", "PACER — District of Hawaii Docket Feed", "米連邦地裁 ハワイ 訴訟記録",
  "legal", "courts",
  "https://ecf.hid.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Hawaii.");

VRSS(gov_pacer_ilnd, "gov-pacer-ilnd", "PACER — N.D. Illinois Docket Feed", "米連邦地裁 イリノイ北部 訴訟記録",
  "legal", "courts",
  "https://ecf.ilnd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Northern District of Illinois.");

VRSS(gov_pacer_innd, "gov-pacer-innd", "PACER — N.D. Indiana Docket Feed", "米連邦地裁 インディアナ北部 訴訟記録",
  "legal", "courts",
  "https://ecf.innd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Northern District of Indiana.");

VRSS(gov_pacer_laed, "gov-pacer-laed", "PACER — E.D. Louisiana Docket Feed", "米連邦地裁 ルイジアナ東部 訴訟記録",
  "legal", "courts",
  "https://ecf.laed.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Eastern District of Louisiana.");

VRSS(gov_pacer_mad, "gov-pacer-mad", "PACER — District of Massachusetts Docket Feed", "米連邦地裁 マサチューセッツ 訴訟記録",
  "legal", "courts",
  "https://ecf.mad.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Massachusetts.");

VRSS(gov_pacer_mied, "gov-pacer-mied", "PACER — E.D. Michigan Docket Feed", "米連邦地裁 ミシガン東部 訴訟記録",
  "legal", "courts",
  "https://ecf.mied.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Eastern District of Michigan.");

VRSS(gov_pacer_moed, "gov-pacer-moed", "PACER — E.D. Missouri Docket Feed", "米連邦地裁 ミズーリ東部 訴訟記録",
  "legal", "courts",
  "https://ecf.moed.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Eastern District of Missouri.");

VRSS(gov_pacer_mowd, "gov-pacer-mowd", "PACER — W.D. Missouri Docket Feed", "米連邦地裁 ミズーリ西部 訴訟記録",
  "legal", "courts",
  "https://ecf.mowd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Western District of Missouri.");

VRSS(gov_pacer_mtd, "gov-pacer-mtd", "PACER — District of Montana Docket Feed", "米連邦地裁 モンタナ 訴訟記録",
  "legal", "courts",
  "https://ecf.mtd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Montana.");

VRSS(gov_pacer_nced, "gov-pacer-nced", "PACER — E.D. North Carolina Docket Feed", "米連邦地裁 ノースカロライナ東部 訴訟記録",
  "legal", "courts",
  "https://ecf.nced.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Eastern District of North Carolina.");

VRSS(gov_pacer_njd, "gov-pacer-njd", "PACER — District of New Jersey Docket Feed", "米連邦地裁 ニュージャージー 訴訟記録",
  "legal", "courts",
  "https://ecf.njd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\",\"pharma\"]", 1800,
  "Live docket entries from the District of New Jersey, a key pharmaceutical litigation venue.");

VRSS(gov_pacer_nvd, "gov-pacer-nvd", "PACER — District of Nevada Docket Feed", "米連邦地裁 ネバダ 訴訟記録",
  "legal", "courts",
  "https://ecf.nvd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Nevada.");

VRSS(gov_pacer_ohnd, "gov-pacer-ohnd", "PACER — N.D. Ohio Docket Feed", "米連邦地裁 オハイオ北部 訴訟記録",
  "legal", "courts",
  "https://ecf.ohnd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Northern District of Ohio.");

VRSS(gov_pacer_ohsd, "gov-pacer-ohsd", "PACER — S.D. Ohio Docket Feed", "米連邦地裁 オハイオ南部 訴訟記録",
  "legal", "courts",
  "https://ecf.ohsd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Southern District of Ohio.");

VRSS(gov_pacer_ord, "gov-pacer-ord", "PACER — District of Oregon Docket Feed", "米連邦地裁 オレゴン 訴訟記録",
  "legal", "courts",
  "https://ecf.ord.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Oregon.");

VRSS(gov_pacer_paed, "gov-pacer-paed", "PACER — E.D. Pennsylvania Docket Feed", "米連邦地裁 ペンシルベニア東部 訴訟記録",
  "legal", "courts",
  "https://ecf.paed.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Eastern District of Pennsylvania.");

VRSS(gov_pacer_pawd, "gov-pacer-pawd", "PACER — W.D. Pennsylvania Docket Feed", "米連邦地裁 ペンシルベニア西部 訴訟記録",
  "legal", "courts",
  "https://ecf.pawd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Western District of Pennsylvania.");

VRSS(gov_pacer_prd, "gov-pacer-prd", "PACER — District of Puerto Rico Docket Feed", "米連邦地裁 プエルトリコ 訴訟記録",
  "legal", "courts",
  "https://ecf.prd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Puerto Rico.");

VRSS(gov_pacer_scd, "gov-pacer-scd", "PACER — District of South Carolina Docket Feed", "米連邦地裁 サウスカロライナ 訴訟記録",
  "legal", "courts",
  "https://ecf.scd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of South Carolina.");

VRSS(gov_pacer_tnmd, "gov-pacer-tnmd", "PACER — M.D. Tennessee Docket Feed", "米連邦地裁 テネシー中部 訴訟記録",
  "legal", "courts",
  "https://ecf.tnmd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Middle District of Tennessee.");

VRSS(gov_pacer_txed, "gov-pacer-txed", "PACER — E.D. Texas Docket Feed", "米連邦地裁 テキサス東部 訴訟記録",
  "legal", "courts",
  "https://ecf.txed.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\",\"patent\"]", 1800,
  "Live docket entries from the Eastern District of Texas, the leading US patent litigation venue.");

VRSS(gov_pacer_txsd, "gov-pacer-txsd", "PACER — S.D. Texas Docket Feed", "米連邦地裁 テキサス南部 訴訟記録",
  "legal", "courts",
  "https://ecf.txsd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the Southern District of Texas.");

VRSS(gov_pacer_txwd, "gov-pacer-txwd", "PACER — W.D. Texas Docket Feed", "米連邦地裁 テキサス西部 訴訟記録",
  "legal", "courts",
  "https://ecf.txwd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\",\"patent\"]", 1800,
  "Live docket entries from the Western District of Texas, a major patent and immigration venue.");

VRSS(gov_pacer_utd, "gov-pacer-utd", "PACER — District of Utah Docket Feed", "米連邦地裁 ユタ 訴訟記録",
  "legal", "courts",
  "https://ecf.utd.uscourts.gov/cgi-bin/rss_outside.pl",
  "en", "[\"usa\",\"courts\",\"dockets\",\"pacer\"]", 1800,
  "Live docket entries filed in the US District Court for the District of Utah.");
