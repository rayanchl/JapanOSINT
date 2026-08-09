/* collectors/sources/tsp_fcc_am_query.c
 * FCC AM broadcast station engineering database (AM Query).
 * Endpoint: https://transition.fcc.gov/fcc-bin/amq?state=XX&list=4&size=9
 *   (keyless, one request per state, ~200 KB per state)
 * Emits per record: callsign, frequency (kHz), service, NCE flag, day/night
 *   code and text, class, licence status, community of licence, state,
 *   country, file number, power, directional flag and antenna pattern id,
 *   facility id, licensee, and the antenna field's real coordinates.
 *
 * TRAPS handled, quoted from the manifest:
 *  - "Same pipe-delimited, space-padded, header-less layout as fmq but a
 *    DIFFERENT column count/order — parse the two separately": this is a
 *    separate collector, and the coordinate block is located by PATTERN rather
 *    than by a hardcoded column index, so a layout difference cannot silently
 *    mis-assign a coordinate.
 *  - "Geo again is transmitter-site DMS across six fields (N/S, deg, min, sec,
 *    E/W, deg, min, sec)". The domain trap — "atof() on '30-20-00 N' yields
 *    30" — is avoided by converting each of the six fields separately:
 *    dd = deg + min/60 + sec/3600, negated for S/W. This is the ANTENNA FIELD,
 *    not the licensee's mailing address (R2); a record without a locatable DMS
 *    block is skipped and counted, never placed.
 *  - "Each station appears multiple times (DAY / NIG / CH); de-duplicate on
 *    facility_id + day_night_code": that pair is the remote_key.
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

#define AMQ_URL_FMT "https://transition.fcc.gov/fcc-bin/amq?state=%s&list=4&size=9"
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
  snprintf(url, sizeof url, AMQ_URL_FMT, state);
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (fcc-am-query)", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 45000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[fcc-am-query] %s http status=%ld\n", state, hr.status);
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
    const char *freq    = jo_field(f, nf, 2);
    const char *service = jo_field(f, nf, 3);
    const char *nce     = jo_field(f, nf, 4);
    const char *dncode  = jo_field(f, nf, 5);
    const char *dntext  = jo_field(f, nf, 6);
    const char *cls     = jo_field(f, nf, 7);
    const char *lstat   = jo_field(f, nf, 9);
    const char *comm    = jo_field(f, nf, 10);
    const char *st      = jo_field(f, nf, 11);
    const char *ctry    = jo_field(f, nf, 12);
    const char *fileno  = jo_field(f, nf, 13);
    const char *power   = jo_field(f, nf, 14);
    const char *dirflag = jo_field(f, nf, 15);
    const char *pattern = jo_field(f, nf, 16);
    const char *facid   = jo_field(f, nf, j - 1);
    const char *licensee = jo_field(f, nf, j + 8);
    if (!call && !facid) continue;

    cJSON *pr = cJSON_CreateObject();
    if (call)     cJSON_AddStringToObject(pr, "callsign", call);
    if (freq)     cJSON_AddStringToObject(pr, "frequency_khz", freq);
    if (service)  cJSON_AddStringToObject(pr, "service", service);
    if (nce)      cJSON_AddStringToObject(pr, "nce", nce);
    if (dncode)   cJSON_AddStringToObject(pr, "day_night_code", dncode);
    if (dntext)   cJSON_AddStringToObject(pr, "day_night_text", dntext);
    if (cls)      cJSON_AddStringToObject(pr, "class", cls);
    if (lstat)    cJSON_AddStringToObject(pr, "licence_status", lstat);
    if (comm)     cJSON_AddStringToObject(pr, "community", comm);
    if (st)       cJSON_AddStringToObject(pr, "state", st);
    if (ctry)     cJSON_AddStringToObject(pr, "country", ctry);
    if (fileno)   cJSON_AddStringToObject(pr, "file_number", fileno);
    if (power)    cJSON_AddStringToObject(pr, "power", power);
    if (dirflag)  cJSON_AddStringToObject(pr, "directional", dirflag);
    if (pattern)  cJSON_AddStringToObject(pr, "antenna_pattern", pattern);
    if (facid)    cJSON_AddStringToObject(pr, "facility_id", facid);
    if (licensee) cJSON_AddStringToObject(pr, "licensee", licensee);
    cJSON_AddNumberToObject(pr, "site_latitude", lat);
    cJSON_AddNumberToObject(pr, "site_longitude", lon);
    cJSON_AddStringToObject(pr, "geo_subject", "antenna field site (DMS from FCC)");
    cJSON_AddStringToObject(pr, "query_state", state);
    cJSON_AddStringToObject(pr, "source", "FCC AM Query (transition.fcc.gov/fcc-bin/amq)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[256], summary[288], key[128];
    snprintf(key, sizeof key, "%s|%s|%s", facid ? facid : (call ? call : ""),
             dncode ? dncode : "", fileno ? fileno : "");
    snprintf(title, sizeof title, "%s %s%s%s%s%s", call ? call : "NEW",
             freq ? freq : "", comm ? " — " : "", comm ? comm : "",
             st ? ", " : "", st ? st : "");
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
             dntext ? dntext : (dncode ? dncode : "operation n/a"),
             power ? " · " : "", power ? power : "",
             dirflag ? " · " : "", dirflag ? dirflag : "",
             licensee ? " · " : "", licensee ? licensee : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.fcc.gov/media/radio/am-query";
    it.lang            = "en";
    it.record_type     = "am-transmitter";
    it.has_geo         = 1;                     /* real antenna field (R2) */
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"broadcast\",\"am\",\"fcc\"]";
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
      fprintf(stderr, "[fcc-am-query] wall-clock budget reached after %d state(s)\n",
              ok_states);
      break;
    }
    int got = emit_state(ctx, sink, STATES[i], &nodms);
    if (got >= 0) { total += got; ok_states++; }
  }
  if (ok_states == 0) {
    fprintf(stderr, "[fcc-am-query] no state query succeeded\n");
    return -1;
  }
  fprintf(stderr, "[fcc-am-query] emitted %d over %d/%d states "
                  "(%d records skipped: no site DMS)\n",
          total, ok_states, NSTATES, nodms);
  return 0;
}

static const source_def tsp_fcc_am_query_def = {
  .id = "fcc-am-query", .collector = "telecom",
  .name = "FCC AM broadcast station engineering database (AM Query)",
  .update_interval_sec = 604800, .run = run,
  .category = "telecom", .type = "dataset",
  .url = "https://transition.fcc.gov/fcc-bin/amq",
  .description = "US medium-wave transmitter registry: frequency, day/night power, directional pattern id, class and licensee, with the antenna field's real coordinates and separate rows for day, night and critical-hours operation.",
  .license = "FCC Media Bureau public query tool — US Government public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_fcc_am_query_def)
