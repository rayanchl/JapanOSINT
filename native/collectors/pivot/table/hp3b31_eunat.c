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

  { .id = "DE_BUNDESKARTELLAMT_DECISIONS", .name = "Germany Bundeskartellamt — antitrust & merger decisions",
    .name_ja = "ドイツ連邦カルテル庁 決定", .category = "government",
    .portal = "https://www.bundeskartellamt.de", .record_type = "de-antitrust-decision",
    .tags = "\"de\",\"antitrust\",\"enforcement\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.bundeskartellamt.de/SiteGlobals/Forms/Suche/"
      "Entscheidungssuche_Formular.html?input_={q}",
    .base = "https://www.bundeskartellamt.de", .filter_query = 1,
    .description = "The Federal Cartel Office's decision database — merger "
      "clearances and prohibitions naming both parties, cartel fines with the "
      "amount per undertaking, abuse-of-dominance rulings and the sector "
      "inquiries that map an entire market's ownership" },

  { .id = "DE_PRTR_FACILITIES", .name = "Germany PRTR — industrial pollutant release register",
    .name_ja = "ドイツ 汚染物質排出登録", .category = "environment",
    .portal = "https://www.thru.de", .record_type = "de-industrial-facility",
    .tags = "\"de\",\"environment\",\"industry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.thru.de/thrude/suche/?tx_solr[q]={q}",
    .base = "https://www.thru.de", .filter_query = 1,
    .description = "Every German industrial installation above the reporting "
      "threshold — the operator, the site coordinates, the activity class and "
      "the annual quantity of each pollutant released to air, water and soil" },

  { .id = "AT_PARLAMENT_RECORDS", .name = "Austria Parlament — parliamentary materials",
    .name_ja = "オーストリア議会 資料検索", .category = "government",
    .portal = "https://www.parlament.gv.at", .record_type = "at-parliamentary-record",
    .tags = "\"at\",\"parliament\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.parlament.gv.at/recherchieren/suche?searchTerm={q}",
    .base = "https://www.parlament.gv.at", .filter_query = 1,
    .description = "Austrian bills, ministerial answers, committee reports and "
      "the members' declarations of outside income and functions, which "
      "Austria publishes at a finer grain than most of the EU" },

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

  { .id = "FR_ASSEMBLEE_NATIONALE", .name = "France Assemblée nationale — parliamentary record",
    .name_ja = "フランス国民議会 議事録", .category = "government",
    .portal = "https://www.assemblee-nationale.fr", .record_type = "fr-parliamentary-record",
    .tags = "\"fr\",\"parliament\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.assemblee-nationale.fr/dyn/recherche?texte={q}",
    .base = "https://www.assemblee-nationale.fr", .filter_query = 1,
    .description = "Bills, amendments with their authors, written questions and "
      "ministerial answers, committee hearings and the audition of named "
      "company executives — which is a public, on-record statement by them" },

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

  { .id = "NL_TWEEDE_KAMER_OPENDATA", .name = "Netherlands Tweede Kamer — parliament open data",
    .name_ja = "オランダ下院 オープンデータ", .category = "government",
    .portal = "https://opendata.tweedekamer.nl", .record_type = "nl-parliamentary-record",
    .tags = "\"nl\",\"parliament\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://opendata.tweedekamer.nl/zoeken?q={q}",
    .base = "https://opendata.tweedekamer.nl", .filter_query = 1,
    .description = "The Dutch lower house's open data service — documents, "
      "motions, votes per member, agenda items and the lobbyist access passes "
      "issued, all as structured records" },

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
    .tags = "\"no\",\"parliament\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.stortinget.no/no/Saker-og-publikasjoner/Sok/?stq={q}",
    .base = "https://www.stortinget.no", .filter_query = 1,
    .description = "Norwegian parliamentary cases, votes per representative, "
      "committee hearings and the register of representatives' financial "
      "interests including shareholdings and paid travel" },

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
    .tags = "\"fi\",\"parliament\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.eduskunta.fi/FI/search/Sivut/results.aspx?k={q}",
    .base = "https://www.eduskunta.fi", .filter_query = 1,
    .description = "Finnish bills, written questions and committee statements, "
      "plus the members' declarations of interests and the register of "
      "election-campaign funding by donor" },

  /* ── Iberia and Italy ─────────────────────────────────────────────────── */
  { .id = "ES_PORTAL_TRANSPARENCIA", .name = "Spain — transparency portal & subsidy database",
    .name_ja = "スペイン 透明性ポータル", .category = "government",
    .portal = "https://transparencia.gob.es", .record_type = "es-transparency",
    .tags = "\"es\",\"transparency\",\"subsidy\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://transparencia.gob.es/es_ES/buscar?texto={q}",
    .base = "https://transparencia.gob.es", .filter_query = 1,
    .description = "Spain's transparency portal — senior officials' agendas "
      "and declared assets, the answered access-to-information requests, and "
      "the route into the BDNS national subsidy database that names every "
      "grant beneficiary with its NIF and the amount received" },

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

  { .id = "IT_OPENCOESIONE_PROJECTS", .name = "Italy OpenCoesione — cohesion project spending",
    .name_ja = "イタリア 結束基金事業", .category = "government",
    .portal = "https://opencoesione.gov.it", .record_type = "it-funded-project",
    .tags = "\"it\",\"funding\",\"beneficiary\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://opencoesione.gov.it/it/ricerca/?q={q}",
    .base = "https://opencoesione.gov.it", .filter_query = 1,
    .description = "Every EU cohesion-funded project in Italy at record level "
      "— the beneficiary with its fiscal code, the implementing body, the "
      "programme, the committed and actually paid amounts, the physical "
      "progress and the municipality. Italy's most granular public-spending "
      "dataset" },

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
