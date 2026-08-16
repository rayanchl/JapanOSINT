/* collectors/sources/tsp_fcc_tv_query.c
 * FCC TV broadcast station engineering database (TV Query) — full-power,
 * Class A, low-power and translator television transmitters, including pending
 * construction permits.
 * Endpoint: https://transition.fcc.gov/fcc-bin/tvq?state=XX&list=4&size=9
 *   (keyless, one request per state, ~350 KB per state)
 * Emits per record: callsign, channel info, service, RF channel, directional
 *   flag, analog/digital flag, class, licence status (LIC/CP/APP), community,
 *   state, country, file number, the ERP/HAAT engineering fields as published,
 *   facility id, licensee, and the transmitter site coordinates.
 *
 * TRAPS handled, quoted from the manifest:
 *  - "Pipe-delimited, space-padded, no header; column layout differs again
 *    from fmq/amq": separate collector, and the coordinate block is found by
 *    PATTERN (N/S … E/W with numeric fields between) rather than by a fixed
 *    index, so the differing layout cannot mis-assign a coordinate.
 *  - "Coordinates are transmitter-site DMS in the same six-field pattern" —
 *    and the domain trap, "atof() on '30-20-00 N' yields 30", is avoided by
 *    converting each of the six fields separately (dd = deg + min/60 +
 *    sec/3600, negated for S/W). This is the transmitter site, not the
 *    licensee's mailing address (R2).
 *  - "Applications with no callsign appear as 'NEW' — key on facility_id, not
 *    callsign": the remote_key leads with facility_id.
 *  - "Rows with '-' in a numeric field mean 'not specified', not zero": a '-'
 *    cell is treated as absent and the key is simply omitted.
 * Licence: FCC Media Bureau public query tool — US Government public domain.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TVQ_URL_FMT "https://transition.fcc.gov/fcc-bin/tvq?state=%s&list=4&size=9"
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
  char url[256];
  snprintf(url, sizeof url, TVQ_URL_FMT, state);
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (fcc-tv-query)", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 45000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[fcc-tv-query] %s http status=%ld\n", state, hr.status);
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
    if (j < 0) { (*nodms)++; continue; }

    const char *call    = jo_field(f, nf, 1);
    const char *chinfo  = jo_field(f, nf, 2);
    const char *service = jo_field(f, nf, 3);
    const char *chan    = jo_field(f, nf, 4);
    const char *dir     = jo_field(f, nf, 5);
    const char *anadig  = jo_field(f, nf, 6);
    const char *cls     = jo_field(f, nf, 7);
    const char *lstat   = jo_field(f, nf, 9);
    const char *comm    = jo_field(f, nf, 10);
    const char *st      = jo_field(f, nf, 11);
    const char *ctry    = jo_field(f, nf, 12);
    const char *fileno  = jo_field(f, nf, 13);
    const char *erp     = jo_field(f, nf, 14);
    const char *facid   = jo_field(f, nf, j - 1);
    const char *licensee = jo_field(f, nf, j + 8);
    if (!call && !facid) continue;

    cJSON *pr = cJSON_CreateObject();
    if (call)     cJSON_AddStringToObject(pr, "callsign", call);
    if (chinfo)   cJSON_AddStringToObject(pr, "channel_info", chinfo);
    if (service)  cJSON_AddStringToObject(pr, "service", service);
    if (chan)     cJSON_AddStringToObject(pr, "rf_channel", chan);
    if (dir)      cJSON_AddStringToObject(pr, "directional", dir);
    if (anadig)   cJSON_AddStringToObject(pr, "analog_digital", anadig);
    if (cls)      cJSON_AddStringToObject(pr, "class", cls);
    if (lstat)    cJSON_AddStringToObject(pr, "licence_status", lstat);
    if (comm)     cJSON_AddStringToObject(pr, "community", comm);
    if (st)       cJSON_AddStringToObject(pr, "state", st);
    if (ctry)     cJSON_AddStringToObject(pr, "country", ctry);
    if (fileno)   cJSON_AddStringToObject(pr, "file_number", fileno);
    if (erp)      cJSON_AddStringToObject(pr, "erp", erp);
    if (facid)    cJSON_AddStringToObject(pr, "facility_id", facid);
    if (licensee) cJSON_AddStringToObject(pr, "licensee", licensee);
    /* the remaining engineering cells vary by service class; keep them raw and
     * in file order rather than guessing names for them */
    cJSON *eng = cJSON_CreateArray();
    for (int i = 15; i < j - 1; i++) {
      const char *v = jo_field(f, nf, i);
      if (v) cJSON_AddItemToArray(eng, cJSON_CreateString(v));
    }
    cJSON_AddItemToObject(pr, "engineering_fields", eng);
    cJSON_AddNumberToObject(pr, "site_latitude", lat);
    cJSON_AddNumberToObject(pr, "site_longitude", lon);
    cJSON_AddStringToObject(pr, "geo_subject", "transmitter site (DMS from FCC)");
    cJSON_AddStringToObject(pr, "query_state", state);
    cJSON_AddStringToObject(pr, "source", "FCC TV Query (transition.fcc.gov/fcc-bin/tvq)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[256], summary[288], key[128];
    snprintf(key, sizeof key, "%s|%s|%s", facid ? facid : "",
             fileno ? fileno : "", chan ? chan : "");
    snprintf(title, sizeof title, "%s%s%s%s%s%s%s",
             call ? call : "NEW",
             chan ? " ch " : "", chan ? chan : "",
             comm ? " — " : "", comm ? comm : "",
             st ? ", " : "", st ? st : "");
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
             lstat ? lstat : "status n/a",
             service ? " · " : "", service ? service : "",
             erp ? " · ERP " : "", erp ? erp : "",
             licensee ? " · " : "", licensee ? licensee : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.fcc.gov/media/television/tv-query";
    it.lang            = "en";
    it.record_type     = "tv-transmitter";
    it.has_geo         = 1;                     /* real transmitter site (R2) */
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"broadcast\",\"tv\",\"fcc\"]";
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
      fprintf(stderr, "[fcc-tv-query] wall-clock budget reached after %d state(s)\n",
              ok_states);
      break;
    }
    int got = emit_state(ctx, sink, STATES[i], &nodms);
    if (got >= 0) { total += got; ok_states++; }
  }
  if (ok_states == 0) {
    fprintf(stderr, "[fcc-tv-query] no state query succeeded\n");
    return -1;
  }
  fprintf(stderr, "[fcc-tv-query] emitted %d over %d/%d states "
                  "(%d records skipped: no site DMS)\n",
          total, ok_states, NSTATES, nodms);
  return 0;
}

static const source_def tsp_fcc_tv_query_def = {
  .id = "fcc-tv-query", .collector = "telecom",
  .name = "FCC TV broadcast station engineering database (TV Query)",
  .update_interval_sec = 604800, .run = run,
  .category = "telecom", .type = "dataset",
  .url = "https://transition.fcc.gov/fcc-bin/tvq",
  .description = "US television transmitter registry — full-power, Class A, low-power and translator — with RF channel, ERP, licence status and transmitter site coordinates, including pending construction permits.",
  .license = "FCC Media Bureau public query tool — US Government public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_fcc_tv_query_def)
