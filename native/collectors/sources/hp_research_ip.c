/* collectors/sources/hp_research_ip.c — research, grants and IP-ownership depth.
 *
 * Existing coverage searched papers. Papers are the shallow end: the deep
 * records are the institution and funder graphs behind them, an ORCID's declared
 * employment history, the grant awarded to a named PI or organisation, the
 * clinical trial's sponsor, and — most under-used in corporate OSINT — the
 * patent and trademark ASSIGNMENT ledgers, which are a public record of who
 * bought what intellectual property from whom, and when. */
#include "../../lib/hpengine.h"

#define OA_UA { "User-Agent: JapanOSINT (research; contact@japanosint.local)", NULL }

static const hp_source HP_RESEARCH[] = {
  /* ── OpenAlex: the entity graph, not the paper list ────────────────────── */
  { .id = "OPENALEX_INSTITUTIONS", .name = "OpenAlex — institutions",
    .name_ja = "OpenAlex — 研究機関", .category = "research",
    .portal = "https://openalex.org", .record_type = "research-institution",
    .tags = "\"research\",\"institutions\"", .free_tier = 1, .headers = OA_UA,
    .url = "https://api.openalex.org/institutions?search={q}&per_page=25",
    .array_path = "results", .title_keys = "display_name", .id_keys = "id",
    .lat_key = "geo.latitude", .lon_key = "geo.longitude",
    .page_param = "page", .page_start = 1,
    .description = "Research institutions matching a name — ROR id, country, type, "
      "geo coordinates, associated institutions (parent/child) and works/citation "
      "counts. The organisational spine of the research graph" },

  { .id = "OPENALEX_FUNDERS", .name = "OpenAlex — research funders",
    .name_ja = "OpenAlex — 研究資金提供者", .category = "research",
    .portal = "https://openalex.org", .record_type = "research-funder",
    .tags = "\"research\",\"funding\"", .free_tier = 1, .headers = OA_UA,
    .url = "https://api.openalex.org/funders?search={q}&per_page=25",
    .array_path = "results", .title_keys = "display_name", .id_keys = "id",
    .page_param = "page", .page_start = 1,
    .description = "Funding bodies matching a name — alternate names, country, "
      "Crossref/ROR identifiers, grant and works counts. Who pays for the research "
      "an entity is associated with" },

  { .id = "OPENALEX_SOURCES", .name = "OpenAlex — journals & repositories",
    .name_ja = "OpenAlex — 掲載媒体", .category = "research",
    .portal = "https://openalex.org", .record_type = "research-source",
    .tags = "\"research\",\"publishers\"", .free_tier = 1, .headers = OA_UA,
    .url = "https://api.openalex.org/sources?search={q}&per_page=25",
    .array_path = "results", .title_keys = "display_name", .id_keys = "id",
    .page_param = "page", .page_start = 1,
    .description = "Journals, conference series and repositories — ISSNs, host "
      "organisation, APC prices, open-access status and whether the venue is in "
      "DOAJ; the check that separates a real venue from a predatory one" },

  { .id = "OPENALEX_AFFILIATION_WORKS", .name = "OpenAlex — works by raw affiliation",
    .name_ja = "OpenAlex — 所属機関名で論文検索", .category = "research",
    .portal = "https://openalex.org", .record_type = "research-work",
    .tags = "\"research\",\"affiliation\"", .free_tier = 1, .headers = OA_UA,
    .url = "https://api.openalex.org/works?filter=raw_affiliation_strings.search:{q}"
           "&per_page=40&sort=publication_date:desc",
    .array_path = "results", .title_keys = "display_name,title", .id_keys = "id",
    .date_keys = "publication_date",
    .page_param = "page", .page_start = 1,
    .description = "Papers whose author affiliation string mentions an organisation — "
      "catches company labs, subsidiaries and unregistered institutes that have no "
      "canonical institution id, which affiliation-id search misses entirely" },

  /* ── Crossref: publisher & funder graph ────────────────────────────────── */
  { .id = "CROSSREF_FUNDER_GRAPH", .name = "Crossref — funder registry",
    .name_ja = "Crossref — 資金提供者レジストリ", .category = "research",
    .portal = "https://www.crossref.org", .record_type = "research-funder",
    .tags = "\"research\",\"funding\"", .free_tier = 1, .headers = OA_UA,
    .url = "https://api.crossref.org/funders?query={q}&rows=25",
    .array_path = "message.items", .title_keys = "name", .id_keys = "id",
    .page_param = "offset", .page_size = 25,
    .description = "The Crossref Funder Registry — canonical funder ids, alternative "
      "names, country and hierarchy (which body sits under which ministry), used to "
      "normalise the funder field on every paper" },

  { .id = "CROSSREF_MEMBERS", .name = "Crossref — publisher members",
    .name_ja = "Crossref — 出版社メンバー", .category = "research",
    .portal = "https://www.crossref.org", .record_type = "research-publisher",
    .tags = "\"research\",\"publishers\"", .free_tier = 1, .headers = OA_UA,
    .url = "https://api.crossref.org/members?query={q}&rows=25",
    .array_path = "message.items", .title_keys = "primary-name", .id_keys = "id",
    .page_param = "offset", .page_size = 25,
    .description = "Crossref member publishers — prefixes owned, titles published, "
      "deposit counts and licensing/coverage metadata; the way to attribute a DOI "
      "prefix to a corporate publisher" },

  /* ── ORCID: person-level depth ─────────────────────────────────────────── */
  { .id = "ORCID_EXPANDED_SEARCH", .name = "ORCID — researcher search",
    .name_ja = "ORCID — 研究者検索", .category = "research",
    .portal = "https://orcid.org", .record_type = "researcher",
    .tags = "\"research\",\"people\"", .free_tier = 1,
    .url = "https://pub.orcid.org/v3.0/expanded-search/?q={q}&rows=25",
    .headers = { "Accept: application/json", NULL },
    .array_path = "expanded-result", .title_keys = "credit-name,given-names,family-names",
    .id_keys = "orcid-id",
    .link_tmpl = "https://orcid.org/{v}", .link_keys = "orcid-id",
    .description = "Researchers by name — ORCID iD, credit name, current institution "
      "affiliations and email where public. The iD is the join key for the two "
      "deeper rows below" },

  { .id = "ORCID_EMPLOYMENTS", .name = "ORCID — declared employment history",
    .name_ja = "ORCID — 職歴", .category = "research",
    .portal = "https://orcid.org", .record_type = "employment-record",
    .tags = "\"research\",\"people\",\"employment\"", .free_tier = 1,
    .url = "https://pub.orcid.org/v3.0/{qn}/employments",
    .headers = { "Accept: application/json", NULL },
    .title_keys = "affiliation-group.0.summaries.0.employment-summary.organization.name",
    .description = "The employment history a researcher has published on their own "
      "ORCID record — organisation, department, role title and start/end dates. "
      "Self-declared, verifiable, and one of the few free CV-grade sources" },

  { .id = "ORCID_WORKS", .name = "ORCID — works claimed by a researcher",
    .name_ja = "ORCID — 業績一覧", .category = "research",
    .portal = "https://orcid.org", .record_type = "research-work",
    .tags = "\"research\",\"people\"", .free_tier = 1,
    .url = "https://pub.orcid.org/v3.0/{qn}/works",
    .headers = { "Accept: application/json", NULL },
    .array_path = "group",
    .title_keys = "work-summary.0.title.title.value",
    .date_keys = "work-summary.0.publication-date.year.value",
    .description = "Every work claimed under an ORCID iD — titles, types, external "
      "identifiers and years, including preprints and datasets that citation "
      "databases index inconsistently" },

  { .id = "ROR_AFFILIATION_MATCH", .name = "ROR — affiliation string matching",
    .name_ja = "ROR — 所属文字列マッチ", .category = "research",
    .portal = "https://ror.org", .record_type = "research-institution",
    .tags = "\"research\",\"institutions\"", .free_tier = 1,
    .url = "https://api.ror.org/organizations?affiliation={q}",
    .array_path = "items", .title_keys = "organization.name,name",
    .id_keys = "organization.id",
    .description = "Resolves a messy affiliation string to canonical ROR "
      "organisations with match confidence and type — the standard way to turn "
      "'Dept of X, Y Univ, Tokyo' into a stable institution id" },

  { .id = "S2_AUTHOR_SEARCH", .name = "Semantic Scholar — author profiles",
    .name_ja = "Semantic Scholar — 著者プロファイル", .category = "research",
    .portal = "https://www.semanticscholar.org", .record_type = "researcher",
    .tags = "\"research\",\"people\"", .free_tier = 1,
    .url = "https://api.semanticscholar.org/graph/v1/author/search?query={q}"
           "&fields=name,affiliations,homepage,paperCount,citationCount,hIndex&limit=25",
    .array_path = "data", .title_keys = "name", .id_keys = "authorId",
    .description = "Author-level metrics and affiliations — paper count, citation "
      "count, h-index and homepage. Useful for confirming whether a claimed academic "
      "record exists at the scale claimed" },

  { .id = "EUROPEPMC_SEARCH", .name = "Europe PMC — life-science literature",
    .name_ja = "Europe PMC — 生命科学文献", .category = "research",
    .portal = "https://europepmc.org", .record_type = "research-work",
    .tags = "\"research\",\"biomed\"", .free_tier = 1,
    .url = "https://www.ebi.ac.uk/europepmc/webservices/rest/search?query={q}"
           "&format=json&pageSize=40&resultType=core",
    .array_path = "resultList.result", .title_keys = "title", .id_keys = "id",
    .date_keys = "firstPublicationDate",
    .page_param = "page", .page_start = 1,
    .description = "Europe PMC records including preprints, patents citing papers, "
      "grant acknowledgements and author affiliations — the grant IDs in the core "
      "result set link a paper to a specific funded project" },

  /* ── Grants ────────────────────────────────────────────────────────────── */
  { .id = "OPENAIRE_PROJECTS", .name = "OpenAIRE — EU-funded projects",
    .name_ja = "OpenAIRE — EU助成プロジェクト", .category = "research",
    .portal = "https://explore.openaire.eu", .record_type = "research-grant",
    .tags = "\"research\",\"funding\",\"eu\"", .free_tier = 1,
    .url = "https://api.openaire.eu/search/projects?name={q}&format=json&size=40",
    .title_keys = "title,acronym", .id_keys = "code",
    .description = "EU and national research projects matching an organisation or "
      "project name — funder, programme, call, grant code, budget and participant "
      "list; the money layer under a research collaboration" },

  { .id = "NIH_REPORTER_PI", .name = "NIH RePORTER — awards by principal investigator",
    .name_ja = "NIH RePORTER — 研究代表者別助成", .category = "research",
    .portal = "https://reporter.nih.gov", .record_type = "research-grant",
    .tags = "\"research\",\"funding\",\"us\"", .free_tier = 1,
    .url = "https://api.reporter.nih.gov/v2/projects/search",
    .post_body = "{\"criteria\":{\"pi_names\":[{\"any_name\":\"{Q}\"}]},"
                 "\"include_fields\":[\"ProjectNum\",\"ProjectTitle\",\"Organization\","
                 "\"AwardAmount\",\"FiscalYear\",\"PrincipalInvestigators\","
                 "\"AgencyIcAdmin\",\"ProjectStartDate\"],\"offset\":0,\"limit\":40}",
    .array_path = "results", .title_keys = "project_title", .id_keys = "project_num",
    .date_keys = "project_start_date",
    .description = "NIH awards to a named principal investigator — project number, "
      "title, awarding institute, organisation and dollar amount by fiscal year. A "
      "PI's funding history is a durable identity and employment trail" },

  { .id = "NSF_AWARDEE", .name = "NSF — awards by awardee organisation",
    .name_ja = "NSF — 受給機関別助成", .category = "research",
    .portal = "https://www.nsf.gov/awardsearch/", .record_type = "research-grant",
    .tags = "\"research\",\"funding\",\"us\"", .free_tier = 1,
    .url = "https://api.nsf.gov/services/v1/awards.json?awardeeName={q}"
           "&printFields=id,title,awardeeName,awardeeCity,piFirstName,piLastName,"
           "startDate,expDate,estimatedTotalAmt,fundProgramName&rzp=40",
    .array_path = "response.award", .title_keys = "title", .id_keys = "id",
    .date_keys = "startDate",
    .description = "US National Science Foundation awards to an organisation — award "
      "id, title, PI names, programme, dates and estimated total amount" },

  { .id = "UKRI_GTR_ORGS", .name = "UKRI Gateway to Research — organisations & projects",
    .name_ja = "英国UKRI 研究助成データ", .category = "research",
    .portal = "https://gtr.ukri.org", .record_type = "research-grant",
    .tags = "\"research\",\"funding\",\"uk\"", .free_tier = 1,
    .url = "https://gtr.ukri.org/api/organisations?q={q}&p=1&s=25",
    .headers = { "Accept: application/json", NULL },
    .array_path = "organisation", .title_keys = "name", .id_keys = "id",
    .page_param = "p", .page_start = 1,
    .description = "UK Research and Innovation funding data — organisations, the "
      "projects they lead or participate in, funders and award values across all UK "
      "research councils" },

  { .id = "CLINICALTRIALS_SPONSOR", .name = "ClinicalTrials.gov — trials by sponsor",
    .name_ja = "臨床試験 — スポンサー別", .category = "health",
    .portal = "https://clinicaltrials.gov", .record_type = "clinical-trial",
    .tags = "\"health\",\"trials\"", .free_tier = 1,
    .url = "https://clinicaltrials.gov/api/v2/studies?query.spons={q}&pageSize=40",
    .array_path = "studies",
    .title_keys = "protocolSection.identificationModule.briefTitle",
    .id_keys = "protocolSection.identificationModule.nctId",
    .date_keys = "protocolSection.statusModule.startDateStruct.date",
    .description = "Clinical trials sponsored or collaborated on by an organisation — "
      "NCT id, phase, status, enrolment, sites and collaborators. Trial registries "
      "expose corporate relationships years before results publish" },

  /* ── Patents & trademarks: the ownership ledger ────────────────────────── */
  { .id = "PATENTSVIEW_PATENTS", .name = "PatentsView — US patents by assignee",
    .name_ja = "PatentsView — 出願人別米国特許", .category = "research",
    .portal = "https://search.patentsview.org", .record_type = "patent",
    .tags = "\"patents\",\"us\"", .key_env = "PATENTSVIEW_API_KEY", .free_tier = 1,
    .url = "https://search.patentsview.org/api/v1/patent/?q=%7B%22_text_any%22"
           "%3A%7B%22patent_title%22%3A%22{q}%22%7D%7D&f=%5B%22patent_id%22%2C"
           "%22patent_title%22%2C%22patent_date%22%2C%22assignees.assignee_organization%22%5D&o=%7B%22size%22%3A40%7D",
    .headers = { "X-Api-Key: {key}", NULL },
    .array_path = "patents", .title_keys = "patent_title", .id_keys = "patent_id",
    .date_keys = "patent_date",
    .description = "US granted patents matching a term, with assignee organisations — "
      "the technology footprint of a company and the co-assignees that reveal joint "
      "ventures" },

  { .id = "PATENTSVIEW_ASSIGNEES", .name = "PatentsView — assignee entities",
    .name_ja = "PatentsView — 権利者エンティティ", .category = "research",
    .portal = "https://search.patentsview.org", .record_type = "patent-assignee",
    .tags = "\"patents\",\"us\",\"ownership\"", .key_env = "PATENTSVIEW_API_KEY",
    .free_tier = 1,
    .url = "https://search.patentsview.org/api/v1/assignee/?q=%7B%22_contains%22"
           "%3A%7B%22assignee_organization%22%3A%22{q}%22%7D%7D&o=%7B%22size%22%3A40%7D",
    .headers = { "X-Api-Key: {key}", NULL },
    .array_path = "assignees", .title_keys = "assignee_organization",
    .id_keys = "assignee_id",
    .description = "Patent assignee entities matching a company name — the "
      "disambiguated organisation records with patent counts, locations and name "
      "variants used across USPTO filings" },

  { .id = "USPTO_PATENT_ASSIGNMENTS", .name = "USPTO — patent assignment ledger",
    .name_ja = "USPTO — 特許権譲渡記録", .category = "economy",
    .portal = "https://assignment.uspto.gov", .record_type = "patent-assignment",
    .tags = "\"patents\",\"us\",\"ownership\"", .free_tier = 1,
    .url = "https://assignment-api.uspto.gov/patent/lookup?query={q}"
           "&filter_assignorName=&rows=40",
    .array_path = "response.docs", .title_keys = "inventionTitle,patAssigneeName",
    .id_keys = "id", .date_keys = "recordedDate",
    .description = "Recorded transfers of US patent ownership — assignor, assignee, "
      "correspondent, execution and recording dates. This ledger shows asset moves "
      "between related companies that no corporate registry reports" },

  { .id = "USPTO_TRADEMARK_ASSIGNMENTS", .name = "USPTO — trademark assignment ledger",
    .name_ja = "USPTO — 商標譲渡記録", .category = "economy",
    .portal = "https://assignment.uspto.gov", .record_type = "trademark-assignment",
    .tags = "\"trademarks\",\"us\",\"ownership\"", .free_tier = 1,
    .url = "https://assignment-api.uspto.gov/trademark/lookup?query={q}&rows=40",
    .array_path = "response.docs", .title_keys = "markDescription,tmAssigneeName",
    .id_keys = "id", .date_keys = "recordedDate",
    .description = "Recorded transfers and security interests in US trademarks — who "
      "assigned which mark to whom, and which marks were pledged as collateral, with "
      "the correspondent address of the filing attorney" },

  { .id = "GOOGLE_PATENTS_QUERY", .name = "Google Patents — global patent search",
    .name_ja = "Google Patents — 世界特許検索", .category = "research",
    .portal = "https://patents.google.com", .record_type = "patent",
    .tags = "\"patents\",\"global\"", .free_tier = 1,
    .url = "https://patents.google.com/xhr/query?url=q%3D{q}&exp=",
    .array_path = "results.cluster.0.result", .title_keys = "patent.title",
    .id_keys = "patent.publication_number", .date_keys = "patent.publication_date",
    .description = "Worldwide patent families matching a term, across USPTO, EPO, "
      "WIPO, CNIPA and JPO collections — assignee, inventor, publication number and "
      "date, including non-English filings" },
};

HP_REGISTER_TABLE(HP_RESEARCH)
