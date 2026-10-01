/* Verified-live int_environment sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "_vjson_idkeys.inc"

/* gcf-projects, fetched with a timeout that fits the upstream. The VJSON macro
 * hard-codes 25 s, and api.gcfund.org/v1/projects answers its 2.7 MB list in
 * 27.6 s (24.9 s to the first byte, live 2026-09-15), so every run failed at
 * 75 s (three attempts). Same call as the macro — jsonlist_emit_paged on the
 * bare array — with 120 s, and keyed on the fund's own ProjectsID (the records
 * carry no generic id field). */
static int run_gcf_projects(const source_ctx *c, intel_sink *s) {
  static const char *const ID = "gcf-projects";
  vidk_ctx kc = { s, ID, "ProjectsID" };
  intel_sink ks = { &kc, vidk_emit };
  int n = jsonlist_emit_paged(&ks, ID, c->http, "https://api.gcfund.org/v1/projects",
                              120000, "", "environment", "en",
                              "[\"int\",\"environment\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]");
  if (n < 0) { fprintf(stderr, "[%s] fetch failed\n", ID); return -1; }
  return 0;
}
static const source_def gcf_projects = {
  .id = "gcf-projects", .collector = "int_environment",
  .name = "Green Climate Fund — approved projects",
  .name_ja = "Green Climate Fund — approved projects",
  .update_interval_sec = 10800, .run = run_gcf_projects,
  .category = "environment", .type = "api",
  .url = "https://api.gcfund.org/v1/projects",
  .description = "All 372 GCF projects: ProjectName, approved reference (FP001), board meeting, theme, sector, lifetime CO2, risk category, direct and indirect beneficiaries, total GCF funding, co-financing, total value, project URL, plus nested Countries, accredited Entities, Disbursements, Funding tranches and ResultAreas. Detail hop verified with ProjectsID 13020.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(gcf_projects)

