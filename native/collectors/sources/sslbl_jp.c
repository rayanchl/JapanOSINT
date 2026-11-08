/* collectors/cyber/sources/sslbl_jp.c
 * Port of server/src/collectors/sslblJp.js (createThreatIntelCollector).
 * Keyless. SSLBL IP CSV intersected with Feodo JP IP set, TOKYO points.
 * JS parseCsv is hand-rolled (split lines, split ',', strip ^"|"$, >=4 cols). */
#include "source.h"
#include "lib/threatintel.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define URL_IP_CSV "https://sslbl.abuse.ch/blacklist/sslipblacklist.csv"
#define FEODO_JSON "https://feodotracker.abuse.ch/downloads/ipblocklist.json"

/* strip a single leading and trailing '"' in place (JS replace /^"|"$/g). */
static char *strip_q(char *s) {
  size_t n = strlen(s);
  if (n && s[n - 1] == '"') s[--n] = 0;
  if (s[0] == '"') return s + 1;
  return s;
}

/* trim ASCII whitespace in place, return start ptr. */
static char *trim(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
  size_t n = strlen(s);
  while (n && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n')) s[--n]=0;
  return s;
}

/* Did each of the two upstreams PARSE? Zero Japanese IPs on an SSL blacklist
 * is a perfectly honest day, so the record count can never be the
 * discriminator here — but this row needs BOTH bodies by construction (the JP
 * address set comes from Feodo, the rows from the SSLBL CSV), so either one
 * failing makes zero rows guaranteed rather than honest. The channel is
 * threatintel_collect's own `ud` pointer, so no other caller of that helper is
 * touched: lib/threatintel.c already treats a NULL from the fetcher as the
 * did-not-parse case, and this fetcher can never return NULL — it builds and
 * returns `features` unconditionally — so that existing signal was unreachable
 * from here and both failures collapsed into "no JP IP today". */
typedef struct { int feodo_parsed, csv_parsed; } sslbl_reads;

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key;
  sslbl_reads *rd = (sslbl_reads *)ud;

  /* 1. Build the JP IP set from Feodo JSON (country === 'JP'). */
  cJSON *feodo = feed_get_json(ctx->http, FEODO_JSON, 12000);
  if (rd) rd->feodo_parsed = cJSON_IsArray(feodo) ? 1 : 0;
  cJSON *jp_set = cJSON_CreateObject();   /* used as a string set */
  int jp_count = 0;
  if (cJSON_IsArray(feodo)) {
    cJSON *row;
    cJSON_ArrayForEach(row, feodo) {
      cJSON *c = cJSON_GetObjectItem(row, "country");
      const char *cc = (c && cJSON_IsString(c)) ? c->valuestring : "";
      if ((cc[0]=='J'||cc[0]=='j') && (cc[1]=='P'||cc[1]=='p') && !cc[2]) {
        cJSON *ipv = cJSON_GetObjectItem(row, "ip_address");
        if (ipv && cJSON_IsString(ipv) && ipv->valuestring[0]) {
          if (!cJSON_GetObjectItem(jp_set, ipv->valuestring)) {
            cJSON_AddBoolToObject(jp_set, ipv->valuestring, 1);
            jp_count++;
          }
        }
      }
    }
  }
  if (feodo) cJSON_Delete(feodo);

  /* 2. Fetch SSLBL CSV text. */
  char *csv = feed_get_text(ctx->http, URL_IP_CSV, 12000);
  if (rd) rd->csv_parsed = csv ? 1 : 0;
  cJSON *features = cJSON_CreateArray();
  if (csv && jp_count > 0) {
    int idx = 0;
    char *save = NULL;
    for (char *line = strtok_r(csv, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
      char *t = trim(line);
      if (!*t || t[0] == '#') continue;
      /* split on ',', strip quotes, need >=4 cols. */
      char *cols[8]; int nc = 0;
      char *cs = NULL;
      for (char *tok = strtok_r(t, ",", &cs); tok && nc < 8;
           tok = strtok_r(NULL, ",", &cs)) {
        cols[nc++] = strip_q(tok);
      }
      if (nc < 4) continue;
      const char *first_seen = cols[0], *ip = cols[1],
                 *port = cols[2], *malware = cols[3];
      if (!cJSON_GetObjectItem(jp_set, ip)) continue;  /* jpIps.has(r.ip) */

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* NO GEOMETRY. Every row here used to be emitted at Tokyo Station
       * (35.6895, 139.6917). What this source reports has no location,
       * and stacking every row on one pin is the fabrication the
       * 2026-07-31 audit deleted from cisa-kev-jp, poc-in-github and
       * peeringdb-jp. Confirmed live by tests/contract/source_contract.py.
       * lib/geojson.c handles an absent geometry correctly — do NOT
       * reintroduce a fallback coordinate. */

      cJSON *p = cJSON_CreateObject();
      cJSON_AddNumberToObject(p, "idx", idx);
      cJSON_AddStringToObject(p, "first_seen", first_seen);
      cJSON_AddStringToObject(p, "ip", ip);
      cJSON_AddStringToObject(p, "port", port);
      cJSON_AddStringToObject(p, "malware", malware);
      cJSON_AddStringToObject(p, "source", "sslbl");  /* live path only */
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      idx++;
    }
  }
  if (csv) free(csv);
  cJSON_Delete(jp_set);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  sslbl_reads rd = {0};
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, &rd);
  /* `return n >= 0 ? 0 : -1` could only be -1 for a programming error (the
   * helper's one negative return is a NULL fetcher pointer), so this row has
   * reported success on every run of its life. Measured 2026-11-07: 2 requests,
   * 2,308 bytes, 0 records — far too small for either real feed, which
   * lib/threatintel.h records as keyless-200 as of 2026-08-01, so this is
   * drift and not a credential gate.
   *
   * An honest empty is preserved exactly: both bodies parsing and no Japanese
   * address among them returns 0, which is a normal day on this blacklist. */
  if (!rd.feodo_parsed || !rd.csv_parsed) {
    fprintf(stderr, "[sslbl-jp] %s%s%s did not parse — reporting a failed "
                    "fetch, not an absence of Japanese addresses on the "
                    "blacklist\n",
            rd.feodo_parsed ? "" : "the Feodo JSON",
            (!rd.feodo_parsed && !rd.csv_parsed) ? " and " : "",
            rd.csv_parsed ? "" : "the SSLBL CSV");
    return -1;
  }
  return n >= 0 ? 0 : -1;
}

static const source_def sslbl_jp_def = {
  .id = "sslbl-jp", .collector = "cyber",
  .name = "abuse.ch SSLBL (JP)", .name_ja = "abuse.ch SSLBL 日本",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(sslbl_jp_def)
