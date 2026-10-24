/* collectors/pivot/table/hp3b31_afrgov.c — batch 31: afrgov — Sub-Saharan African public record.
 *
 * The received wisdom is that African corporate and government records are not
 * online. That is out of date and, for the records that matter most in a
 * due-diligence context, wrong: South Africa runs a full OCDS procurement API,
 * Kenya, Nigeria, Ghana and Rwanda all publish company registry search, SAFLII
 * and AfricanLII carry decades of judgments across a dozen jurisdictions, and
 * the extractive-industry transparency regimes publish contract-level payment
 * data for exactly the sectors where the risk concentrates.
 *
 * What is genuinely scarce is *indexing*. These portals are rarely crawled and
 * almost never queried programmatically, which is precisely why wiring them is
 * worth more here than another European register.
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

static const hp_source HP3B31_AFRGOV[] = {
  /* ── South Africa ─────────────────────────────────────────────────────── */
  { .id = "ZA_CIPC_ENTERPRISE", .name = "South Africa CIPC — company & close corporation search",
    .name_ja = "南アフリカ 企業登記", .category = "government",
    .portal = "https://eservices.cipc.co.za", .record_type = "za-entity",
    .tags = "\"za\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://eservices.cipc.co.za/EnterpriseSearch/Search?enterpriseName={q}",
    .base = "https://eservices.cipc.co.za", .filter_query = 1,
    .description = "The Companies and Intellectual Property Commission's "
      "enterprise search — registration number, entity type, status, and the "
      "directors and members recorded against a South African company" },

  { .id = "ZA_GOVERNMENT_GAZETTE", .name = "South Africa — government gazette notices",
    .name_ja = "南アフリカ 官報", .category = "government",
    .portal = "https://www.gov.za", .record_type = "za-gazette-notice",
    .tags = "\"za\",\"gazette\",\"legal\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    /* RE-POINTED 2026-10-21 (open item B2). The old path 404s; gov.za's own
     * search form declares GET /search?search_query=. Measured both ways, and
     * the href_must is the whole point: with no filter the impossible entity
     * still returns 3 /documents/ anchors (/documents/acts, /documents/latest,
     * /documents/constitution/...), which would be emitted as gazette notices
     * for EVERY entity. Narrowed to the notices path:
     *   search_query=Toyota             3 /documents/notices/ anchors
     *   search_query=zzqqxxjjwwvv7731   0
     * A filter that works and chrome that does not are two different
     * problems, and only the second is fixed by href_must. */
    .url = "https://www.gov.za/search?search_query={q}",
    .base = "https://www.gov.za", .href_must = "/documents/notices/",
    /* filter_query is DROPPED here too, and this row had carried it since it
     * was written. gov.za filters server-side (measured above), so the local
     * pass only ever removed the upstream's own hits: a gazette notice is
     * titled for the INSTRUMENT, not the company — "Competition Act:
     * approved merger — Barloworld SA (Pty) Ltd and Leatoy (Pty) Ltd" is a
     * real match for an entity its slug never names. */
    .description = "National and provincial gazettes — liquidations, estate "
      "notices, licence grants, expropriations and the regulatory "
      "determinations that only ever appear in gazette form" },

  { .id = "ZA_MUNICIPAL_MONEY", .name = "South Africa Municipal Money — municipal finances",
    .name_ja = "南アフリカ 自治体財政", .category = "government",
    .portal = "https://municipalmoney.gov.za", .record_type = "za-municipal-finance",
    .tags = "\"za\",\"public-finance\",\"local-government\"", .free_tier = 1,
    /* RE-POINTED 2026-10-21 (open item B2). municipalmoney.gov.za/search 404s
     * on three readings nine days apart, but the National Treasury publishes
     * the data through a cubes API the site itself calls "Municipal Money
     * API". Measured that day, the filter asked BOTH ways (rule 4d):
     *   cut=demarcation.label:"Cape Town"           39 facts, total 39
     *   cut=demarcation.label:"Zzqqxxjjwwvv7731"     0 facts, total 0
     *   no cut at all                           10,000 of a declared 10,314
     * so the cut is honoured and this stays an entity pivot. The entity must
     * be the demarcation LABEL exactly as the API spells it — "Cape Town",
     * not "City of Cape Town", which returns 0 — which is why the name says
     * so. `page` is a page NUMBER, so page_size is deliberately NOT declared
     * (lint-sources: page-size-on-page-number).
     * This reads ONE of the 21 cubes the API publishes; audit outcomes, cash
     * coverage and creditor days are siblings nobody has registered yet, so
     * the description no longer claims them. */
    .url = "https://municipaldata.treasury.gov.za/api/cubes/uifwexp/facts"
      "?cut=demarcation.label:%22{q}%22&page=0",   /* exhaustive-ok: one page IS the whole answer for a cut — 39 of a declared 39 for Cape Town, and the entire uncut cube is 10,314 facts; declaring page_param made the engine re-read page 0 and file a repeated-page notice every run */
    .array_path = "data", .title_keys = "item.label",
    /* Each of these field names CONTAINS A DOT and is a literal key, not a
     * path — the shape hp_path's literal-key fallback exists for. One fact is
     * (municipality, item, year), so the identity is all three: keying on the
     * demarcation alone would collapse 39 facts onto one. */
    .id_keys = "demarcation.code+item.code+financial_year_end.year",
    /* NO paging, measured rather than assumed. With page_param declared the
     * engine asked page 1, got page 0's bytes back, and filed a shape notice
     * naming a repeated page on EVERY run — a healthy row crying wolf, which
     * is how people learn to ignore notices. One page is complete here: a cut
     * answers 39 of its own declared 39 for Cape Town, and the whole uncut
     * cube is 10,314 facts, so no single municipality's (item x year) history
     * comes near a page. */
    .description = "Unauthorised, irregular, fruitless and wasteful "
      "expenditure as audited, per municipality per financial year, from "
      "National Treasury's own figures. Ask it for the municipality's name as "
      "the Treasury spells it (\"Cape Town\"). Municipal financial distress is "
      "the leading indicator for service-delivery contracts" },

  { .id = "ZA_PMG_PARLIAMENT", .name = "South Africa PMG — parliamentary committee record",
    .name_ja = "南アフリカ議会 委員会記録", .category = "government",
    .portal = "https://pmg.org.za", .record_type = "za-parliamentary-record",
    .tags = "\"za\",\"parliament\",\"oversight\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://pmg.org.za/search/?q={q}",
    .base = "https://pmg.org.za", .filter_query = 1,
    .description = "The Parliamentary Monitoring Group's minutes of every "
      "committee meeting, questions to ministers, and the tabled reports — the "
      "most complete record of South African state-owned enterprise oversight "
      "that exists in one place" },

  { .id = "AFRICA_SAFLII_JUDGMENTS", .name = "SAFLII — Southern African legal information",
    .name_ja = "南部アフリカ 判例データベース", .category = "legal",
    .portal = "https://www.saflii.org", .record_type = "africa-judgment",
    .tags = "\"za\",\"africa\",\"courts\",\"judgment\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.saflii.org/cgi-bin/sinosrch.cgi?query={q}&method=auto",
    .base = "https://www.saflii.org", .filter_query = 1,
    .description = "Judgments from South Africa, Botswana, Lesotho, Malawi, "
      "Mauritius, Namibia, Eswatini, Tanzania, Zambia and Zimbabwe — parties, "
      "citation and the full text of the decision" },

  { .id = "AFRICA_OPEN_AFRICA_CKAN", .name = "open.africa — continental dataset catalog",
    .name_ja = "アフリカ オープンデータ目録", .category = "government",
    .portal = "https://open.africa", .record_type = "africa-dataset",
    .tags = "\"africa\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://open.africa/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Code for Africa's continental CKAN — budget, health "
      "facility, extractives and election datasets scraped or negotiated out of "
      "governments that publish nothing machine-readable themselves" },

  /* ── West Africa ──────────────────────────────────────────────────────── */
  { .id = "NG_CAC_COMPANY_SEARCH", .name = "Nigeria CAC — company registration search",
    .name_ja = "ナイジェリア 企業登記", .category = "government",
    .portal = "https://search.cac.gov.ng", .record_type = "ng-entity",
    .tags = "\"ng\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://search.cac.gov.ng/home?searchTerm={q}",
    .base = "https://search.cac.gov.ng", .filter_query = 1,
    .description = "Nigeria's Corporate Affairs Commission public search — "
      "RC number, company status, registered address and the business "
      "classification. Nigeria has also begun publishing persons with "
      "significant control against these records" },

  { .id = "NG_BPP_PROCUREMENT", .name = "Nigeria — Bureau of Public Procurement records",
    .name_ja = "ナイジェリア 公共調達庁", .category = "government",
    .portal = "https://www.bpp.gov.ng", .record_type = "ng-procurement",
    .tags = "\"ng\",\"procurement\",\"debarment\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.bpp.gov.ng/?s={q}",
    .base = "https://www.bpp.gov.ng", .filter_query = 1,
    .description = "Certificates of no objection, contract award approvals and "
      "the debarment list of contractors barred from Nigerian federal "
      "procurement, with the ground and the duration" },

  { .id = "NG_NEITI_EXTRACTIVES", .name = "Nigeria NEITI — extractive industry audits",
    .name_ja = "ナイジェリア 採掘産業透明性", .category = "government",
    .portal = "https://neiti.gov.ng", .record_type = "ng-extractive-payment",
    .tags = "\"ng\",\"extractives\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://neiti.gov.ng/?s={q}",
    .base = "https://neiti.gov.ng", .filter_query = 1,
    .description = "Nigeria Extractive Industries Transparency Initiative audit "
      "reports — company-by-company payments to government, production volumes "
      "by field or lease, and the reconciliation discrepancies that name the "
      "operator responsible" },

  { .id = "SN_MARCHES_PUBLICS", .name = "Senegal — public procurement portal",
    .name_ja = "セネガル 公共調達", .category = "government",
    .portal = "https://www.marchespublics.sn", .record_type = "sn-procurement",
    .tags = "\"sn\",\"procurement\",\"francophone\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.marchespublics.sn/index.php?option=com_search&searchword={q}",
    .base = "https://www.marchespublics.sn", .filter_query = 1,
    .description = "Senegal's ARMP procurement portal — tender notices, award "
      "decisions and the dispute rulings of the regulator, which name the "
      "excluded bidders and the reason" },

  /* ── East Africa ──────────────────────────────────────────────────────── */
  { .id = "KE_PPRA_DEBARMENT", .name = "Kenya PPRA — debarred suppliers & tender portal",
    .name_ja = "ケニア 調達規制庁", .category = "government",
    .portal = "https://ppra.go.ke", .record_type = "ke-debarment",
    .tags = "\"ke\",\"procurement\",\"debarment\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://ppra.go.ke/?s={q}",
    .base = "https://ppra.go.ke", .filter_query = 1,
    .description = "Kenya's Public Procurement Regulatory Authority — the "
      "debarment register naming suppliers barred from public contracts, the "
      "Review Board decisions and the tender portal notices" },

  { .id = "KE_OPEN_DATA", .name = "Kenya Open Data — national dataset portal",
    .name_ja = "ケニア オープンデータ", .category = "government",
    .portal = "https://www.opendata.go.ke", .record_type = "ke-dataset",
    .tags = "\"ke\",\"open-data\",\"catalog\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.opendata.go.ke/search?q={q}",
    .base = "https://www.opendata.go.ke", .filter_query = 1,
    .description = "Kenyan county and national datasets — budget allocations, "
      "health facility registers, school lists and the census tables used to "
      "verify claimed project locations" },

  { .id = "TZ_BRELA_ENTITY", .name = "Tanzania BRELA — business registration",
    .name_ja = "タンザニア 企業登記", .category = "government",
    .portal = "https://ors.brela.go.tz", .record_type = "tz-entity",
    .tags = "\"tz\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ors.brela.go.tz/orsAdmin/searchcompany?name={q}",
    .base = "https://ors.brela.go.tz", .filter_query = 1,
    .description = "Tanzania's Business Registrations and Licensing Agency — "
      "company registration number, status, directors and the industrial "
      "licence attached to a manufacturing entity" },

  { .id = "RW_RDB_REGISTRY", .name = "Rwanda RDB — company registration",
    .name_ja = "ルワンダ 企業登記", .category = "government",
    .portal = "https://org.rdb.rw", .record_type = "rw-entity",
    .tags = "\"rw\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://org.rdb.rw/business/search?q={q}",
    .base = "https://org.rdb.rw", .filter_query = 1,
    .description = "Rwanda Development Board's company register — TIN, "
      "registration date, shareholders and directors. Rwanda is one of the few "
      "African registries that publishes shareholding percentages openly" },

  { .id = "ET_ETHIOPIA_TRADE_REGISTRY", .name = "Ethiopia — trade registration & licensing",
    .name_ja = "エチオピア 商業登記", .category = "government",
    .portal = "https://etrade.gov.et", .record_type = "et-entity",
    .tags = "\"et\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://etrade.gov.et/business-license-checker?businessName={q}",
    .base = "https://etrade.gov.et", .filter_query = 1,
    .description = "Ethiopia's trade registration and business licence checker "
      "— the licence number, the permitted business activity, the capital "
      "registered and the licence renewal status" },

  /* ── Southern Africa & regional bodies ────────────────────────────────── */
  { .id = "BW_CIPA_REGISTRY", .name = "Botswana CIPA — companies & IP authority",
    .name_ja = "ボツワナ 企業登記", .category = "government",
    .portal = "https://www.cipa.co.bw", .record_type = "bw-entity",
    .tags = "\"bw\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.cipa.co.bw/?s={q}",
    .base = "https://www.cipa.co.bw", .filter_query = 1,
    .description = "Botswana's Companies and Intellectual Property Authority — "
      "registration status, directors and the annual return record, in a "
      "jurisdiction used as a regional holding domicile" },

  { .id = "AFRICA_RESOURCE_PROJECTS", .name = "Resource Projects — extractive payments by project",
    .name_ja = "資源事業 支払開示", .category = "government",
    .portal = "https://resourceprojects.org", .record_type = "extractive-payment",
    .tags = "\"africa\",\"extractives\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://resourceprojects.org/search?q={q}",
    .base = "https://resourceprojects.org", .filter_query = 1,
    .description = "Payments to governments disclosed under the EU Accounting "
      "Directive, UK, Canadian and Norwegian rules, broken down by project and "
      "payment type — taxes, royalties, bonuses and infrastructure "
      "improvements. Company-to-state money at asset level" },
};

HP_REGISTER_TABLE(HP3B31_AFRGOV)
