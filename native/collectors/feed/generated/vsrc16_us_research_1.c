/* Verified-live us_research sources (13), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(nsf_awards_research_gov, "nsf-awards-research-gov", "NSF award search on research.gov (alternate live host)", "NSF award search on research.gov (alternate live host)",
  "us_research", "research",
  "https://www.research.gov/awardapi-service/v1/awards.json?keyword=graphene&rpp=10",
  "response.award",
  "en", "[\"us\",\"research\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Same NSF award data as api.nsf.gov but served from research.gov: full abstractText, award id, title, dates, awardee organisation and PI. Worth carrying as a second host because api.nsf.gov has had outages; both returned identical 55KB payloads in this probe.");

VJSON(openalex_work_by_doi, "openalex-work-by-doi", "OpenAlex work by DOI (id pivot)", "OpenAlex work by DOI (id pivot)",
  "us_research", "research",
  "https://api.openalex.org/works/doi:10.1038/nature12373",
  ".", /* the work IS the record. Path mesh emitted 42 MeSH descriptor/qualifier rows keyed on descriptor_ui (42 emitted, 14 stored, 2026-09-14) and discarded title, authorships, institutions and referenced_works; "." keeps the whole work, mesh[] included, on one row */
  "en", "[\"us\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Same full work record but keyed by DOI instead of OpenAlex id - lets you enter the OpenAlex graph from any Crossref/DataCite DOI you already hold and come back out with authors, institutions, funders and the referenced_works edge list.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

