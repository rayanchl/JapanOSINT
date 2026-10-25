/* collectors/pivot/table/hp3b31_eunat.c — batch 31: eunat — European national public sector.
 *
 * Europe's company registers are already wired elsewhere in this tree. What was
 * missing is the layer that explains *why* a company appears in a register at
 * all: the national procurement portal that awarded it a contract, the
 * parliament that debated the file, the lobby register that names who asked
 * for the rule, and the official gazette where the decision took legal effect.
 *
 * A striking amount of this is a proper API rather than a portal. The Danish
 * Folketing, the Swedish Riksdag, the Norwegian Storting, the Polish Sejm, the
 * Swiss Parliament and the German Bundestag all publish OData or JSON services
 * covering members, documents, votes and committee work — and almost nothing
 * queries them.
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

static const hp_source HP3B31_EUNAT[] = {
  /* ── Germany, Austria, Switzerland ────────────────────────────────────── */
  { .id = "DE_BUNDESTAG_DIP", .name = "Germany Bundestag DIP — parliamentary process API",
    .name_ja = "ドイツ連邦議会 議事情報API", .category = "government",
    .portal = "https://search.dip.bundestag.de", .record_type = "de-parliamentary-record",
    .tags = "\"de\",\"parliament\",\"legislation\"", .key_env = "BUNDESTAG_DIP_API_KEY",
    .free_tier = 1,
    .url = "https://search.dip.bundestag.de/api/v1/vorgang?f.titel={q}&apikey={key}",
    .array_path = "documents", .title_keys = "titel,vorgangstyp",
    .id_keys = "id", .date_keys = "datum",
    .next_path = "cursor", .page_max = 30,
    .description = "The Bundestag's documentation and information system — "
      "bills, small and large parliamentary questions, committee referrals and "
      "the answers. German ministries disclose enforcement counts, contract "
      "detail and incident data almost exclusively through question answers" },

  { .id = "AT_LOBBYING_REGISTER", .name = "Austria — lobbying and interest representation register",
    .name_ja = "オーストリア ロビー登録簿", .category = "government",
    .portal = "https://www.lobbyreg.justiz.gv.at", .record_type = "at-lobbyist",
    .tags = "\"at\",\"lobbying\",\"influence\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.lobbyreg.justiz.gv.at/edikte/ll/lobbyreg.nsf/suche?"
      "openform&subf={q}",
    .base = "https://www.lobbyreg.justiz.gv.at", .filter_query = 1,
    .description = "Austria's statutory register held by the Ministry of "
      "Justice — lobbying firms and their clients, in-house lobbyists, "
      "self-governing bodies and associations, with the declared annual "
      "lobbying expenditure and the code of conduct each has signed" },

  /* ── France, Benelux ──────────────────────────────────────────────────── */
  { .id = "FR_BOAMP_NOTICES", .name = "France BOAMP — public contract notices",
    .name_ja = "フランス 公共調達公告", .category = "government",
    .portal = "https://www.boamp.fr", .record_type = "fr-tender",
    .tags = "\"fr\",\"procurement\"", .free_tier = 1,
    .url = "https://www.boamp.fr/api/explore/v2.1/catalog/datasets/boamp/records"
      "?where=search(%22{q}%22)&limit=100",
    .array_path = "results", .title_keys = "objet,nomacheteur",
    .id_keys = "idweb", .date_keys = "dateparution",
    .page_param = "offset", .page_size = 100, .page_start = 0, .page_max = 40,
    .description = "The Bulletin officiel des annonces de marchés publics — "
      "French tender notices and award results with the buyer, the object, the "
      "procedure, the estimated value and the successful supplier" },

  { .id = "FR_AN_QUESTIONS_ECRITES",
    .name = "France Assemblée nationale — written questions to ministers",
    .name_ja = "フランス国民議会 書面質問",
    .category = "government",
    .portal = "https://data.assemblee-nationale.fr/questions/questions-ecrites",
    .record_type = "fr-parliamentary-question",
    .tags = "\"fr\",\"parliament\",\"question\",\"minister\",\"open-data\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "https://data.assemblee-nationale.fr/static/openData/repository/17/"
           "questions/questions_ecrites/Questions_ecrites.json.zip",
    .zip_all_entries = 1,
    .array_path = "question",
    .title_keys = "indexationAN.analyses.analyse,indexationAN.rubrique",
    .id_keys = "uid",
    .body_keys = "textesQuestion.texteQuestion.texte",
    .date_keys = "textesQuestion.texteQuestion.infoJO.dateJO",
    .interval = 604800,
    .timeout_ms = 180000,
    .description = "Every written question a French deputy has put to a "
      "minister this legislature, with the minister's answer where one has "
      "been given: the question's identifier and number, the subject heading "
      "the Assemblée indexes it under, the deputy who asked it with their "
      "parliamentary group, the ministry it was addressed to, the Journal "
      "officiel issue and page it was published in, and the full text. A "
      "written question is the cheapest way a deputy puts a company, a site "
      "closure or a contract on the public record, so the text is a primary "
      "source naming firms that no company register would." },

  { .id = "NL_DATA_OVERHEID_CKAN", .name = "Netherlands data.overheid.nl — dataset catalog",
    .name_ja = "オランダ オープンデータ目録", .category = "government",
    .portal = "https://data.overheid.nl", .record_type = "nl-dataset",
    .tags = "\"nl\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://data.overheid.nl/data/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "The Dutch national data register — municipal permit and "
      "subsidy registers, the WOO (freedom of information) publication "
      "platform exports, environmental monitoring and the base registries that "
      "underpin Dutch administration" },

  /* ── Nordics ──────────────────────────────────────────────────────────── */
  { .id = "SE_RIKSDAGEN_DOCUMENTS", .name = "Sweden Riksdag — document API",
    .name_ja = "スウェーデン議会 文書API", .category = "government",
    .portal = "https://data.riksdagen.se", .record_type = "se-parliamentary-record",
    .tags = "\"se\",\"parliament\",\"legislation\"", .free_tier = 1,
    .url = "https://data.riksdagen.se/dokumentlista/?sok={q}&utformat=json&sz=200",
    .array_path = "dokumentlista.dokument", .title_keys = "titel,organ",
    .id_keys = "id", .date_keys = "datum", .link_keys = "dokument_url_html",
    .page_param = "p", .page_max = 30,
    .description = "The Riksdag's full document API — bills, motions, "
      "committee reports, written questions and the government's answers, plus "
      "the members' declared assets and side-income filings" },

  { .id = "NO_STORTINGET_DATA", .name = "Norway Storting — parliamentary data service",
    .name_ja = "ノルウェー議会 データサービス", .category = "government",
    .portal = "https://data.stortinget.no", .record_type = "no-parliamentary-record",
    .tags = "\"no\",\"parliament\"", .free_tier = 1,
    /* RE-POINTED 2026-10-21 (open item B2), and a change of KIND as well as of
     * path: interval 0 to daily. The scraped search path 404s, and the
     * Storting publishes a documented JSON export instead — but that export
     * takes a SESSION, not an entity (`?sakid=` alone answers HTTP 500), so it
     * cannot answer "what do we have on X" and must not stay an interval-0
     * pivot that nothing would ever run (house rule 3). The openFDA nine went
     * the other way and CLAUDE.md records what that cost.
     *
     * AND THE OBVIOUS REPLACEMENT WAS ALREADY REGISTERED. The triage named
     * /eksport/saker; `make lint-sources` answered dup-endpoint, because
     * `eur-stortinget-saker` (collectors/feed/generated/vsrc13_europe_gov_2.c)
     * has read exactly that URL every six hours all along — measured today,
     * 197 records, rc=0. Registering it again would have been the data.gov.uk
     * PROXY row over again: two rows, one upstream, no new data. **Run the
     * dup-endpoint lint before believing a re-point is a new source.**
     *
     * So this row takes an export the tree does NOT read, and the one its own
     * description already promised ("committee hearings"): /eksport/horinger,
     * 76 hearings in 83 KB, measured 2026-10-21. Of the nine Stortinget
     * exports registered in this tree, three live ones were missing —
     * horinger, interpellasjoner and sporretimesporsmal — and the other two
     * are the rows beneath this one.
     *
     * A hearing's subject lives one level down, in the case it is about, so
     * the title is a dotted path through the flattened record. */
    .url = "https://data.stortinget.no/eksport/horinger?format=json",
    .interval = 86400,
    .array_path = "horinger_liste",
    .title_keys = "horing_sak_info_liste.0.sak_korttittel,komite.navn",
    .id_keys = "id",
    .description = "Every committee hearing of the Norwegian parliament — the "
      "committee holding it, the case it concerns, the deadline for written "
      "submissions and whether it is written-only. Who is invited to be heard "
      "on a bill is the earliest public signal of who is lobbying it" },

  /* The one file of France's parliamentary open data the engine can read
   * today (open item B2, 2026-10-22). The row above this beat's FR pivot
   * stays failing honestly: its register is the debates and amendments, which
   * data.assemblee-nationale.fr publishes as CSV or JSON INSIDE A ZIP, and no
   * engine mode reads that (lib/zipread.c exists but only HP_XLSX uses it).
   *
   * MEASURED 2026-10-22: HTTP 200, 109,317 bytes, text/csv, 2,077 data rows,
   * SEMICOLON-delimited (5 fields on semicolons, 1 on commas) and NOT UTF-8 —
   * windows-1252, which is why a UTF-8 reader sees "Identifiant du d?put?".
   * One deputy holds many staff, so the identity is the (deputy, collaborator)
   * tuple and not the deputy: keying on the deputy alone would collapse 2,077
   * rows onto the ~577 who have staff.
   *
   * `index_url` is not optional here. The path carries `/17/` — the
   * LEGISLATURE number — which is a value frozen into a URL exactly like a
   * date, and it will 404 when the 18th begins. The portal's own
   * deputies-in-office page links the current file, so the engine follows it
   * every run; `.url` is the edition seen today, which the engine replaces. */
  { .id = "FR_AN_DEPUTY_STAFF",
    .name = "France Assemblée nationale — deputies' parliamentary staff",
    .name_ja = "フランス国民議会 議員秘書", .category = "government",
    .portal = "https://data.assemblee-nationale.fr",
    .record_type = "fr-parliamentary-staff",
    .tags = "\"fr\",\"parliament\",\"staff\"", .mode = HP_CSV, .free_tier = 1,
    .url = "https://data.assemblee-nationale.fr/static/openData/repository/17/"
      "amo/collaborateurs_csv_opendata/liste_collaborateurs_excel.csv",
    .index_url = "https://data.assemblee-nationale.fr/acteurs/deputes-en-exercice",
    .index_href_must = "liste_collaborateurs_excel.csv",
    .csv_delim = "semi", .charset = "windows-1252",
    .interval = 604800,
    .title_keys = "Nom du collaborateur",
    .id_keys = "Identifiant du député+Nom du collaborateur+Prénom du collaborateur",
    .description = "Every parliamentary assistant employed by a sitting French "
      "deputy, by name, with the deputy they work for and that deputy's own "
      "identifier — from the Assemblée nationale's own open data. Who staffs a "
      "legislator's office is a public fact that is rarely published anywhere "
      "else" },

  { .id = "NO_STORTINGET_INTERPELLATIONS",
    .name = "Norway Storting — interpellations to ministers",
    .name_ja = "ノルウェー議会 大臣への質問(緊急質問)", .category = "government",
    .portal = "https://data.stortinget.no", .record_type = "no-parliamentary-record",
    .tags = "\"no\",\"parliament\"", .free_tier = 1,
    /* One of the three live exports the tree was missing (2026-10-21): 5
     * interpellations, each naming the minister who answered and the minister
     * it was put to — which are not always the same person, and the
     * difference is the point. */
    .url = "https://data.stortinget.no/eksport/interpellasjoner?format=json",
    .interval = 86400,
    .array_path = "sporsmal_liste",
    /* `tittel` EXISTS on these records and is null on all five of them (the
     * session's interpellations are still at status 1, not yet titled), so a
     * row declaring it alone emitted a shape notice saying 0 of 5 records
     * carried the declared title. The minister it was put to is always
     * present, so it is the fallback — and `tittel` stays first for the
     * records that will have one. */
    .title_keys = "tittel,sporsmal_til_minister_tittel", .id_keys = "id",
    .description = "Interpellations put to Norwegian ministers — the full "
      "question text, who asked it, which minister answered and on whose "
      "behalf, with the date answered" },

  { .id = "NO_STORTINGET_QUESTION_TIME",
    .name = "Norway Storting — question-time questions",
    .name_ja = "ノルウェー議会 質問時間", .category = "government",
    .portal = "https://data.stortinget.no", .record_type = "no-parliamentary-record",
    .tags = "\"no\",\"parliament\"", .free_tier = 1,
    /* The third missing export: 60 question-time questions, same shape as the
     * interpellations above and the same reason for registering it. */
    .url = "https://data.stortinget.no/eksport/sporretimesporsmal?format=json",
    .interval = 86400,
    .array_path = "sporsmal_liste", .title_keys = "tittel", .id_keys = "id",
    .description = "Questions put to ministers in the Norwegian parliament's "
      "question time — the question as asked, the member who asked it, the "
      "minister it was addressed to and the one who answered" },

  { .id = "NO_DOFFIN_TENDERS", .name = "Norway Doffin — public procurement database",
    .name_ja = "ノルウェー 公共調達", .category = "government",
    .portal = "https://www.doffin.no", .record_type = "no-tender",
    .tags = "\"no\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.doffin.no/search?searchString={q}",
    .base = "https://www.doffin.no", .filter_query = 1,
    .description = "Norway's national procurement notice database — the "
      "contracting authority, the CPV classification, the estimated value and "
      "the award notice naming the supplier and the contract sum" },

  { .id = "DK_FOLKETINGET_ODA", .name = "Denmark Folketinget — open data API",
    .name_ja = "デンマーク議会 オープンデータ", .category = "government",
    .portal = "https://oda.ft.dk", .record_type = "dk-parliamentary-record",
    .tags = "\"dk\",\"parliament\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.ft.dk/da/soegning?q={q}",
    .base = "https://www.ft.dk", .filter_query = 1,
    .description = "The Danish parliament's records — cases, documents, "
      "committee questions and the answers, together with the members' "
      "register of interests. Denmark exposes the whole legislative graph" },

  { .id = "DK_UDBUD_TENDERS", .name = "Denmark udbud.dk — public tender notices",
    .name_ja = "デンマーク 公共調達", .category = "government",
    .portal = "https://www.udbud.dk", .record_type = "dk-tender",
    .tags = "\"dk\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.udbud.dk/Pages/Tenders/Search?searchTerm={q}",
    .base = "https://www.udbud.dk", .filter_query = 1,
    .description = "Danish state, regional and municipal tender notices and "
      "awards, including the below-threshold notices Denmark publishes "
      "voluntarily and which therefore cover most local government buying" },

  { .id = "FI_EDUSKUNTA_RECORDS", .name = "Finland Eduskunta — parliamentary documents",
    .name_ja = "フィンランド議会 文書", .category = "government",
    .portal = "https://www.eduskunta.fi", .record_type = "fi-parliamentary-record",
    .tags = "\"fi\",\"parliament\"", .mode = HP_JSON, .free_tier = 1,
    /* RE-POINTED 2026-10-22 (open item B2-bis). The scraped SharePoint search
     * path is gone; Eduskunta publishes a documented open-data API instead,
     * and the row stays an entity pivot because that API FILTERS — measured
     * both ways, which is the only reading that settles it (rule 4d):
     *     columnName=lastname&columnValue=Marin              1 row
     *     columnName=lastname&columnValue=zzqqxxjjwwvv7731   0 rows
     *     no filter at all                                 100 rows, hasMore
     *
     * It needed an ENGINE AFFORDANCE, not just a path. The API answers a JSON
     * TABLE — `rowData` is an array of ARRAYS and the column names are in a
     * sibling `columnNames` — so without `row_header_path` the engine would
     * name the cells by POSITION and this row's keys would be positions whose
     * meaning the API can change without anything here noticing. That is the
     * same hazard the ECB workbook taught, where two sheets of one file have
     * different geometry. `row_header_path` was added for this row and for the
     * Austrian one below; hptest 49-49f pin it, including that a row
     * declaring no header path still behaves exactly as before.
     *
     * MemberOfParliament is the table a name can be looked up in: 19 tables
     * exist and the others key on ids and technical keys. `page` is a page
     * NUMBER and 0-based, so page_zero_based is set and page_size is NOT (that
     * would make it an offset — see lint-sources page-size-on-page-number). */
    .url = "https://avoindata.eduskunta.fi/api/v1/tables/MemberOfParliament/"
      "rows?perPage=100&page=0&columnName=lastname&columnValue={q}",
    .array_path = "rowData", .row_header_path = "columnNames",
    .title_keys = "lastname", .id_keys = "personId",
    .page_param = "page", .page_zero_based = 1,
    .description = "Every member of the Finnish parliament the surname matches "
      "— the person id the rest of Eduskunta's open data keys on, their party, "
      "and whether they hold a ministerial post. Ask it for a surname" },

  /* ── Iberia and Italy ─────────────────────────────────────────────────── */

  { .id = "PT_DIARIO_REPUBLICA", .name = "Portugal — Diário da República",
    .name_ja = "ポルトガル 官報", .category = "government",
    .portal = "https://diariodarepublica.pt", .record_type = "pt-gazette-notice",
    .tags = "\"pt\",\"gazette\",\"law\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://diariodarepublica.pt/dr/pesquisa?q={q}",
    .base = "https://diariodarepublica.pt", .filter_query = 1,
    .description = "The Portuguese official gazette — laws, concession "
      "grants, expropriation declarations, public appointments and the "
      "second-series notices where regulators publish sanctions against named "
      "firms and professionals" },

  { .id = "IT_ANAC_CONTRACTS", .name = "Italy ANAC — anti-corruption authority contract data",
    .name_ja = "イタリア 汚職防止庁 契約データ", .category = "government",
    .portal = "https://dati.anticorruzione.it", .record_type = "it-contract",
    .tags = "\"it\",\"procurement\",\"anti-corruption\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://dati.anticorruzione.it/opendata/search?q={q}",
    .base = "https://dati.anticorruzione.it", .filter_query = 1,
    .description = "Italy's national anti-corruption authority publishes every "
      "public contract with its CIG identifier, the awarding body, the "
      "participants, the winner and the amounts paid — plus the "
      "white-list and interdiction records for mafia infiltration" },

  { .id = "PL_SEJM_API", .name = "Poland Sejm — parliamentary API",
    .name_ja = "ポーランド下院 API", .category = "government",
    .portal = "https://api.sejm.gov.pl", .record_type = "pl-parliamentary-record",
    .tags = "\"pl\",\"parliament\"", .free_tier = 1,
    .url = "https://api.sejm.gov.pl/sejm/term10/interpellations?limit=500",
    .filter_query = 1, .title_keys = "title,from",
    .id_keys = "num", .date_keys = "receiptDate",
    .page_param = "offset", .page_size = 500, .page_start = 0, .page_max = 30,
    .interval = 21600,
    .description = "The Sejm's open API — interpellations and the ministries' "
      "written answers, MP records, votes and committee sittings. Polish "
      "ministries answer interpellations with contract and inspection detail "
      "they publish nowhere else" },

  { .id = "CZ_NEN_PROCUREMENT", .name = "Czech Republic NEN — electronic procurement",
    .name_ja = "チェコ 電子調達", .category = "government",
    .portal = "https://nen.nipez.cz", .record_type = "cz-tender",
    .tags = "\"cz\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://nen.nipez.cz/verejne-zakazky?search={q}",
    .base = "https://nen.nipez.cz", .filter_query = 1,
    .description = "The Czech national electronic procurement tool — the "
      "contracting authority, the participants, the award and the contract "
      "text, which Czech law requires to be published in the contracts register "
      "or the contract is void" },
};

HP_REGISTER_TABLE(HP3B31_EUNAT)
