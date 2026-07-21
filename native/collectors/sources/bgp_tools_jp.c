/* collectors/telecom/sources/bgp_tools_jp.c — port of
 * server/src/collectors/bgpToolsJp.js.
 * bgp.tools/asns.csv (needs a real contact-email UA or 403) intersected with
 * the RIPEstat JP-country ASN set. SEED/_meta dropped (rule 7); the JS
 * `ua_required` branch yields 0 features so we likewise emit 0 when no UA.
 * Features pinned at Tokyo; props order: idx, asn, name, url, source. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* portable case-insensitive substring (avoids GNU strcasestr) */
static int ci_contains(const char *hay, const char *needle) {
  if (!hay || !needle || !*needle) return 0;
  size_t nl = strlen(needle);
  for (const char *h = hay; *h; h++) {
    size_t i = 0;
    while (i < nl && h[i] &&
           tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) i++;
    if (i == nl) return 1;
  }
  return 0;
}

#define URL_ASNS "https://bgp.tools/asns.csv"
#define URL_RIPESTAT_JP "https://stat.ripe.net/data/country-resource-list/data.json?resource=JP"
#define TIMEOUT_MS 30000

/* RIPEstat JP ASN set as a sorted long array; membership via bsearch. */
static int cmp_long(const void *a, const void *b) {
  long x = *(const long *)a, y = *(const long *)b;
  return x < y ? -1 : x > y ? 1 : 0;
}

static long *fetch_jp_asn_set(http_client *http, size_t *out_n) {
  *out_n = 0;
  cJSON *j = feed_get_json(http, URL_RIPESTAT_JP, TIMEOUT_MS);
  if (!j) return NULL;
  cJSON *data = cJSON_GetObjectItem(j, "data");
  cJSON *res = data ? cJSON_GetObjectItem(data, "resources") : NULL;
  cJSON *asn = res ? cJSON_GetObjectItem(res, "asn") : NULL;
  size_t cap = 256, n = 0;
  long *set = malloc(cap * sizeof(long));
  cJSON *item;
  cJSON_ArrayForEach(item, asn) {
    char s[64] = {0};
    if (cJSON_IsString(item)) snprintf(s, sizeof s, "%s", item->valuestring);
    else if (cJSON_IsNumber(item)) snprintf(s, sizeof s, "%lld", (long long)item->valuedouble);
    else continue;
    /* match /^(\d+)(?:-(\d+))?$/ */
    char *dash = strchr(s, '-');
    char *e;
    long start = strtol(s, &e, 10);
    if (e == s) continue;
    long end = start;
    if (dash) {
      char *e2;
      end = strtol(dash + 1, &e2, 10);
      if (e2 == dash + 1) continue;
    } else if (*e != 0) {
      continue;
    }
    for (long v = start; v <= end && v - start < 5000; v++) {
      if (n == cap) { cap *= 2; set = realloc(set, cap * sizeof(long)); }
      set[n++] = v;
    }
  }
  cJSON_Delete(j);
  qsort(set, n, sizeof(long), cmp_long);
  *out_n = n;
  return set;
}

/* parseCsvAsn: skip blank / "asn"-prefixed lines; split on first comma;
 * strip leading/trailing quote from name; numeric asn only. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *ua = getenv("BGPTOOLS_USER_AGENT");
  if (!ua || !*ua || ci_contains(ua, "example.com") || ci_contains(ua, "@example") ||
      !strchr(ua, '@')) {
    fprintf(stderr, "[bgp-tools-jp] no valid BGPTOOLS_USER_AGENT — 0 rows\n");
    return 0;
  }

  size_t jp_n = 0;
  long *jp = fetch_jp_asn_set(ctx->http, &jp_n);

  char ua_hdr[512];
  snprintf(ua_hdr, sizeof ua_hdr, "User-Agent: %s", ua);
  const char *headers[] = { ua_hdr, NULL };
  http_response r = {0};
  int rc = http_request(ctx->http, "GET", URL_ASNS, headers, NULL, 0,
                         TIMEOUT_MS, 0, &r);
  if (rc != 0 || r.status < 200 || r.status >= 300 || !r.body) {
    http_response_free(&r);
    free(jp);
    fprintf(stderr, "[bgp-tools-jp] asns.csv fetch failed (status %ld)\n", r.status);
    return -1;
  }

  cJSON *features = cJSON_CreateArray();
  int idx = 0;
  if (jp && jp_n > 0) {
    for (char *p = r.body, *nl; p && *p && idx < 4000; p = nl ? nl + 1 : NULL) {
      nl = strchr(p, '\n');
      size_t len = nl ? (size_t)(nl - p) : strlen(p);
      /* trim */
      char *line = p;
      while (len && (*line == ' ' || *line == '\t' || *line == '\r')) { line++; len--; }
      while (len && (line[len-1]==' '||line[len-1]=='\t'||line[len-1]=='\r')) len--;
      if (!len) continue;
      char buf[512];
      size_t cl = len < sizeof(buf)-1 ? len : sizeof(buf)-1;
      memcpy(buf, line, cl); buf[cl] = 0;
      if (strncmp(buf, "asn", 3) == 0) continue;
      char *comma = strchr(buf, ',');
      if (!comma) continue;
      *comma = 0;
      char *e;
      long asn = strtol(buf, &e, 10);
      if (e == buf) continue;            /* !Number.isFinite */
      if (!bsearch(&asn, jp, jp_n, sizeof(long), cmp_long)) continue;
      char *name = comma + 1;
      size_t nl2 = strlen(name);
      if (nl2 && name[0] == '"') { name++; nl2--; }
      if (nl2 && name[nl2-1] == '"') name[nl2-1] = 0;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(139.6917));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(35.6895));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);
      cJSON *pr = cJSON_CreateObject();
      cJSON_AddNumberToObject(pr, "idx", idx);
      cJSON_AddNumberToObject(pr, "asn", (double)asn);
      cJSON_AddStringToObject(pr, "name", name);
      char urlbuf[64];
      snprintf(urlbuf, sizeof urlbuf, "https://bgp.tools/as/%ld", asn);
      cJSON_AddStringToObject(pr, "url", urlbuf);
      cJSON_AddStringToObject(pr, "source", "bgp_tools");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      idx++;
    }
  }
  http_response_free(&r);
  free(jp);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[bgp-tools-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def bgp_tools_jp_def = {
  .id = "bgp-tools-jp", .collector = "telecom",
  .name = "BGP.tools (JP ASNs)", .name_ja = "BGP.tools (日本AS)",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(bgp_tools_jp_def)
