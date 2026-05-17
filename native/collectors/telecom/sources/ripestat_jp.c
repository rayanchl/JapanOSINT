/* collectors/telecom/sources/ripestat_jp.c
 * Port of server/src/collectors/ripestatJp.js (intelEnvelope, single item).
 * RIPEstat country-resource-list?resource=JP → 1 intel row, ASN/IPv4/IPv6
 * prefix counts. uid = ripestat-jp|jp-country-resources (fixed). No geometry. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define API_URL "https://stat.ripe.net/data/country-resource-list/data.json?resource=JP"

/* new Date().toISOString() → YYYY-MM-DDTHH:MM:SS.mmmZ (UTC) */
static void iso_now(char *o, size_t n) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm tm;
  gmtime_r(&tv.tv_sec, &tm);
  char base[32];
  strftime(base, sizeof base, "%Y-%m-%dT%H:%M:%S", &tm);
  snprintf(o, n, "%s.%03dZ", base, (int)(tv.tv_usec / 1000));
}

/* Number.prototype.toLocaleString() default (en-US): group integer by 3
 * with commas. n is a non-negative integer count. */
static void loc(long n, char *o, size_t cap) {
  char tmp[32];
  int len = snprintf(tmp, sizeof tmp, "%ld", n);
  int commas = (len - 1) / 3;
  int total = len + commas;
  if ((size_t)total + 1 > cap) { snprintf(o, cap, "%ld", n); return; }
  o[total] = '\0';
  int oi = total - 1, di = len - 1, cnt = 0;
  while (di >= 0) {
    o[oi--] = tmp[di--];
    if (++cnt == 3 && di >= 0) { o[oi--] = ','; cnt = 0; }
  }
}

static int alen(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return cJSON_IsArray(v) ? cJSON_GetArraySize(v) : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *country = feed_get_json_h(ctx->http, API_URL, hdrs, 15000);

  cJSON *data = country ? cJSON_GetObjectItem(country, "data") : NULL;
  cJSON *res = data ? cJSON_GetObjectItem(data, "resources") : NULL;
  int asn = alen(res, "asn");
  int ip4 = alen(res, "ipv4");
  int ip6 = alen(res, "ipv6");
  int live = (country != NULL) && (asn > 0);

  char la[32], l4[32], l6[32];
  loc(asn, la, sizeof la);
  loc(ip4, l4, sizeof l4);
  loc(ip6, l6, sizeof l6);

  char summary[160];
  snprintf(summary, sizeof summary,
    "%s ASNs \xc2\xb7 %s IPv4 prefixes \xc2\xb7 %s IPv6 prefixes",
    la, l4, l6);

  char body[256];
  snprintf(body, sizeof body,
    "ASNs registered to JP: %d\nIPv4 prefixes: %d\nIPv6 prefixes: %d",
    asn, ip4, ip6);

  cJSON *props = cJSON_CreateObject();           /* EXACT JS key order */
  cJSON_AddStringToObject(props, "country", "JP");
  cJSON_AddNumberToObject(props, "asn_count", asn);
  cJSON_AddNumberToObject(props, "ipv4_count", ip4);
  cJSON_AddNumberToObject(props, "ipv6_count", ip6);
  /* err = country?.err || null. On a successful fetch it's null; on failure
   * the JS message is itself nondeterministic — emit generic. */
  if (!country)
    cJSON_AddStringToObject(props, "err", "fetch_failed");
  else
    cJSON_AddItemToObject(props, "err", cJSON_CreateNull());
  cJSON_AddStringToObject(props, "source", "ripestat_country_resource_list");
  char *pj = cJSON_PrintUnformatted(props);

  char tags[96];
  snprintf(tags, sizeof tags,
    "[\"bgp\",\"routing\",\"country-stats\",\"%s\"]",
    live ? "reachable" : "unreachable");

  char pub[40];
  iso_now(pub, sizeof pub);

  intel_item it = {0};
  it.remote_key     = "jp-country-resources";
  it.title          = "Japan country routing resources (RIPEstat)";
  it.body           = body;
  it.summary        = summary;
  it.link           = "https://stat.ripe.net/JP";
  it.lang           = "en";
  it.published_at   = pub;
  it.record_type    = "ripestat-jp";
  it.properties_json = pj;
  it.tags_json      = tags;
  int n = (sink->emit(sink, &it) >= 0) ? 1 : 0;

  free(pj);
  cJSON_Delete(props);
  if (country) cJSON_Delete(country);
  fprintf(stderr, "[ripestat-jp] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def ripestat_jp_def = {
  .id = "ripestat-jp", .collector = "telecom",
  .name = "RIPEstat (JP)", .name_ja = "RIPEstat 日本",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(ripestat_jp_def)
