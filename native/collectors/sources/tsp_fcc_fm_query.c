/* collectors/sources/tsp_fcc_fm_query.c
 * FCC FM broadcast station engineering database (FM Query).
 * Endpoint: https://transition.fcc.gov/fcc-bin/fmq?state=XX&...&serv=FM
 *   &freq=88.0&fre2=108.0&list=4&size=9   (keyless, one request per state,
 *   ~350 KB per state; the exact query string probed upstream is reused with
 *   only `state` varied)
 * Emits per record: callsign, frequency, service, channel, directional flag,
 *   class, licence status (LIC/CP/APP), community of licence, state, country,
 *   file number, ERP/HAAT engineering fields as published, facility id,
 *   licensee, and the transmitter site coordinates.
 *
 * TRAPS handled, quoted from the manifest:
 *  - "PIPE-DELIMITED (not comma)" and "leading and trailing '|', every field
 *    space-padded — trim each field. NO header row": psv_split + trim below.
 *  - "carry true transmitter-site coordinates in DMS. Parse the DMS properly —
 *    atof() on '30-20-00 N' yields 30." Here the DMS arrives as SIX separate
 *    pipe fields per record (hemisphere letter, degrees, minutes, decimal
 *    seconds for latitude, then the same for longitude), so each is converted
 *    on its own: dd = deg + min/60 + sec/3600, negated for S/W. A single
 *    strtod over a whole DMS string is exactly the bug being avoided.
 *  - "This is the antenna location, not the licensee's mailing address" — the
 *    R2 trap for this domain. The licensee NAME is emitted; the licensee's
 *    address is never used for geometry, and a record whose DMS block cannot
 *    be located is skipped and counted rather than being placed somewhere.
 *  - "column order is fixed by the list=4 output format": the six-field DMS
 *    block is located by PATTERN (N/S … E/W with numeric fields between), and
 *    facility id / licensee are read relative to it, so a column shift cannot
 *    silently mis-assign coordinates.
 *  - "Query is per-state (state=XX) — 50+ requests to cover the US": all 50
 *    states plus DC and PR are walked, under a wall-clock budget; partial
 *    coverage returns what was fetched (R3), never -1.
 * Licence: FCC Media Bureau public query tool — US Government public domain.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FMQ_URL_FMT \
  "https://transition.fcc.gov/fcc-bin/fmq?state=%s&call=&city=&arn=&serv=FM" \
  "&vac=&freq=88.0&fre2=108.0&facid=&class=&dkt=&list=4&dist=&dlat2=&mlat2=" \
  "&slat2=&NS=N&dlon2=&mlon2=&slon2=&EW=W&size=9"
#define MAXF 64
#define MAX_ROWS 40000
#define BUDGET_SEC 240

static const char *const STATES[] = {
  "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA","HI","ID","IL","IN","IA",
  "KS","KY","LA","ME","MD","MA","MI","MN","MS","MO","MT","NE","NV","NH","NJ",
  "NM","NY","NC","ND","OH","OK","OR","PA","RI","SC","SD","TN","TX","UT","VT",
  "VA","WA","WV","WI","WY","DC","PR"
};
#define NSTATES ((int)(sizeof STATES / sizeof STATES[0]))

static int psv_split(char *line, char **out, int max) {
  int n = 0;
  char *p = line;
  while (n < max) {
    char *s = p;
    while (*p && *p != '|') p++;
    if (*p == '|') { *p = '\0'; out[n++] = s; p++; }
    else { out[n++] = s; break; }
  }
  return n;
}
static char *trim(char *s) {
  while (*s == ' ' || *s == '\t') s++;
  size_t n = strlen(s);
  while (n && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = '\0';
  return s;
}
static int is_num(const char *s, double *v) {
  if (!s || !*s) return 0;
  char *e = NULL;
  double d = strtod(s, &e);
  if (!e || e == s) return 0;
  while (*e == ' ') e++;
  if (*e) return 0;
  *v = d;
  return 1;
}
static int hemi(const char *s, char a, char b) {
  return s && s[0] && s[1] == '\0' && (s[0] == a || s[0] == b);
}
/* Locate the six-field DMS block and convert it. Returns the index of the
 * N/S field, or -1. lat/lon are set only on success. */
static int dms_block(char **f, int nf, double *lat, double *lon) {
  for (int j = 1; j + 8 < nf; j++) {
    if (!hemi(f[j], 'N', 'S') || !hemi(f[j + 4], 'E', 'W')) continue;
    double d1, m1, s1, d2, m2, s2;
    if (!is_num(f[j + 1], &d1) || !is_num(f[j + 2], &m1) || !is_num(f[j + 3], &s1))
      continue;
    if (!is_num(f[j + 5], &d2) || !is_num(f[j + 6], &m2) || !is_num(f[j + 7], &s2))
      continue;
    double la = d1 + m1 / 60.0 + s1 / 3600.0;
    double lo = d2 + m2 / 60.0 + s2 / 3600.0;
    if (f[j][0] == 'S') la = -la;
    if (f[j + 4][0] == 'W') lo = -lo;
    if (la < -90.0 || la > 90.0 || lo < -180.0 || lo > 180.0) continue;
    if (la == 0.0 && lo == 0.0) continue;
    *lat = la; *lon = lo;
    return j;
  }
  return -1;
}

static int emit_state(const source_ctx *ctx, intel_sink *sink,
                      const char *state, int *nodms) {
  char url[512];
  snprintf(url, sizeof url, FMQ_URL_FMT, state);
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (fcc-fm-query)", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 45000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[fcc-fm-query] %s http status=%ld\n", state, hr.status);
    http_response_free(&hr);
    return -1;
  }
  char *body = hr.body;
  hr.body = NULL;
  http_response_free(&hr);

  int n = 0;
  char *p = body, *line;
  while ((line = jo_next_line_cr(&p)) != NULL) {
    if (!*line || !strchr(line, '|')) continue;
    char *f[MAXF];
    int nf = psv_split(line, f, MAXF);
    for (int i = 0; i < nf; i++) f[i] = trim(f[i]);

    double lat = 0, lon = 0;
    int j = dms_block(f, nf, &lat, &lon);
    if (j < 0) { (*nodms)++; continue; }   /* no site coordinates -> skipped */

    const char *call    = jo_field(f, nf, 1);
    const char *freq    = jo_field(f, nf, 2);
    const char *service = jo_field(f, nf, 3);
    const char *chan    = jo_field(f, nf, 4);
    const char *dir     = jo_field(f, nf, 5);
    const char *cls     = jo_field(f, nf, 7);
    const char *lstat   = jo_field(f, nf, 9);
    const char *comm    = jo_field(f, nf, 10);
    const char *st      = jo_field(f, nf, 11);
    const char *ctry    = jo_field(f, nf, 12);
    const char *fileno  = jo_field(f, nf, 13);
    const char *erp_h   = jo_field(f, nf, 14);
    const char *erp_v   = jo_field(f, nf, 15);
    const char *haat_h  = jo_field(f, nf, 16);
    const char *haat_v  = jo_field(f, nf, 17);
    const char *facid   = jo_field(f, nf, j - 1);
    const char *licensee = jo_field(f, nf, j + 8);
    if (!call && !facid) continue;             /* no identity -> no row (R1) */

    cJSON *pr = cJSON_CreateObject();
    if (call)     cJSON_AddStringToObject(pr, "callsign", call);
    if (freq)     cJSON_AddStringToObject(pr, "frequency", freq);
    if (service)  cJSON_AddStringToObject(pr, "service", service);
    if (chan)     cJSON_AddStringToObject(pr, "channel", chan);
    if (dir)      cJSON_AddStringToObject(pr, "directional", dir);
    if (cls)      cJSON_AddStringToObject(pr, "class", cls);
    if (lstat)    cJSON_AddStringToObject(pr, "licence_status", lstat);
    if (comm)     cJSON_AddStringToObject(pr, "community", comm);
    if (st)       cJSON_AddStringToObject(pr, "state", st);
    if (ctry)     cJSON_AddStringToObject(pr, "country", ctry);
    if (fileno)   cJSON_AddStringToObject(pr, "file_number", fileno);
    if (erp_h)    cJSON_AddStringToObject(pr, "erp_horizontal", erp_h);
    if (erp_v)    cJSON_AddStringToObject(pr, "erp_vertical", erp_v);
    if (haat_h)   cJSON_AddStringToObject(pr, "haat_horizontal_m", haat_h);
    if (haat_v)   cJSON_AddStringToObject(pr, "haat_vertical_m", haat_v);
    if (facid)    cJSON_AddStringToObject(pr, "facility_id", facid);
    if (licensee) cJSON_AddStringToObject(pr, "licensee", licensee);
    cJSON_AddNumberToObject(pr, "site_latitude", lat);
    cJSON_AddNumberToObject(pr, "site_longitude", lon);
    cJSON_AddStringToObject(pr, "geo_subject", "transmitter antenna site (DMS from FCC)");
    cJSON_AddStringToObject(pr, "query_state", state);
    cJSON_AddStringToObject(pr, "source", "FCC FM Query (transition.fcc.gov/fcc-bin/fmq)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[256], summary[288], key[128];
    snprintf(key, sizeof key, "%s|%s|%s", facid ? facid : "", call ? call : "",
             fileno ? fileno : "");
    snprintf(title, sizeof title, "%s %s%s%s%s%s", call ? call : "NEW",
             freq ? freq : "", comm ? " — " : "", comm ? comm : "",
             st ? ", " : "", st ? st : "");
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
             lstat ? lstat : "status n/a",
             cls ? " · class " : "", cls ? cls : "",
             erp_h ? " · ERP " : "", erp_h ? erp_h : "",
             licensee ? " · " : "", licensee ? licensee : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.fcc.gov/media/radio/fm-query";
    it.lang            = "en";
    it.record_type     = "fm-transmitter";
    it.has_geo         = 1;                     /* real transmitter site (R2) */
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"broadcast\",\"fm\",\"fcc\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  free(body);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t t0 = time(NULL);
  int total = 0, ok_states = 0, nodms = 0;
  for (int i = 0; i < NSTATES; i++) {
    if (total >= MAX_ROWS) break;
    if (time(NULL) - t0 > BUDGET_SEC) {
      fprintf(stderr, "[fcc-fm-query] wall-clock budget reached after %d state(s)\n",
              ok_states);
      break;
    }
    int got = emit_state(ctx, sink, STATES[i], &nodms);
    if (got >= 0) { total += got; ok_states++; }
  }
  if (ok_states == 0) {
    fprintf(stderr, "[fcc-fm-query] no state query succeeded\n");
    return -1;
  }
  fprintf(stderr, "[fcc-fm-query] emitted %d over %d/%d states "
                  "(%d records skipped: no site DMS)\n",
          total, ok_states, NSTATES, nodms);
  return 0;
}

static const source_def tsp_fcc_fm_query_def = {
  .id = "fcc-fm-query", .collector = "telecom",
  .name = "FCC FM broadcast station engineering database (FM Query)",
  .update_interval_sec = 604800, .run = run,
  .category = "telecom", .type = "dataset",
  .url = "https://transition.fcc.gov/fcc-bin/fmq",
  .description = "Every licensed and applied-for US full-power FM transmitter: callsign, frequency, class, ERP, HAAT, licensee and the actual transmitter site converted from the FCC's degrees/minutes/seconds fields.",
  .license = "FCC Media Bureau public query tool — US Government public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_fcc_fm_query_def)
