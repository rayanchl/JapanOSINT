/* cert_first_teams.c — FIRST.org CSIRT team directory.
 *
 * Endpoint: https://api.first.org/data/v1/teams?limit=100&offset=N  (keyless)
 * Shape:    { status, data: [ … ], total: 874, … }; paginate via limit/offset.
 *           This collector walks up to FIRST_MAX_PAGES pages and stops as soon
 *           as a page comes back empty or `total` is reached.
 *
 * Emits, every value read out of the response: team name, ISO country code,
 * host organisation, team type, contact e-mail, establishment date, PGP
 * fingerprint, website and constituency, keyed on the FIRST team id.
 *
 * R2 — deliberately NO geometry. Each record carries a two-letter country code
 * and nothing more; resolving that to a capital city would put ~874 incident
 * response teams on ~100 invented points. `country` stays a property.
 *
 * R3 — a page that fetched but held nothing is not an error. -1 is returned
 * only if the FIRST page fetch fails outright; a later page failing just ends
 * the walk with whatever really came back.
 *
 * Licence: FIRST publishes this as a public API (access:"public" in the
 * response envelope). Keyless. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIRST_PAGE_SIZE 100
/* Runaway guard, not an editorial bound: the walk normally ends on `total`
 * (878 teams at the last probe, so it stops at page 9) or on an empty page.
 * FIRST is a growing membership, though, and a ceiling that quietly clipped
 * the directory once it passed 1,200 teams would be invisible — so a run that
 * ends here says so with a collector-truncation-notice, and the ceiling is
 * raisable with $JO_FIRST_MAX_PAGES. */
#define FIRST_MAX_PAGES 12   /* exhaustive-ok: page-walk runaway guard; an early stop emits a collector-truncation-notice */

static void put(cJSON *p, const char *out_key, cJSON *rec, const char *in_key) {
  const char *v = jo_sv(rec, in_key);
  if (v) cJSON_AddStringToObject(p, out_key, v);
}

static int emit_team(intel_sink *s, cJSON *t) {
  const char *team = jo_sv(t, "team");
  if (!team) return 0;                  /* no fetched name -> no row (R1) */
  const char *country = jo_sv(t, "country");
  const char *host    = jo_sv(t, "host");
  const char *id      = jo_sv(t, "id");

  char title[512];
  if (country) snprintf(title, sizeof title, "%s (%s)", team, country);
  else         snprintf(title, sizeof title, "%s", team);

  char rk[256];
  snprintf(rk, sizeof rk, "%s", id ? id : team);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "team", team);
  put(p, "first_team_id",   t, "id");
  put(p, "country",         t, "country");
  put(p, "host",            t, "host");
  put(p, "team_type",       t, "team-type");
  put(p, "email",           t, "email");
  put(p, "establishment",   t, "establishment");
  put(p, "pgp_fingerprint", t, "pgp-fingerprint");
  put(p, "website",         t, "website");
  put(p, "constituency",    t, "constituency");
  put(p, "phone",           t, "phone");
  cJSON_AddStringToObject(p, "source", "first_org_teams");
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  /* `website` is a URL FIRST published for this team, not a URL we built. */
  const char *link = jo_sv(t, "website");

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = host;
  it.summary         = host;
  it.link            = link;
  it.lang            = "en";
  it.record_type     = "csirt-team";
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"cyber\",\"csirt\",\"cert\",\"directory\",\"first\"]";
  int rc = s->emit(s, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  int total_rows = 0, pages_ok = 0;
  double total = -1;
  int max_pages = FIRST_MAX_PAGES, hit_ceiling = 0, walked_out = 0;
  const char *penv = getenv("JO_FIRST_MAX_PAGES");
  if (penv && *penv) { int v = atoi(penv); if (v > 0) max_pages = v; }
  char url[160];

  for (int page = 0; page < max_pages; page++) {
    snprintf(url, sizeof url,
             "https://api.first.org/data/v1/teams?limit=%d&offset=%d",
             FIRST_PAGE_SIZE, page * FIRST_PAGE_SIZE);
    cJSON *doc = feed_get_json_h(c->http, url, hdrs, 25000);
    if (!doc) {
      if (page == 0) {
        fprintf(stderr, "[first-csirt-team-directory] fetch/parse failed\n");
        return -1;                                   /* the fetch failed (R3) */
      }
      fprintf(stderr, "[first-csirt-team-directory] page %d failed; stopping "
                      "with %d rows\n", page, total_rows);
      break;
    }
    pages_ok++;
    cJSON *tot = cJSON_GetObjectItem(doc, "total");
    if (tot && cJSON_IsNumber(tot)) total = tot->valuedouble;
    cJSON *data = cJSON_GetObjectItem(doc, "data");
    int here = 0;
    if (cJSON_IsArray(data)) {
      cJSON *t;
      cJSON_ArrayForEach(t, data) here += emit_team(s, t);
    }
    cJSON_Delete(doc);
    total_rows += here;
    if (here == 0) { walked_out = 1; break; }   /* ran off the end of the set */
    if (total > 0 && (page + 1) * FIRST_PAGE_SIZE >= total) { walked_out = 1; break; }
    if (page + 1 == max_pages) hit_ceiling = 1;
  }
  fprintf(stderr, "[first-csirt-team-directory] emitted %d over %d pages\n",
          total_rows, pages_ok);
  /* The ceiling stopping the walk means the directory outgrew it. Report it as
   * data — a run that quietly returned the first 1,200 of 1,400 teams would
   * look identical to a complete one. */
  if (hit_ceiling && !walked_out)
    jo_trunc_notice(s, "first-csirt-team-directory", url, total_rows,
                    total > 0 ? (long)total : -1,
                    "the page-walk ceiling stopped the run before FIRST's team "
                    "directory was exhausted",
                    "raise $JO_FIRST_MAX_PAGES");
  return 0;                                 /* fetched fine; 0 rows is OK (R3) */
}

static const source_def cert_first_teams_def = {
  .id = "first-csirt-team-directory", .collector = "cyber",
  .name = "FIRST.org CSIRT Team Directory",
  .update_interval_sec = 604800, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.first.org/data/v1/teams",
  .description = "Global directory of FIRST member CSIRT/SOC teams with country "
                 "code, host organisation, contact address, constituency and PGP "
                 "fingerprint - who to contact for incident response in a given "
                 "country.",
  .license = "FIRST publishes this as a public API (access:\"public\" in the "
             "response envelope).",
  .free_tier = 1 };
REGISTER_SOURCE(cert_first_teams_def)
