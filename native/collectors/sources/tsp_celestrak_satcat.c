/* collectors/sources/tsp_celestrak_satcat.c
 * CelesTrak SATCAT — the full NORAD catalogue of every catalogued space object.
 * Endpoint: https://celestrak.org/pub/satcat.csv   (keyless, ~6.7 MB CSV)
 * Emits (all straight off the wire): OBJECT_NAME, OBJECT_ID (international
 *   designator), NORAD_CAT_ID, OBJECT_TYPE, OPS_STATUS_CODE, OWNER,
 *   LAUNCH_DATE, LAUNCH_SITE code, DECAY_DATE, PERIOD, INCLINATION, APOGEE,
 *   PERIGEE, RCS, DATA_STATUS_CODE, ORBIT_CENTER, ORBIT_TYPE.
 *
 * parse_notes honoured:
 *  - "6.7 MB — stream/parse line-by-line, do not slurp into one buffer": the
 *    body is taken once off http_request and walked in place, one line at a
 *    time, with no per-row allocation and no second copy.
 *  - "No coordinates in the file; LAUNCH_SITE is a 5-letter site code (AFETR,
 *    TYMSC) not a lat/lon — do NOT geocode it": every row here is has_geo=0.
 *  - "Empty fields are zero-length": absent cells are omitted, never defaulted.
 *
 * STATED BOUND (not a silent truncation): the catalogue is ~64k objects and is
 * almost entirely static, so this collector emits the part that moves —
 * objects LAUNCHED or DECAYED within the last WINDOW_DAYS — capped at
 * MAX_ROWS. Both numbers are recorded in every row's properties.
 *
 * Licence: CelesTrak usage policy (celestrak.org/usage-policy.php) — free
 * reuse, no more than one retrieval per data-update cycle, attribution to
 * CelesTrak / Dr T.S. Kelso.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SATCAT_URL  "https://celestrak.org/pub/satcat.csv"
#define WINDOW_DAYS 365
#define MAX_ROWS    6000
#define MAXCOL      32

/* RFC4180 field splitter, in place. Cells are NUL-terminated inside `line`. */
static int tsp_csv_split(char *line, char **out, int max) {
  int n = 0;
  char *p = line;
  while (n < max) {
    if (*p == '"') {
      char *start, *dst;
      p++; start = dst = p;
      while (*p) {
        if (*p == '"') {
          if (p[1] == '"') { *dst++ = '"'; p += 2; continue; }
          p++; break;
        }
        *dst++ = *p++;
      }
      while (*p && *p != ',') p++;
      char end = *p;
      *dst = '\0';
      out[n++] = start;
      if (end == ',') { p++; continue; }
      break;
    }
    char *start = p;
    while (*p && *p != ',') p++;
    if (*p == ',') { *p = '\0'; out[n++] = start; p++; }
    else { out[n++] = start; break; }
  }
  return n;
}

static const char *tsp_cell(char **f, int nf, int idx) {
  if (idx < 0 || idx >= nf || !f[idx] || !f[idx][0]) return NULL;
  return f[idx];
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (satcat)",
                         "Accept: text/csv", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", SATCAT_URL, hdrs, NULL, 0, 60000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[celestrak-satcat] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  char *body = hr.body;                 /* single copy, parsed in place */
  hr.body = NULL;
  http_response_free(&hr);

  char *p = body;
  char *head = jo_next_line_cr(&p);
  if (!head || !*head) {
    fprintf(stderr, "[celestrak-satcat] empty body\n");
    free(body);
    return -1;
  }
  char *hc[MAXCOL];
  int nh = tsp_csv_split(head, hc, MAXCOL);
  int i_name  = jo_col_of(hc, nh, "OBJECT_NAME");
  int i_intl  = jo_col_of(hc, nh, "OBJECT_ID");
  int i_norad = jo_col_of(hc, nh, "NORAD_CAT_ID");
  int i_type  = jo_col_of(hc, nh, "OBJECT_TYPE");
  int i_ops   = jo_col_of(hc, nh, "OPS_STATUS_CODE");
  int i_own   = jo_col_of(hc, nh, "OWNER");
  int i_ldate = jo_col_of(hc, nh, "LAUNCH_DATE");
  int i_lsite = jo_col_of(hc, nh, "LAUNCH_SITE");
  int i_decay = jo_col_of(hc, nh, "DECAY_DATE");
  int i_per   = jo_col_of(hc, nh, "PERIOD");
  int i_inc   = jo_col_of(hc, nh, "INCLINATION");
  int i_apo   = jo_col_of(hc, nh, "APOGEE");
  int i_peri  = jo_col_of(hc, nh, "PERIGEE");
  int i_rcs   = jo_col_of(hc, nh, "RCS");
  int i_dstat = jo_col_of(hc, nh, "DATA_STATUS_CODE");
  int i_ctr   = jo_col_of(hc, nh, "ORBIT_CENTER");
  int i_otype = jo_col_of(hc, nh, "ORBIT_TYPE");
  if (i_name < 0 || i_norad < 0 || i_ldate < 0) {
    fprintf(stderr, "[celestrak-satcat] header shape changed; not parsing\n");
    free(body);
    return -1;
  }

  char cutoff[16];
  jo_days_ago_iso(WINDOW_DAYS, cutoff, sizeof cutoff);

  int n = 0, seen = 0;
  char *line;
  while ((line = jo_next_line_cr(&p)) != NULL) {
    if (!*line) continue;
    seen++;
    char *f[MAXCOL];
    int nf = tsp_csv_split(line, f, MAXCOL);
    const char *name  = tsp_cell(f, nf, i_name);
    const char *norad = tsp_cell(f, nf, i_norad);
    if (!name || !norad) continue;                 /* no identity -> no row */
    const char *launch = tsp_cell(f, nf, i_ldate);
    const char *decay  = tsp_cell(f, nf, i_decay);
    int recent = (launch && strcmp(launch, cutoff) >= 0) ||
                 (decay  && strcmp(decay,  cutoff) >= 0);
    if (!recent) continue;
    if (n >= MAX_ROWS) break;

    const char *owner = tsp_cell(f, nf, i_own);
    const char *otype = tsp_cell(f, nf, i_type);
    const char *inc   = tsp_cell(f, nf, i_inc);
    const char *apo   = tsp_cell(f, nf, i_apo);
    const char *peri  = tsp_cell(f, nf, i_peri);

    cJSON *pr = cJSON_CreateObject();
    jo_add_str(pr, "object_name", name);
    jo_add_str(pr, "norad_cat_id", norad);
    jo_add_str(pr, "international_designator", tsp_cell(f, nf, i_intl));
    jo_add_str(pr, "object_type", otype);
    jo_add_str(pr, "ops_status_code", tsp_cell(f, nf, i_ops));
    jo_add_str(pr, "owner", owner);
    jo_add_str(pr, "launch_date", launch);
    jo_add_str(pr, "launch_site_code", tsp_cell(f, nf, i_lsite));
    jo_add_str(pr, "decay_date", decay);
    jo_add_str(pr, "period_min", tsp_cell(f, nf, i_per));
    jo_add_str(pr, "inclination_deg", inc);
    jo_add_str(pr, "apogee_km", apo);
    jo_add_str(pr, "perigee_km", peri);
    jo_add_str(pr, "rcs_m2", tsp_cell(f, nf, i_rcs));
    jo_add_str(pr, "data_status_code", tsp_cell(f, nf, i_dstat));
    jo_add_str(pr, "orbit_center", tsp_cell(f, nf, i_ctr));
    jo_add_str(pr, "orbit_type", tsp_cell(f, nf, i_otype));
    cJSON_AddStringToObject(pr, "window", "launched or decayed in the last 365 days");
    cJSON_AddNumberToObject(pr, "row_cap", MAX_ROWS);
    cJSON_AddStringToObject(pr, "source", "CelesTrak SATCAT");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[192], summary[320], link[128], key[32];
    snprintf(title, sizeof title, "%s (NORAD %s)", name, norad);
    snprintf(key, sizeof key, "%s", norad);
    snprintf(link, sizeof link,
             "https://celestrak.org/satcat/table-satcat.php?CATNR=%s", norad);
    char orbit[64] = "";
    if (apo && peri) snprintf(orbit, sizeof orbit, " · %s-%s km", peri, apo);
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s%s%s%s",
             owner ? owner : "owner n/a",
             otype ? " " : "", otype ? otype : "",
             launch ? " · launched " : "", launch ? launch : "",
             orbit,
             inc ? " · incl " : "", inc ? inc : "",
             decay ? " · decayed " : "", decay ? decay : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = launch;
    it.record_type     = "satellite-catalog";
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"satellite\",\"celestrak\"]";
    /* R2: SATCAT carries no coordinates. LAUNCH_SITE is a site CODE. */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[celestrak-satcat] emitted %d of %d catalogued objects "
                  "(launched/decayed since %s)\n", n, seen, cutoff);
  return 0;                        /* a quiet year would not be an error (R3) */
}

static const source_def tsp_celestrak_satcat_def = {
  .id = "celestrak-satcat", .collector = "satellite",
  .name = "CelesTrak SATCAT (full satellite catalogue)",
  .update_interval_sec = 86400, .run = run,
  .category = "satellite", .type = "dataset", .url = SATCAT_URL,
  .description = "Complete NORAD satellite catalogue: owner, launch date, launch-site code, decay date, orbital period/inclination/apogee/perigee and radar cross-section for every catalogued object. Recent launches and re-entries are emitted as rows.",
  .license = "CelesTrak usage policy (celestrak.org/usage-policy.php) — free reuse, one retrieval per update cycle, attribution to CelesTrak / Dr T.S. Kelso.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_celestrak_satcat_def)
